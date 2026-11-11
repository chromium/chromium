// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_invocation_helper.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_feature.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/selection_overlay.mojom.h"
#include "chrome/browser/glic/selection/selection_overlay_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/metrics_logger.h"
#include "chrome/common/glic_enums.mojom-shared.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/platform_util.h"
#endif

namespace glic {

// TODO(b/573577148): Make
// InstanceIndependentHotkeyManager::RequestCaptureRegion() call this and delete
// its copy of the routing below.
void InvokeSelectionOverlay(tabs::TabInterface& tab,
                            ::selection::EntryPoint entry_point) {
#if BUILDFLAG(IS_CHROMEOS)
  if (platform_util::IsBrowserLockedFullscreen(
          tab.GetBrowserWindowInterface())) {
    return;
  }
#endif
  content::WebContents* web_contents = tab.GetContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!GetGlicProviderSpec(profile).SupportsFeature(
          GlicFeature::kCaptureRegion)) {
    return;
  }

  if (!base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox) ||
      GlicSidePanelCoordinator::IsShowing(&tab)) {
    if (auto* service = GlicKeyedService::Get(profile)) {
      // TODO(b/573577148): Pass a per-entry-point InvocationSource once
      // the web client accepts one for capture-region sessions.
      GlicInvokeOptions options(Target(tab),
                                mojom::InvocationSource::kCaptureRegionHotkey);
      options.wait_for_panel_open = true;
      service->Invoke(std::move(options));
    }
    return;
  }

  auto* controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  if (!controller) {
    return;
  }
  if (auto* selection_observer = GlicSelectionObserver::From(&tab)) {
    if (std::optional<gfx::Rect> bounds =
            selection_observer->GetCurrentSelectionBounds()) {
      selection_observer->DismissUI(
          GlicSelectionObserver::DismissReason::kActionTaken);
      controller->ShowWithSelection(
          selection_observer->GetSelectedFrame(), *bounds,
          selection::InteractionOptions::New(/*hide_handles=*/true,
                                             /*skip_intro_animation=*/true),
          entry_point);
      return;
    }
  }
  controller->Show(/*options=*/nullptr, entry_point);
}

}  // namespace glic
