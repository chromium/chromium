// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
#define CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_

#include <list>
#include <memory>
#include <queue>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_optimization_guide_service.h"
#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screen_ai_service.h"
#include "chrome/browser/accessibility/tree_fixing/internal/ax_tree_fixing_screenshotter.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/ax_node_id_forward.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/callback_list.h"
#else
#include "base/scoped_observation.h"
#include "ui/accessibility/platform/ax_mode_observer.h"
#include "ui/accessibility/platform/ax_platform.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class Profile;

#if BUILDFLAG(IS_CHROMEOS)
namespace ash {
struct AccessibilityStatusEventDetails;
}  // namespace ash
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class AXTreeID;
struct AXTreeUpdate;
}  // namespace ui

namespace tree_fixing {

using HeadingsIdentificationCallback =
    base::OnceCallback<void(const ui::AXTreeID&,
                            const std::vector<ui::AXNodeID>)>;
using MainNodeIdentificationCallback =
    base::OnceCallback<void(const ui::AXTreeID&, ui::AXNodeID)>;
using ScreenshotCallback = base::OnceCallback<void(const SkBitmap& bitmap)>;

// This class handles the communication between the browser process and any
// downstream services used to fix the AXTree, such as: the optimization guide,
// Screen2x, Aratea, etc.
class AXTreeFixingServicesRouter
    : public KeyedService,
      public AXTreeFixingOptimizationGuideService::
          HeadingsIdentificationDelegate,
      public AXTreeFixingScreenAIService::MainNodeIdentificationDelegate,
      public AXTreeFixingScreenshotter::ScreenshotDelegate
#if !BUILDFLAG(IS_CHROMEOS)
    ,
      public ui::AXModeObserver
#endif  // !BUILDFLAG(IS_CHROMEOS)
{
 public:
  class AXTreeFixingWebContentsObserver : public content::WebContentsObserver {
   public:
    explicit AXTreeFixingWebContentsObserver(
        content::WebContents& web_contents,
        AXTreeFixingServicesRouter* router);
    AXTreeFixingWebContentsObserver(AXTreeFixingWebContentsObserver&&) = delete;
    AXTreeFixingWebContentsObserver(const AXTreeFixingWebContentsObserver&) =
        delete;
    AXTreeFixingWebContentsObserver& operator=(
        AXTreeFixingWebContentsObserver&&) = delete;
    AXTreeFixingWebContentsObserver& operator=(
        const AXTreeFixingWebContentsObserver&) = delete;
    ~AXTreeFixingWebContentsObserver() override;

    // content::WebContentsObserver:
    void DocumentOnLoadCompletedInPrimaryMainFrame() override;

   private:
    void TryIdentifyMainNode();
    void OnMainNodeIdentified(const ui::AXTreeID& tree_id,
                              ui::AXNodeID node_id);
    void OnHeadingsIdentified(const ui::AXTreeID& tree_id,
                              const std::vector<ui::AXNodeID> headings);

    uint32_t retry_attempts_ = 0;
    raw_ptr<AXTreeFixingServicesRouter> router_;
    base::WeakPtrFactory<AXTreeFixingWebContentsObserver> weak_ptr_factory_{
        this};
  };

  explicit AXTreeFixingServicesRouter(Profile* profile);
  AXTreeFixingServicesRouter(const AXTreeFixingServicesRouter&) = delete;
  AXTreeFixingServicesRouter& operator=(const AXTreeFixingServicesRouter&) =
      delete;
  ~AXTreeFixingServicesRouter() override;

  // --- Public APIs for any request to fix an AXTree ---

  // Identifies the main node of an AXTree, and asynchronously returns the
  // identified node_id and its associated tree_id as a std::pair via the
  // provided callback. The AXTreeUpdate that clients provide to this method
  // should represent a full AXTree for the page in order to accurately identify
  // a main node. The AXTree should not have an existing node with Role kMain.
  // Returns true if the request was processed successfully, false otherwise.
  bool IdentifyMainNode(const ui::AXTreeUpdate& ax_tree,
                        MainNodeIdentificationCallback callback);

  // Identifies the headings of a page.
  void IdentifyHeadings(const ui::AXTreeUpdate& ax_tree,
                        const raw_ptr<content::WebContents> web_contents,
                        HeadingsIdentificationCallback callback);

#if !BUILDFLAG(IS_CHROMEOS)
  // ui::AXModeObserver:
  void OnAXModeAdded(ui::AXMode mode) override;
#endif  // !BUILDFLAG(IS_CHROMEOS)

 private:
  // AXTreeFixingScreenAIService::MainNodeIdentificationDelegate overrides:
  void OnMainNodeIdentified(const ui::AXTreeID& tree_id,
                            ui::AXNodeID node_id,
                            int request_id) override;
  void OnServiceStateChanged(bool service_ready) override;

  void MakeMainNodeRequestToScreenAI(const ui::AXTreeUpdate& ax_tree,
                                     MainNodeIdentificationCallback callback);

  // AXTreeFixingOptimizationGuideService::HeadingsIdentificationDelegate
  // overrides:
  void OnHeadingsIdentified(const ui::AXTreeID& tree_id,
                            const std::vector<ui::AXNodeID>,
                            int request_id) override;

  // AXTreeFixingScreenshotter::ScreenshotDelegate overrides:
  void OnScreenshotCaptured(const SkBitmap& bitmap, int request_id) override;

  void MakeScreenshotRequest(const raw_ptr<content::WebContents> web_contents,
                             ScreenshotCallback callback);
  void OnScreenshotReceivedForHeadings(int opt_guide_request_id,
                                       const ui::AXTreeUpdate& ax_tree_update,
                                       const SkBitmap& bitmap);

  // ScreenAI related objects: service instance, and a list of callbacks/ids.
  std::unique_ptr<AXTreeFixingScreenAIService> screen_ai_service_;
  std::list<std::pair<int, MainNodeIdentificationCallback>>
      pending_screen_ai_callbacks_;
  int next_screen_ai_request_id_ = 0;
  bool can_make_main_node_identification_requests_ = false;
  using QueuedScreenAIRequest =
      std::tuple<ui::AXTreeUpdate, MainNodeIdentificationCallback>;
  std::queue<QueuedScreenAIRequest> screen_ai_request_queue_;

  // Screenshot related objects: screenshot instance, a list of callbacks/ids.
  std::unique_ptr<AXTreeFixingScreenshotter> screenshotter_;
  std::list<std::pair<int, ScreenshotCallback>> pending_screenshot_callbacks_;
  int next_screenshot_request_id_ = 0;

  // Optimization Guide related objects.
  std::unique_ptr<AXTreeFixingOptimizationGuideService> opt_guide_service_;
  std::list<std::pair<int, HeadingsIdentificationCallback>>
      pending_opt_guide_callbacks_;
  int next_opt_guide_request_id_ = 0;

  void ToggleEnabledState();

  std::vector<std::unique_ptr<AXTreeFixingWebContentsObserver>>
      web_contents_observers_;
  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;

#if BUILDFLAG(IS_CHROMEOS)
  void OnAccessibilityStatusEvent(
      const ash::AccessibilityStatusEventDetails& details);

  base::CallbackListSubscription accessibility_status_subscription_;
#else
  ui::AXMode current_ax_mode_;
  base::ScopedObservation<ui::AXPlatform, ui::AXModeObserver>
      ax_mode_observation_{this};
#endif  // BUILDFLAG(IS_CHROMEOS)
  base::WeakPtrFactory<AXTreeFixingServicesRouter> weak_ptr_factory_{this};
};

}  // namespace tree_fixing

#endif  // CHROME_BROWSER_ACCESSIBILITY_TREE_FIXING_AX_TREE_FIXING_SERVICES_ROUTER_H_
