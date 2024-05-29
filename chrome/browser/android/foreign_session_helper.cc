// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/foreign_session_helper.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/service/sync_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/recent_tabs/jni_headers/ForeignSessionHelper_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
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

bool ShouldSkipTab(const sessions::SessionTab& session_tab) {
  if (session_tab.navigations.empty()) {
    return true;
  }

  int selected_index = session_tab.normalized_navigation_index();
  const sessions::SerializedNavigationEntry& current_navigation =
      session_tab.navigations.at(selected_index);

  if (current_navigation.virtual_url().is_empty()) {
    return true;
  }

  return false;
}

bool ShouldSkipWindow(const sessions::SessionWindow& window) {
  for (const auto& tab_ptr : window.tabs) {
    const sessions::SessionTab& session_tab = *(tab_ptr.get());
    if (!ShouldSkipTab(session_tab)) {
      return false;
    }
  }
  return true;
}

bool ShouldSkipSession(const SyncedSession& session) {
  for (const auto& window_pair : session.windows) {
    const sessions::SessionWindow& window = window_pair.second->wrapped_window;
    if (!ShouldSkipWindow(window)) {
      return false;
    }
  }
  return true;
}

void JNI_ForeignSessionHelper_CopyTabToJava(
    JNIEnv* env,
    const sessions::SessionTab& tab,
    ScopedJavaLocalRef<jobject>& j_window) {
  int selected_index = tab.normalized_navigation_index();
  DCHECK_GE(selected_index, 0);
  DCHECK_LT(selected_index, static_cast<int>(tab.navigations.size()));

  const sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);

  GURL tab_url = current_navigation.virtual_url();

  Java_ForeignSessionHelper_pushTab(
      env, j_window, url::GURLAndroid::FromNativeGURL(env, tab_url),
      ConvertUTF16ToJavaString(env, current_navigation.title()),
      tab.timestamp.InMillisecondsSinceUnixEpoch(),
      tab.last_active_time.InMillisecondsSinceUnixEpoch(), tab.tab_id.id());
}

void JNI_ForeignSessionHelper_CopyWindowToJava(
    JNIEnv* env,
    const sessions::SessionWindow& window,
    ScopedJavaLocalRef<jobject>& j_window) {
  for (const auto& tab_ptr : window.tabs) {
    const sessions::SessionTab& session_tab = *(tab_ptr.get());

    if (ShouldSkipTab(session_tab)) {
      return;
    }

    JNI_ForeignSessionHelper_CopyTabToJava(env, session_tab, j_window);
  }
}

void JNI_ForeignSessionHelper_CopySessionToJava(
    JNIEnv* env,
    const SyncedSession& session,
    ScopedJavaLocalRef<jobject>& j_session) {
  for (const auto& window_pair : session.windows) {
    const sessions::SessionWindow& window = window_pair.second->wrapped_window;

    if (ShouldSkipWindow(window)) {
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

static jlong JNI_ForeignSessionHelper_Init(JNIEnv* env, Profile* profile) {
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

void ForeignSessionHelper::Destroy(JNIEnv* env) {
  delete this;
}

jboolean ForeignSessionHelper::IsTabSyncEnabled(JNIEnv* env) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  return service && service->GetOpenTabsUIDelegate();
}

void ForeignSessionHelper::TriggerSessionSync(JNIEnv* env) {
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  service->TriggerRefresh({syncer::SESSIONS});
}

void ForeignSessionHelper::SetOnForeignSessionCallback(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  callback_.Reset(env, callback);
}

void ForeignSessionHelper::FireForeignSessionCallback() {
  if (callback_.is_null()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  Java_ForeignSessionCallback_onUpdated(env, callback_);
}

jboolean ForeignSessionHelper::GetForeignSessions(
    JNIEnv* env,
    const JavaParamRef<jobject>& result) {
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
  base::Value::Dict& pref_collapsed_sessions = pref_update.Get();
  base::Value::Dict collapsed_sessions = pref_collapsed_sessions.Clone();
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
        env, result, ConvertUTF8ToJavaString(env, session.GetSessionTag()),
        ConvertUTF8ToJavaString(env, session.GetSessionName()),
        session.GetModifiedTime().InMillisecondsSinceUnixEpoch(),
        static_cast<int>(session.GetDeviceFormFactor())));

    // Push the full session, with tabs ordered by visual position.
    JNI_ForeignSessionHelper_CopySessionToJava(env, session,
                                               last_pushed_session);
  }

  return true;
}

jboolean ForeignSessionHelper::GetMobileAndTabletForeignSessions(
    JNIEnv* env,
    const JavaParamRef<jobject>& result) {
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
    if (session->GetDeviceFormFactor() ==
            syncer::DeviceInfo::FormFactor::kPhone ||
        session->GetDeviceFormFactor() ==
            syncer::DeviceInfo::FormFactor::kTablet) {
      last_pushed_session.Reset(Java_ForeignSessionHelper_pushSession(
          env, result, ConvertUTF8ToJavaString(env, session->GetSessionTag()),
          ConvertUTF8ToJavaString(env, session->GetSessionName()),
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

jboolean ForeignSessionHelper::OpenForeignSessionTab(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab,
    const JavaParamRef<jstring>& session_tag,
    jint session_tab_id,
    jint j_disposition) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    LOG(ERROR) << "Null OpenTabsUIDelegate returned.";
    return false;
  }

  const sessions::SessionTab* session_tab;

  if (!open_tabs->GetForeignTab(ConvertJavaStringToUTF8(env, session_tag),
                                SessionID::FromSerializedValue(session_tab_id),
                                &session_tab)) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return false;
  }

  if (session_tab->navigations.empty()) {
    LOG(ERROR) << "Foreign tab no longer has valid navigations.";
    return false;
  }

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab_android)
    return false;
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
    JNIEnv* env,
    const JavaParamRef<jstring>& session_tag) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (open_tabs) {
    open_tabs->DeleteForeignSession(ConvertJavaStringToUTF8(env, session_tag));
  }
}

void ForeignSessionHelper::SetInvalidationsForSessionsEnabled(
    JNIEnv* env,
    jboolean enabled) {
  syncer::SyncService* service = SyncServiceFactory::GetForProfile(profile_);
  if (!service) {
    return;
  }

  service->SetInvalidationsForSessionsEnabled(enabled);
}

jint ForeignSessionHelper::OpenForeignSessionTabsAsBackgroundTabs(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab,
    const JavaParamRef<jintArray>& j_session_tab_ids,
    const JavaParamRef<jstring>& session_tag) {
  std::vector<int> session_tab_ids;
  base::android::JavaIntArrayToIntVector(env, j_session_tab_ids,
                                         &session_tab_ids);
  int tabs_android_count = env->GetArrayLength(j_session_tab_ids);

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab_android) {
    return 0;
  }

  // Open the first tab in the list with a renderer and web contents.
  if (!ForeignSessionHelper::RestoreTabWithRenderer(session_tag, j_tab,
                                                    session_tab_ids[0])) {
    return 0;
  }
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents) {
    return 0;
  }
  int num_tabs_restored = 1;

  // Using the web contents of the first tab, load the rest of the tabs
  // with no renderer and as background tabs.
  for (int i = 1; i < tabs_android_count; i++) {
    if (ForeignSessionHelper::RestoreTabNoRenderer(
            session_tag, session_tab_ids[i], web_contents)) {
      num_tabs_restored++;
    }
  }
  return num_tabs_restored;
}

bool ForeignSessionHelper::RestoreTabWithRenderer(
    const JavaParamRef<jstring>& session_tag,
    const JavaParamRef<jobject>& j_tab,
    int session_tab_id) {
  JNIEnv* env = base::android::AttachCurrentThread();
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return false;
  }

  const sessions::SessionTab* foreground_session_tab;

  if (!open_tabs->GetForeignTab(ConvertJavaStringToUTF8(env, session_tag),
                                SessionID::FromSerializedValue(session_tab_id),
                                &foreground_session_tab)) {
    return false;
  }

  if (foreground_session_tab->navigations.empty()) {
    return false;
  }

  TabAndroid* tab_android = TabAndroid::GetNativeTab(env, j_tab);
  if (!tab_android) {
    return false;
  }
  content::WebContents* web_contents = tab_android->web_contents();
  if (!web_contents) {
    return false;
  }

  SessionRestore::RestoreForeignSessionTab(web_contents,
                                           *foreground_session_tab,
                                           WindowOpenDisposition::CURRENT_TAB);
  return true;
}

bool ForeignSessionHelper::RestoreTabNoRenderer(
    const JavaParamRef<jstring>& session_tag,
    int session_tab_id,
    content::WebContents* web_contents) {
  OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return false;
  }

  const sessions::SessionTab* background_session_tab;

  if (!open_tabs->GetForeignTab(
          ConvertJavaStringToUTF8(base::android::AttachCurrentThread(),
                                  session_tag),
          SessionID::FromSerializedValue(session_tab_id),
          &background_session_tab)) {
    return false;
  }

  if (background_session_tab->navigations.empty()) {
    return false;
  }

  SessionRestore::RestoreForeignSessionTab(
      web_contents, *background_session_tab,
      WindowOpenDisposition::NEW_BACKGROUND_TAB, true);
  return true;
}
