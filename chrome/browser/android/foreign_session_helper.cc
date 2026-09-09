// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/foreign_session_helper.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "third_party/jni_zero/default_conversions.h"
#include "ui/base/window_open_disposition.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/recent_tabs/jni_headers/ForeignSessionHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using sync_sessions::OpenTabsUIDelegate;
using sync_sessions::SyncedSession;

namespace {

OpenTabsUIDelegate* GetOpenTabsUIDelegate(Profile* profile) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // Only return the delegate if it exists.
  if (!service) {
    return nullptr;
  }

  return service->GetOpenTabsUIDelegate();
}

const sessions::SessionTab* GetForeignSessionTab(OpenTabsUIDelegate* open_tabs,
                                                 const std::string& session_tag,
                                                 int session_tab_id) {
  if (!open_tabs) {
    return nullptr;
  }

  const sessions::SessionTab* session_tab = nullptr;
  if (!open_tabs->GetForeignTab(session_tag,
                                SessionID::FromSerializedValue(session_tab_id),
                                &session_tab)) {
    return nullptr;
  }

  if (ForeignSessionHelper::ShouldSkipTab(*session_tab)) {
    return nullptr;
  }

  return session_tab;
}

static void JNI_ForeignSessionHelper_CopyTabToJava(
    JNIEnv* env,
    const sessions::SessionTab& tab,
    ScopedJavaLocalRef<jobject>& j_window) {
  int selected_index = tab.normalized_navigation_index();
  DCHECK_GE(selected_index, 0);
  DCHECK_LT(selected_index, static_cast<int>(tab.navigations.size()));

  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);

  Java_ForeignSessionHelper_pushTab(
      env, j_window, current_navigation.virtual_url(),
      current_navigation.title(), tab.timestamp.InMillisecondsSinceUnixEpoch(),
      tab.last_active_time.InMillisecondsSinceUnixEpoch(), tab.tab_id.id());
}

static void JNI_ForeignSessionHelper_CopyWindowToJava(
    JNIEnv* env,
    const sessions::SessionWindow& window,
    ScopedJavaLocalRef<jobject>& j_window) {
  for (const auto& tab_ptr : window.tabs) {
    const sessions::SessionTab& session_tab = *(tab_ptr.get());

    if (ForeignSessionHelper::ShouldSkipTab(session_tab)) {
      continue;
    }

    JNI_ForeignSessionHelper_CopyTabToJava(env, session_tab, j_window);
  }
}

static void JNI_ForeignSessionHelper_CopySessionToJava(
    JNIEnv* env,
    const SyncedSession& session,
    ScopedJavaLocalRef<jobject>& j_session) {
  for (const auto& window_pair : session.windows) {
    const sessions::SessionWindow& window = window_pair.second->wrapped_window;

    if (ForeignSessionHelper::ShouldSkipWindow(window)) {
      continue;
    }

    ScopedJavaLocalRef<jobject> last_pushed_window;
    last_pushed_window.Reset(Java_ForeignSessionHelper_pushWindow(
        env, j_session, window.timestamp.InMillisecondsSinceUnixEpoch(),
        window.window_id.id()));

    JNI_ForeignSessionHelper_CopyWindowToJava(env, window, last_pushed_window);
  }
}

}  // namespace

// static
bool ForeignSessionHelper::ShouldSkipTab(
    const sessions::SessionTab& session_tab) {
  if (session_tab.navigations.empty()) {
    return true;
  }

  int selected_index = session_tab.normalized_navigation_index();
  const sessions::SerializedNavigationEntry& current_navigation =
      session_tab.navigations.at(selected_index);

  if (current_navigation.virtual_url().is_empty()) {
    return true;
  }

  if (!SessionSyncServiceFactory::ShouldSyncURL(
          current_navigation.virtual_url())) {
    return true;
  }

  return false;
}

// static
bool ForeignSessionHelper::ShouldSkipWindow(
    const sessions::SessionWindow& window) {
  for (const auto& tab_ptr : window.tabs) {
    const sessions::SessionTab& session_tab = *(tab_ptr.get());
    if (!ShouldSkipTab(session_tab)) {
      return false;
    }
  }
  return true;
}

// static
bool ForeignSessionHelper::ShouldSkipSession(
    const sync_sessions::SyncedSession& session) {
  for (const auto& window_pair : session.windows) {
    const sessions::SessionWindow& window = window_pair.second->wrapped_window;
    if (!ShouldSkipWindow(window)) {
      return false;
    }
  }
  return true;
}

static int64_t JNI_ForeignSessionHelper_Init(Profile* profile) {
  ForeignSessionHelper* foreign_session_helper =
      new ForeignSessionHelper(profile);
  return reinterpret_cast<intptr_t>(foreign_session_helper);
}

ForeignSessionHelper::ForeignSessionHelper(Profile* profile)
    : profile_(profile) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // NOTE: The SessionSyncService can be null in tests.
  if (service) {
    // base::Unretained() is safe below because the subscription itself is a
    // class member field and handles destruction well.
    foreign_session_updated_subscription_ =
        service->SubscribeToForeignSessionsChanged(base::BindRepeating(
            &ForeignSessionHelper::FireForeignSessionCallback,
            base::Unretained(this)));
  }
}

ForeignSessionHelper::~ForeignSessionHelper() = default;

void ForeignSessionHelper::Destroy() {
  delete this;
}

bool ForeignSessionHelper::IsTabSyncEnabled() {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return service && service->GetOpenTabsUIDelegate();
}

void ForeignSessionHelper::TriggerSessionSync() {
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  service->TriggerRefresh(
      syncer::SyncService::TriggerRefreshSource::kForeignSessionHelper,
      {syncer::SESSIONS});
}

void ForeignSessionHelper::SetOnForeignSessionCallback(
    JNIEnv* env,
    const JavaRef<jobject>& callback) {
  callback_.Reset(env, callback);
}

void ForeignSessionHelper::FireForeignSessionCallback() {
  if (callback_.is_null()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_ForeignSessionCallback_onUpdated(env, callback_);
}

bool ForeignSessionHelper::GetForeignSessions(JNIEnv* env,
                                              const JavaRef<jobject>& result) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return false;
  }

  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  if (!open_tabs->GetAllForeignSessions(&sessions)) {
    return false;
  }

  // Use a pref to keep track of sessions that were collapsed by the user.
  // To prevent the pref from accumulating stale sessions, clear it each time
  // and only add back sessions that are still current.
  ScopedDictPrefUpdate pref_update(profile_->GetPrefs(),
                                   prefs::kNtpCollapsedForeignSessions);
  base::DictValue& pref_collapsed_sessions = pref_update.Get();
  base::DictValue collapsed_sessions = pref_collapsed_sessions.Clone();
  pref_collapsed_sessions.clear();

  ScopedJavaLocalRef<jobject> last_pushed_session;

  // Note: we don't own the SyncedSessions themselves.
  for (size_t i = 0; i < sessions.size(); ++i) {
    const SyncedSession& session = *(sessions[i]);
    if (ShouldSkipSession(session)) {
      continue;
    }

    const bool is_collapsed =
        (collapsed_sessions.Find(session.GetSessionTag()) != nullptr);

    if (is_collapsed) {
      pref_collapsed_sessions.Set(session.GetSessionTag(), true);
    }

    last_pushed_session.Reset(Java_ForeignSessionHelper_pushSession(
        env, result, session.GetSessionTag(), session.GetSessionName(),
        session.GetModifiedTime().InMillisecondsSinceUnixEpoch(),
        static_cast<int>(session.GetDeviceFormFactor())));

    // Push the full session, with tabs ordered by visual position.
    JNI_ForeignSessionHelper_CopySessionToJava(env, session,
                                               last_pushed_session);
  }

  return true;
}

bool ForeignSessionHelper::GetMobileAndTabletForeignSessions(
    JNIEnv* env,
    const JavaRef<jobject>& result) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return false;
  }

  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  if (!open_tabs->GetAllForeignSessions(&sessions)) {
    return false;
  }

  ScopedJavaLocalRef<jobject> last_pushed_session;
  size_t skipped_tabs_on_restore = 0;

  // Note: we don't own the SyncedSessions themselves.
  for (const SyncedSession* session : sessions) {
    if (ShouldSkipSession(*session)) {
      continue;
    }

    if (session->GetDeviceFormFactor() ==
            syncer::DeviceInfo::FormFactor::kPhone ||
        session->GetDeviceFormFactor() ==
            syncer::DeviceInfo::FormFactor::kTablet) {
      last_pushed_session.Reset(Java_ForeignSessionHelper_pushSession(
          env, result, session->GetSessionTag(), session->GetSessionName(),
          session->GetModifiedTime().InMillisecondsSinceUnixEpoch(),
          static_cast<int>(session->GetDeviceFormFactor())));

      // Push the full session, with tabs ordered by visual position.
      JNI_ForeignSessionHelper_CopySessionToJava(env, *session,
                                                 last_pushed_session);
    } else {
      skipped_tabs_on_restore++;
    }
  }
  return (skipped_tabs_on_restore != sessions.size());
}

bool ForeignSessionHelper::OpenForeignSessionTab(TabAndroid* tab_android,
                                                 const std::string& session_tag,
                                                 int32_t session_tab_id,
                                                 int32_t j_disposition) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    LOG(ERROR) << "Null OpenTabsUIDelegate returned.";
    return false;
  }

  const sessions::SessionTab* session_tab =
      GetForeignSessionTab(open_tabs, session_tag, session_tab_id);
  if (!session_tab) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return false;
  }

  if (!tab_android) {
    return false;
  }
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents) {
    return false;
  }

  WindowOpenDisposition disposition =
      static_cast<WindowOpenDisposition>(j_disposition);
  SessionRestore::RestoreForeignSessionTab(web_contents, *session_tab,
                                           disposition);

  return true;
}

void ForeignSessionHelper::DeleteForeignSession(
    const std::string& session_tag) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (open_tabs) {
    open_tabs->DeleteForeignSession(session_tag);
  }
}

void ForeignSessionHelper::SetInvalidationsForSessionsEnabled(bool enabled) {
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  service->SetInvalidationsForSessionsEnabled(enabled);
}

int32_t ForeignSessionHelper::OpenForeignSessionTabsAsBackgroundTabs(
    TabAndroid* tab_android,
    const std::vector<int32_t>& session_tab_ids,
    const std::string& session_tag) {
  if (session_tab_ids.empty()) {
    return 0;
  }

  if (!tab_android) {
    return 0;
  }

  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return 0;
  }

  // Open the first valid tab in the list with a renderer and web contents.
  content::WebContents* web_contents = nullptr;
  size_t first_tab_index = 0;
  for (; first_tab_index < session_tab_ids.size(); ++first_tab_index) {
    web_contents = RestoreTabWithRenderer(open_tabs, session_tag, tab_android,
                                          session_tab_ids[first_tab_index]);
    if (web_contents) {
      break;
    }
  }
  if (!web_contents) {
    return 0;
  }
  int num_tabs_restored = 1;

  // Using the web contents of the first tab, load the rest of the tabs
  // as background tabs without a renderer.
  base::WeakPtr<content::WebContents> weak_web_contents =
      web_contents->GetWeakPtr();
  for (size_t i = first_tab_index + 1; i < session_tab_ids.size(); ++i) {
    if (!weak_web_contents) {
      break;
    }
    if (RestoreTabNoRenderer(open_tabs, session_tag, session_tab_ids[i],
                             weak_web_contents.get())) {
      num_tabs_restored++;
    }
  }
  return num_tabs_restored;
}

content::WebContents* ForeignSessionHelper::RestoreTabWithRenderer(
    sync_sessions::OpenTabsUIDelegate* open_tabs,
    const std::string& session_tag,
    TabAndroid* tab_android,
    int session_tab_id) {
  const sessions::SessionTab* foreground_session_tab =
      GetForeignSessionTab(open_tabs, session_tag, session_tab_id);
  if (!foreground_session_tab) {
    return nullptr;
  }

  if (!tab_android) {
    return nullptr;
  }
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents) {
    return nullptr;
  }

  return SessionRestore::RestoreForeignSessionTab(
      web_contents, *foreground_session_tab,
      WindowOpenDisposition::CURRENT_TAB);
}

bool ForeignSessionHelper::RestoreTabNoRenderer(
    sync_sessions::OpenTabsUIDelegate* open_tabs,
    const std::string& session_tag,
    int session_tab_id,
    content::WebContents* web_contents) {
  const sessions::SessionTab* background_session_tab =
      GetForeignSessionTab(open_tabs, session_tag, session_tab_id);
  if (!background_session_tab) {
    return false;
  }

  SessionRestore::RestoreForeignSessionTab(
      web_contents, *background_session_tab,
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      /*skip_renderer_creation=*/true);
  return true;
}

DEFINE_JNI(ForeignSessionHelper)
