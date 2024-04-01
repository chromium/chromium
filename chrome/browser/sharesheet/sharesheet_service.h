// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/sharesheet/share_action/share_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace apps {
struct IntentLaunchInfo;
}  // namespace apps

namespace views {
class View;
}

namespace content {
class WebContents;
}

namespace gfx {
struct VectorIcon;
}

namespace sharesheet {

class SharesheetServiceDelegator;
class SharesheetUiDelegate;

// The SharesheetService is the root service that provides a sharesheet for
// Chrome desktop.
class SharesheetService : public KeyedService {
 public:
  using GetNativeWindowCallback = base::OnceCallback<gfx::NativeWindow()>;

  explicit SharesheetService(Profile* profile);
  ~SharesheetService() override;

  SharesheetService(const SharesheetService&) = delete;
  SharesheetService& operator=(const SharesheetService&) = delete;

  // Displays the dialog (aka bubble) for sharing content (or files) with
  // other applications and targets. `intent` contains the list of the
  // files/content to be shared. If the files to share contains Google
  // Drive hosted document, only drive share action will be shown.
  //
  // `delivered_callback` is run to signify that the intent has been
  // delivered to the target selected by the user (which may then show its own
  // separate UI, e.g. for Nearby Sharing). `delivered_callback` must be
  // non-null.
  // `close_callback` is run to signify that the share flow has finished and the
  // dialog has closed (this includes separate UI, e.g. Nearby Sharing).
  void ShowBubble(content::WebContents* web_contents,
                  apps::IntentPtr intent,
                  LaunchSource source,
                  DeliveredCallback delivered_callback,
                  CloseCallback close_callback = base::NullCallback());
  void ShowBubble(apps::IntentPtr intent,
                  LaunchSource source,
                  GetNativeWindowCallback get_native_window_callback,
                  DeliveredCallback delivered_callback,
                  CloseCallback close_callback = base::NullCallback());

  // Gets the sharesheet controller for the given |native_window|.
  SharesheetController* GetSharesheetController(
      gfx::NativeWindow native_window);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skips the generic Sharesheet bubble and directly displays the
  // NearbyShare bubble dialog for ARC.
  void ShowNearbyShareBubbleForArc(gfx::NativeWindow native_window,
                                   apps::IntentPtr intent,
                                   LaunchSource source,
                                   DeliveredCallback delivered_callback,
                                   CloseCallback close_callback,
                                   ActionCleanupCallback cleanup_callback);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  // |share_action_type| is set to null when testing, but should otherwise have
  // a valid value.
  void OnBubbleClosed(gfx::NativeWindow native_window,
                      const std::optional<ShareActionType>& share_action_type);

  // OnTargetSelected is called by both apps and share actions.
  // If |type| is kAction, expect |share_action_type| to have a valid
  // ShareActionType. If |type| is kArcApp or kWebApp, expect |app_name|
  // to contain a valid app name.
  void OnTargetSelected(gfx::NativeWindow native_window,
                        const TargetType type,
                        const std::optional<ShareActionType>& share_action_type,
                        const std::optional<std::u16string>& app_name,
                        apps::IntentPtr intent,
                        views::View* share_action_view);

  // Only share actions, which have a |share_action_type|, call this function.
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator,
                            const ShareActionType share_action_type);

  // If the files to share contains a Google Drive hosted document, only the
  // drive share action will be shown.
  bool HasShareTargets(const apps::IntentPtr& intent);

  Profile* GetProfile();

  // Only share actions, which have a |share_action_type|, are expected to have
  // a vector icon. Return nullptr if |share_action_type| is null.
  const gfx::VectorIcon* GetVectorIcon(
      const std::optional<ShareActionType>& share_action_type);

  // ==========================================================================
  // ========================== Testing APIs ==================================
  // ==========================================================================
  void ShowBubbleForTesting(gfx::NativeWindow native_window,
                            apps::IntentPtr intent,
                            LaunchSource source,
                            DeliveredCallback delivered_callback,
                            CloseCallback close_callback,
                            int num_actions_to_add);
  SharesheetUiDelegate* GetUiDelegateForTesting(
      gfx::NativeWindow native_window);
  static void SetSelectedAppForTesting(const std::u16string& target_name);

 private:
  using SharesheetServiceIconLoaderCallback =
      base::OnceCallback<void(std::vector<TargetInfo> targets)>;

  void PrepareToShowBubble(apps::IntentPtr intent,
                           GetNativeWindowCallback get_native_window_callback,
                           DeliveredCallback delivered_callback,
                           CloseCallback close_callback);

  std::vector<TargetInfo> GetActionsForIntent(const apps::IntentPtr& intent);

  void LoadAppIcons(std::vector<apps::IntentLaunchInfo> intent_launch_info,
                    std::vector<TargetInfo> targets,
                    size_t index,
                    SharesheetServiceIconLoaderCallback callback);

  void OnIconLoaded(std::vector<apps::IntentLaunchInfo> intent_launch_info,
                    std::vector<TargetInfo> targets,
                    size_t index,
                    SharesheetServiceIconLoaderCallback callback,
                    apps::IconValuePtr icon_value);

  void OnAppIconsLoaded(apps::IntentPtr intent,
                        GetNativeWindowCallback get_native_window_callback,
                        DeliveredCallback delivered_callback,
                        CloseCallback close_callback,
                        std::vector<TargetInfo> targets);

  void OnReadyToShowBubble(gfx::NativeWindow native_window,
                           apps::IntentPtr intent,
                           DeliveredCallback delivered_callback,
                           CloseCallback close_callback,
                           std::vector<TargetInfo> targets);

  void LaunchApp(const std::u16string& target_name, apps::IntentPtr intent);

  SharesheetServiceDelegator* GetOrCreateDelegator(
      gfx::NativeWindow native_window);
  SharesheetServiceDelegator* GetDelegator(gfx::NativeWindow native_window);

  void RecordUserActionMetrics(
      const std::optional<ShareActionType>& share_action_type,
      const std::optional<std::u16string>& app_name);
  void RecordTargetCountMetrics(const std::vector<TargetInfo>& targets);
  // Makes |intent| related UMA recordings.
  void RecordShareDataMetrics(const apps::IntentPtr& intent);

  raw_ptr<Profile> profile_;
  std::unique_ptr<ShareActionCache> share_action_cache_;
  raw_ptr<apps::AppServiceProxy> app_service_proxy_;

  // Record of all active SharesheetServiceDelegators. These can be retrieved
  // by ShareActions and used as SharesheetControllers to make bubble changes.
  std::vector<std::unique_ptr<SharesheetServiceDelegator>> active_delegators_;

  base::WeakPtrFactory<SharesheetService> weak_factory_{this};
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
