// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
#define CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sharesheet/sharesheet_action_cache.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace apps {
struct IntentLaunchInfo;
class AppServiceProxyBase;
}

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

class SharesheetServiceDelegate;

// The SharesheetService is the root service that provides a sharesheet for
// Chrome desktop.
class SharesheetService : public KeyedService {
 public:
  explicit SharesheetService(Profile* profile);
  ~SharesheetService() override;

  SharesheetService(const SharesheetService&) = delete;
  SharesheetService& operator=(const SharesheetService&) = delete;

  // Displays the dialog (aka bubble) for sharing content (or files) with
  // other applications and targets. |intent| contains the list of the
  // files/content to be shared. If the files to share contains Google
  // Drive hosted document, only drive share action will be shown.
  //
  // |delivered_callback| is run to signify that the intent has been
  // delivered to the target selected by the user (which may then show its own
  // separate UI, e.g. for Nearby Sharing)
  void ShowBubble(content::WebContents* web_contents,
                  apps::mojom::IntentPtr intent,
                  SharesheetMetrics::LaunchSource source,
                  DeliveredCallback delivered_callback);
  void ShowBubble(content::WebContents* web_contents,
                  apps::mojom::IntentPtr intent,
                  bool contains_hosted_document,
                  SharesheetMetrics::LaunchSource source,
                  DeliveredCallback delivered_callback);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Skips the generic Sharesheet bubble and directly displays the
  // NearbyShare bubble dialog.
  void ShowNearbyShareBubble(content::WebContents* web_contents,
                             apps::mojom::IntentPtr intent,
                             SharesheetMetrics::LaunchSource source,
                             sharesheet::DeliveredCallback delivered_callback);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  void OnBubbleClosed(gfx::NativeWindow native_window,
                      const std::u16string& active_action);
  void OnTargetSelected(gfx::NativeWindow native_window,
                        const std::u16string& target_name,
                        const TargetType type,
                        apps::mojom::IntentPtr intent,
                        views::View* share_action_view);
  bool OnAcceleratorPressed(const ui::Accelerator& accelerator,
                            const std::u16string& active_action);
  SharesheetServiceDelegate* GetOrCreateDelegate(
      gfx::NativeWindow native_window);
  SharesheetServiceDelegate* GetDelegate(gfx::NativeWindow native_window);

  // If the files to share contains a Google Drive hosted document, only the
  // drive share action will be shown.
  bool HasShareTargets(const apps::mojom::IntentPtr& intent,
                       bool contains_hosted_document);
  Profile* GetProfile();
  const gfx::VectorIcon* GetVectorIcon(const std::u16string& display_name);

  static void SetSelectedAppForTesting(const std::u16string& target_name);

 private:
  using SharesheetServiceIconLoaderCallback =
      base::OnceCallback<void(std::vector<TargetInfo> targets)>;

  void LoadAppIcons(std::vector<apps::IntentLaunchInfo> intent_launch_info,
                    std::vector<TargetInfo> targets,
                    size_t index,
                    SharesheetServiceIconLoaderCallback callback);

  void LaunchApp(const std::u16string& target_name,
                 apps::mojom::IntentPtr intent);

  void OnIconLoaded(std::vector<apps::IntentLaunchInfo> intent_launch_info,
                    std::vector<TargetInfo> targets,
                    size_t index,
                    SharesheetServiceIconLoaderCallback callback,
                    apps::mojom::IconValuePtr icon_value);

  void OnAppIconsLoaded(SharesheetServiceDelegate* delegate,
                        apps::mojom::IntentPtr intent,
                        DeliveredCallback delivered_callback,
                        std::vector<TargetInfo> targets);

  void ShowBubbleWithDelegate(SharesheetServiceDelegate* delegate,
                              apps::mojom::IntentPtr intent,
                              bool contains_hosted_document,
                              DeliveredCallback delivered_callback);

  void RecordUserActionMetrics(const std::u16string& target_name);
  void RecordTargetCountMetrics(const std::vector<TargetInfo>& targets);
  void RecordShareActionMetrics(const std::u16string& target_name);

  Profile* profile_;
  std::unique_ptr<SharesheetActionCache> sharesheet_action_cache_;
  apps::AppServiceProxyBase* app_service_proxy_;

  // Record of all active SharesheetServiceDelegates. These can be retrieved
  // by ShareActions and used as SharesheetControllers to make bubble changes.
  std::vector<std::unique_ptr<SharesheetServiceDelegate>> active_delegates_;

  base::WeakPtrFactory<SharesheetService> weak_factory_{this};
};

}  // namespace sharesheet

#endif  // CHROME_BROWSER_SHARESHEET_SHARESHEET_SERVICE_H_
