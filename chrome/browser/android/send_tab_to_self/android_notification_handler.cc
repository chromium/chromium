// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/android/send_tab_to_self/send_tab_to_self_android_bridge.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NotificationManager_jni.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfNotificationReceiver_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace send_tab_to_self {

namespace {

std::optional<std::string> GetScrollToTextFragmentFromEntry(
    const SendTabToSelfEntry& entry) {
  if (!base::FeatureList::IsEnabled(kSendTabToSelfPropagateScrollPosition) ||
      entry.GetPageContext().scroll_position.IsEmpty()) {
    return std::nullopt;
  }

  shared_highlighting::TextFragment tf =
      entry.GetPageContext()
          .scroll_position.text_fragment.ToSharedHighlightingTextFragment();

  return tf.ToEscapedString(shared_highlighting::TextFragment::
                                EscapedStringFormat::kWithoutTextDirective);
}

bool IsTabModelViable(TabModel* tab_model) {
  return !tab_model->IsOffTheRecord() &&
         tab_model->GetTabModelType() == TabModel::TabModelType::kStandard;
}

// Helper function to find the active standard TabModel's WebContents.
// If `require_visible` is true, only returns the WebContents if it is visible.
//
// Note: `require_visible` is set to false during cold start window creation
// (`OnTabModelAdded`) or app foreground transitions
// (`HandleApplicationStateChange`). This is because during these quick
// lifecycle transitions, the WebContents is active and attached, but the
// compositor might not have drawn the first frame yet (so `GetVisibility()`
// still temporarily returns `HIDDEN` or `OCCLUDED`). Requiring visibility in
// these scenarios would cause auto-opening to fail due to timing races. During
// real-time push sync delivery (`DisplayNewEntriesOnUIThread`), however,
// `require_visible` is set to true to ensure entries only auto-open if actively
// browsing.
content::WebContents* GetActiveWebContents(bool require_visible) {
  for (TabModel* model : TabModelList::models()) {
    // Exclude OTR models and non-standard models (e.g., kEmpty, kArchived).
    // kEmpty represents stub models which never have an active WebContents.
    if (!IsTabModelViable(model)) {
      continue;
    }
    content::WebContents* wc = model->GetActiveWebContents();
    if (!wc) {
      continue;
    }
    if (require_visible &&
        wc->GetVisibility() != content::Visibility::VISIBLE) {
      continue;
    }
    return wc;
  }
  return nullptr;
}

}  // namespace

AndroidNotificationHandler::AndroidNotificationHandler(
    SendTabToSelfModel* send_tab_to_self_model)
    : send_tab_to_self_model_(send_tab_to_self_model) {
  // Observe TabModelList to guarantee that CheckAndOpenPendingEntries runs
  // during cold start as soon as the initial browser window tab model is
  // attached.
  TabModelList::AddObserver(this);

  // Observe ApplicationStatusListener to guarantee that
  // CheckAndOpenPendingEntries runs during warm start or when a notification
  // tap brings Chrome from the background to the foreground.
  app_status_listener_ =
      base::android::ApplicationStatusListener::New(base::BindRepeating(
          &AndroidNotificationHandler::HandleApplicationStateChange,
          weak_factory_.GetWeakPtr()));

  CheckAndOpenPendingEntries();
}

AndroidNotificationHandler::~AndroidNotificationHandler() {
  TabModelList::RemoveObserver(this);
}

void AndroidNotificationHandler::DisplayNewEntries(
    base::span<const SendTabToSelfEntry* const> new_entries) {
  if (new_entries.empty()) {
    return;
  }

  std::vector<SendTabToSelfEntry> vector_copy;

  for (const SendTabToSelfEntry* entry : new_entries) {
    vector_copy.push_back(*entry);
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AndroidNotificationHandler::DisplayNewEntriesOnUIThread,
                     weak_factory_.GetWeakPtr(), std::move(vector_copy)));
}

void AndroidNotificationHandler::DisplayNewEntriesOnUIThread(
    const std::vector<SendTabToSelfEntry>& new_entries) {
  // Called when new entries are received from sync.
  content::WebContents* const target_web_contents =
      base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)
          ? GetActiveWebContents(/*require_visible=*/true)
          : nullptr;

  // If Chrome is already open and active in the foreground, entries are
  // opened directly as new background tabs.
  if (target_web_contents) {
    for (const SendTabToSelfEntry& entry : new_entries) {
      OpenEntryInBackgroundTab(entry, *target_web_contents);
    }
    ShowMessageBanner(new_entries.back().GetDeviceName(), target_web_contents);
  } else {
    // Otherwise, show a standard system notification.
    for (const SendTabToSelfEntry& entry : new_entries) {
      ShowNotification(entry);
    }
  }
}

void AndroidNotificationHandler::ShowNotification(
    const SendTabToSelfEntry& entry) {
  JNIEnv* env = AttachCurrentThread();

  // Set the expiration to 10 days from when the notification is displayed.
  base::Time expiration_time = entry.GetSharedTime() + base::Days(10);

  ScopedJavaLocalRef<jclass> send_tab_to_self_notification_receiver_class =
      Java_SendTabToSelfNotificationReceiver_getSendTabToSelfNotificationReciever(
          env);

  std::optional<std::string> internal_scroll_to_text_fragment =
      GetScrollToTextFragmentFromEntry(entry);

  Java_NotificationManager_showNotification(
      env, ConvertUTF8ToJavaString(env, entry.GetGUID()),
      ConvertUTF8ToJavaString(env, entry.GetURL().spec()),
      ConvertUTF8ToJavaString(env, entry.GetTitle()),
      ConvertUTF8ToJavaString(env, entry.GetDeviceName()),
      expiration_time.InMillisecondsSinceUnixEpoch(),
      send_tab_to_self_notification_receiver_class,
      internal_scroll_to_text_fragment
          ? ConvertUTF8ToJavaString(env, *internal_scroll_to_text_fragment)
          : nullptr);
}

void AndroidNotificationHandler::HideNotification(const std::string& guid) {
  JNIEnv* env = AttachCurrentThread();
  Java_NotificationManager_hideNotification(env,
                                            ConvertUTF8ToJavaString(env, guid));
}

void AndroidNotificationHandler::DismissEntries(
    base::span<const std::string> guids) {
  // Hides system notifications for the specified GUIDs (e.g., after they have
  // been successfully opened or deleted remotely).
  for (const std::string& guid : guids) {
    HideNotification(guid);
  }
}

void AndroidNotificationHandler::OnTabModelAdded(TabModel* tab_model) {
  // When a regular browser window is created (e.g., during cold start), check
  // for and open any pending tab notifications.
  if (IsTabModelViable(tab_model)) {
    CheckAndOpenPendingEntries();
  }
}

void AndroidNotificationHandler::OnTabModelRemoved(TabModel* tab_model) {}

void AndroidNotificationHandler::HandleApplicationStateChange(
    base::android::ApplicationState state) {
  // When Chrome transitions from background to foreground (warm start or via a
  // notification click), check for and open any pending background tabs.
  if (state == base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    CheckAndOpenPendingEntries();
  }
}

void AndroidNotificationHandler::CheckAndOpenPendingEntries() {
  // Checks if there are any entries that have not been opened yet.
  if (!base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    return;
  }

  // If an active browser window WebContents is available, auto-opens all unread
  // entries as new background tabs and dismisses their system notifications.
  content::WebContents* const target_web_contents =
      GetActiveWebContents(/*require_visible=*/false);
  if (!target_web_contents) {
    return;
  }

  const std::vector<const SendTabToSelfEntry*> pending_entries =
      send_tab_to_self_model_->GetUnopenedEntriesTargetedToLocalDevice();

  for (const SendTabToSelfEntry* entry : pending_entries) {
    OpenEntryInBackgroundTab(*entry, *target_web_contents);
    HideNotification(entry->GetGUID());
  }

  if (!pending_entries.empty()) {
    ShowMessageBanner(pending_entries.back()->GetDeviceName(),
                      target_web_contents);
  }
}

void AndroidNotificationHandler::OpenEntryInBackgroundTab(
    const SendTabToSelfEntry& entry,
    content::WebContents& target_web_contents) {
  auto nav_params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(target_web_contents.GetBrowserContext()),
      entry.GetURL(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_params->source_contents = &target_web_contents;
  nav_params->disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  nav_params->window_action = NavigateParams::WindowAction::kNoAction;
  nav_params->internal_scroll_to_text_fragment =
      GetScrollToTextFragmentFromEntry(entry);

  NavigateParams* nav_params_ptr = nav_params.get();
  Navigate(nav_params_ptr,
           base::BindOnce(&AndroidNotificationHandler::OnNavigationStarted,
                          weak_factory_.GetWeakPtr(), entry.GetGUID(),
                          entry.GetURL(), entry.GetDeviceName(),
                          entry.GetPageContext(), std::move(nav_params)));
}

void AndroidNotificationHandler::OnNavigationStarted(
    const std::string& guid,
    const GURL& url,
    const std::string& device_name,
    const PageContext& page_context,
    std::unique_ptr<NavigateParams> nav_params,
    base::WeakPtr<content::NavigationHandle> navigation_handle) {
  CHECK(base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen));
  if (content::WebContents* new_contents =
          nav_params->navigated_or_inserted_contents) {
    if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields)) {
      FillWebContents(new_contents, url::Origin::Create(url), page_context);
    }

    // Attach a visual label indicating the sender device name to the newly
    // opened background tab.
    if (TabAndroid* tab = TabAndroid::FromWebContents(new_contents)) {
      send_tab_to_self::AttachTabLabel(tab, guid, device_name);
    }
  }

  send_tab_to_self_model_->MarkEntryOpened(guid);
}

void AndroidNotificationHandler::ShowMessageBanner(
    std::string_view device_name,
    content::WebContents* web_contents) {
  send_tab_to_self::ShowMessageBanner(web_contents, device_name);
}

}  // namespace send_tab_to_self

DEFINE_JNI(NotificationManager)
DEFINE_JNI(SendTabToSelfNotificationReceiver)
