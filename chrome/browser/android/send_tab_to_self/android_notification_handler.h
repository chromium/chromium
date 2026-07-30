// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/android/application_status_listener.h"
#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list_observer.h"
#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/receiving_ui_handler.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

struct NavigateParams;

namespace send_tab_to_self {

struct PageContext;
class SendTabToSelfEntry;
class SendTabToSelfModel;

// Responsible for displaying notifications on Android. Overrides
// ReceivingUIHandler so that it is called for all updates to share
// entries.
class AndroidNotificationHandler : public ReceivingUiHandler,
                                   public TabModelListObserver,
                                   public SendTabToSelfModelObserver,
                                   public TabModelObserver {
 public:
  explicit AndroidNotificationHandler(
      SendTabToSelfModel* send_tab_to_self_model);
  ~AndroidNotificationHandler() override;

  // ReceivingUiHandler implementation.
  void DisplayNewEntries(
      base::span<const SendTabToSelfEntry* const> new_entries) override;
  void DismissEntries(base::span<const std::string> guids) override;

 protected:
  // Overridden in tests to mock actual Android JNI notification calls.
  virtual void ShowNotification(const SendTabToSelfEntry& entry);
  virtual void HideNotification(const std::string& guid);
  virtual void ShowMessageBanner(std::string_view device_name,
                                 int opened_tab_count,
                                 content::WebContents* web_contents);
  virtual bool OpenInNativeAppIfPossible(const GURL& url);

 private:
  // SendTabToSelfModelObserver implementation.
  void OnModelReady() override;

  // TabModelListObserver:
  void OnTabModelAdded(TabModel* tab_model) override;
  void OnTabModelRemoved(TabModel* tab_model) override;

  // TabModelObserver:
  void DidAddTab(TabAndroid* tab, TabModel::TabLaunchType type) override;

  void OnNavigationStarted(
      const std::string& guid,
      const GURL& url,
      const std::string& device_name,
      const PageContext& page_context,
      std::unique_ptr<NavigateParams> nav_params,
      base::WeakPtr<content::NavigationHandle> navigation_handle);

  // Handles application state transitions (e.g., Chrome coming to foreground).
  void HandleApplicationStateChange(base::android::ApplicationState state);

  // Automatically opens any unread synced entries as background tabs and
  // dismisses their system notifications if an active standard window is
  // available.
  void CheckAndOpenPendingEntries();

  enum class AutoOpenTrigger {
    kImmediate,
    kOnActivation,
  };

  void OpenEntries(base::span<const SendTabToSelfEntry* const> entries,
                   content::WebContents* target_web_contents,
                   AutoOpenTrigger trigger);

  enum class OpenResult {
    kOpenedInTab,
    kOpenedInNativeApp,
  };

  static AutoOpenOutcome GetAutoOpenOutcome(AutoOpenTrigger trigger,
                                            OpenResult open_result);

  // Opens the given entry in the context of `target_web_contents` (either in a
  // matching native app if available, or as a new background tab) and marks the
  // entry as opened.
  OpenResult OpenEntry(const SendTabToSelfEntry& entry,
                       content::WebContents& target_web_contents,
                       int tabstrip_index);

  const raw_ptr<SendTabToSelfModel> send_tab_to_self_model_;

  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      model_observation_{this};

  base::ScopedMultiSourceObservation<TabModel, TabModelObserver>
      tab_model_observations_{this};

  std::unique_ptr<base::android::ApplicationStatusListener>
      app_status_listener_;

  base::WeakPtrFactory<AndroidNotificationHandler> weak_factory_{this};
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_ANDROID_SEND_TAB_TO_SELF_ANDROID_NOTIFICATION_HANDLER_H_
