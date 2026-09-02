// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/send_tab_to_self/android_notification_handler.h"

#include <optional>
#include <string>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/span.h"
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
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/shared_highlighting/core/common/text_fragment.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/NotificationManager_jni.h"
#include "chrome/android/chrome_jni_headers/SendTabToSelfNotificationReceiver_jni.h"

using base::android::ScopedJavaLocalRef;
using jni_zero::AttachCurrentThread;

namespace send_tab_to_self {

namespace {

bool IsTabModelViable(TabModel* tab_model) {
  return !tab_model->IsOffTheRecord() &&
         tab_model->GetTabModelType() == TabModel::TabModelType::kStandard;
}

// Helper function to find the active standard TabModel's WebContents if Chrome
// is in the foreground (has visible activities).
// Returns nullptr if Chrome is in the background.
//
// Note: The WebContents is not required to be visible (e.g. it is allowed to be
// hidden during cold start or app foreground transitions, or when the tab
// switcher is open) as long as Chrome is in the foreground, to prevent timing
// races and support auto-opening in the tab switcher.
content::WebContents* GetActiveWebContents(bool require_visible) {
  if (base::FeatureList::IsEnabled(kSendTabToSelfSupportAutoOpenInTabGrid)) {
    // Only auto-open if Chrome is in the foreground (has visible activities)
    // to avoid resource waste and ensure the user gets a notification if they
    // are using another app.
    if (!base::android::ApplicationStatusListener::HasVisibleActivities()) {
      return nullptr;
    }
  }

  // Track candidate WebContents across decreasing priority tiers:
  // Priority 1: Active tab of the active TabModel (returned immediately).
  // Priority 2: Active tab of an inactive TabModel.
  // Priority 3: First tab of the active TabModel (fallback for delayed tab
  // initialization).
  // Priority 4: First tab of an inactive TabModel (last fallback).
  content::WebContents* inactive_model_active_tab = nullptr;
  content::WebContents* active_model_fallback_tab = nullptr;
  content::WebContents* inactive_model_fallback_tab = nullptr;

  for (TabModel* model : TabModelList::models()) {
    if (!IsTabModelViable(model) || model->GetTabCount() == 0) {
      continue;
    }

    // Check if the model has a viable active tab.
    content::WebContents* active_tab = model->GetActiveWebContents();
    if (active_tab && (!require_visible || active_tab->GetVisibility() ==
                                               content::Visibility::VISIBLE)) {
      if (model->IsActiveModel()) {
        // Priority 1: Best target found. Return immediately.
        return active_tab;
      }
      if (!inactive_model_active_tab) {
        // Priority 2 candidate.
        inactive_model_active_tab = active_tab;
      }
      continue;
    }

    // Fallback for delayed tab initialization where tabs are added to the model
    // before active tab selection is updated.
    content::WebContents* fallback_tab = model->GetWebContentsAt(0);
    if (!fallback_tab ||
        (require_visible &&
         fallback_tab->GetVisibility() != content::Visibility::VISIBLE)) {
      continue;
    }

    if (model->IsActiveModel()) {
      if (!active_model_fallback_tab) {
        // Priority 3 candidate.
        active_model_fallback_tab = fallback_tab;
      }
    } else if (!inactive_model_fallback_tab) {
      // Priority 4 candidate.
      inactive_model_fallback_tab = fallback_tab;
    }
  }

  // Return the highest-priority candidate found among lower tiers.
  if (inactive_model_active_tab) {
    return inactive_model_active_tab;
  }
  if (active_model_fallback_tab) {
    return active_model_fallback_tab;
  }
  return inactive_model_fallback_tab;
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

  if (base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)) {
    if (send_tab_to_self_model_->IsReady()) {
      OnModelReady();
    } else {
      model_observation_.Observe(send_tab_to_self_model_);
    }
  }
}

AndroidNotificationHandler::~AndroidNotificationHandler() {
  TabModelList::RemoveObserver(this);
}

void AndroidNotificationHandler::DisplayNewEntries(
    base::span<const SendTabToSelfEntry* const> new_entries) {
  if (new_entries.empty()) {
    return;
  }

  // Called when new entries are received from sync.
  content::WebContents* const target_web_contents =
      base::FeatureList::IsEnabled(kSendTabToSelfAutoOpen)
          ? GetActiveWebContents(
                /*require_visible=*/!base::FeatureList::IsEnabled(
                    kSendTabToSelfSupportAutoOpenInTabGrid))
          : nullptr;

  if (target_web_contents) {
    // If there is a target tab (i.e. Chrome is active / in the foreground),
    // open the entries in background tabs.
    OpenEntriesInBackground(
        new_entries, *target_web_contents,
        AutoOpenOutcome::kTabsOpenedImmediatelyInBackground);
  } else {
    // Chrome is *not* in the foreground, so show notifications for the entries.
    for (const SendTabToSelfEntry* entry : new_entries) {
      ShowNotification(*entry);
      // TODO(crbug.com/488072250): Record this only if kSendTabToSelfAutoOpen
      // is enabled.
      RecordAutoOpenOutcome(AutoOpenOutcome::kUnopenedImmediately);
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
      GetScrollPositionAsTextFragment(&entry);

  std::vector<uint8_t> page_context_bytes;
  if (base::FeatureList::IsEnabled(kSendTabToSelfPropagateFormFields) &&
      !entry.GetPageContext().form_field_info.fields.empty()) {
    std::string serialized_page_context =
        PageContextToProto(entry.GetPageContext()).SerializeAsString();
    page_context_bytes.assign(serialized_page_context.begin(),
                              serialized_page_context.end());
  }

  Java_NotificationManager_showNotification(
      env, entry.GetGUID(), entry.GetURL().spec(), entry.GetTitle(),
      entry.GetDeviceName(), expiration_time.InMillisecondsSinceUnixEpoch(),
      send_tab_to_self_notification_receiver_class,
      internal_scroll_to_text_fragment, page_context_bytes);
}

void AndroidNotificationHandler::HideNotification(const std::string& guid) {
  JNIEnv* env = AttachCurrentThread();
  Java_NotificationManager_hideNotification(env, guid);
}

void AndroidNotificationHandler::DismissEntries(
    base::span<const std::string> guids) {
  // Hides system notifications for the specified GUIDs (e.g., after they have
  // been successfully opened or deleted remotely).
  for (const std::string& guid : guids) {
    HideNotification(guid);
  }
}

void AndroidNotificationHandler::OnModelReady() {
  CheckAndOpenPendingEntries();
  model_observation_.Reset();
}

void AndroidNotificationHandler::OnTabModelAdded(TabModel* tab_model) {
  // When a regular browser window is created (e.g., during cold start), check
  // for and open any pending tab notifications.
  if (!IsTabModelViable(tab_model)) {
    return;
  }

  if (tab_model->GetTabCount() > 0) {
    CheckAndOpenPendingEntries();
  } else {
    // If the model is empty (e.g. during delayed initialization), observe it
    // to trigger auto-open when the first tab is added.
    tab_model_observations_.AddObservation(tab_model);
  }
}

void AndroidNotificationHandler::OnTabModelRemoved(TabModel* tab_model) {
  if (tab_model_observations_.IsObservingSource(tab_model)) {
    tab_model_observations_.RemoveObservation(tab_model);
  }
}

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

  OpenEntriesInBackground(
      send_tab_to_self_model_->GetUnopenedEntriesTargetedToLocalDevice(),
      *target_web_contents,
      AutoOpenOutcome::kTabsOpenedInBackgroundUponActivation);
}

void AndroidNotificationHandler::OpenEntriesInBackground(
    base::span<const SendTabToSelfEntry* const> entries,
    content::WebContents& target_web_contents,
    AutoOpenOutcome outcome) {
  int next_tabstrip_index = TabModel::kInvalidIndex;
  // Insert tabs after the active tab, if available, preserving chronological
  // order when opening multiple tabs simultaneously.
  const TabModel* model =
      TabModelList::GetTabModelForWebContents(&target_web_contents);
  if (model) {
    const int active_index = model->GetActiveIndex();
    if (active_index != TabModel::kInvalidIndex) {
      next_tabstrip_index = active_index + 1;
    }
  }

  std::string_view last_device_name;

  for (const SendTabToSelfEntry* entry : entries) {
    OpenEntryInBackground(*entry, target_web_contents, next_tabstrip_index);
    RecordAutoOpenOutcome(outcome);

    if (next_tabstrip_index != TabModel::kInvalidIndex) {
      ++next_tabstrip_index;
    }

    // Dismiss any system notification associated with this entry.
    HideNotification(entry->GetGUID());

    last_device_name = entry->GetDeviceName();
  }

  // Display an in-app banner for the most recent sender device if at least
  // one tab was opened.
  if (!last_device_name.empty()) {
    CHECK(!entries.empty());
    ShowMessageBanner(last_device_name, entries.size(), &target_web_contents,
                      entries.front()->GetURL());
  }
}

void AndroidNotificationHandler::OpenEntryInBackground(
    const SendTabToSelfEntry& entry,
    content::WebContents& target_web_contents,
    int tabstrip_index) {
  send_tab_to_self_model_->MarkEntryOpened(entry.GetGUID());

  auto nav_params = std::make_unique<NavigateParams>(
      Profile::FromBrowserContext(target_web_contents.GetBrowserContext()),
      entry.GetURL(), ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  nav_params->source_contents = &target_web_contents;
  nav_params->disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  nav_params->window_action = NavigateParams::WindowAction::kNoAction;
  nav_params->tabstrip_index = tabstrip_index;
  nav_params->internal_scroll_to_text_fragment =
      GetScrollPositionAsTextFragment(&entry);

  // Keep a raw pointer to the NavigateParams aside since the unique_ptr will
  // be moved into the Navigate() call.
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
    RecordHasScrollPositionOnOpened(!page_context.scroll_position.IsEmpty());

    // Attach a visual label indicating the sender device name to the newly
    // opened background tab.
    if (TabAndroid* tab = TabAndroid::FromWebContents(new_contents)) {
      send_tab_to_self::AttachTabLabel(tab, guid, device_name);
    }
  }
}

void AndroidNotificationHandler::ShowMessageBanner(
    std::string_view device_name,
    int opened_tab_count,
    content::WebContents* web_contents,
    const GURL& opened_tab_url) {
  send_tab_to_self::ShowMessageBanner(web_contents, device_name,
                                      opened_tab_count, opened_tab_url);
}

void AndroidNotificationHandler::DidAddTab(TabAndroid* tab,
                                           TabModel::TabLaunchType type) {
  // Stop observing immediately to avoid recursive DidAddTab calls if
  // CheckAndOpenPendingEntries() opens new background tabs synchronously.
  if (TabModel* model = TabModelList::GetTabModelForTabAndroid(tab)) {
    if (tab_model_observations_.IsObservingSource(model)) {
      tab_model_observations_.RemoveObservation(model);
    }
  }
  CheckAndOpenPendingEntries();
}

}  // namespace send_tab_to_self

DEFINE_JNI(NotificationManager)
DEFINE_JNI(SendTabToSelfNotificationReceiver)
