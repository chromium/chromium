// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_COORDINATOR_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_instance.h"

namespace glic {

class GlicFocusedBrowserManager;
class GlicMetrics;
class GlicPinnedTabManager;

// Coordinates the lifecycle and switching of different GlicSharingManager
// implementations based on the instance state (attached, detached, live mode).
class GlicSharingManagerCoordinator {
 public:
  GlicSharingManagerCoordinator(Profile* profile,
                                GlicInstance::UIDelegate* ui_delegate,
                                GlicMetrics* metrics);
  ~GlicSharingManagerCoordinator();

  GlicSharingManagerCoordinator(const GlicSharingManagerCoordinator&) = delete;
  GlicSharingManagerCoordinator& operator=(
      const GlicSharingManagerCoordinator&) = delete;

  // returns the active sharing manager that delegates to the appropriate
  // internal manager based on current state.
  GlicSharingManager& GetActiveSharingManager();

  // Called to update the internal delegate based on state changes.
  void UpdateState(mojom::PanelStateKind panel_state_kind,
                   mojom::WebClientMode interaction_mode);

  GlicPinnedTabManager* GetPinnedTabManager();

  void OnGlicWindowActivationChanged(bool active);

 private:
  void UpdateSharingManagerDelegate();

  std::unique_ptr<GlicPinnedTabManager> pinned_tab_manager_;

#if !BUILDFLAG(IS_ANDROID)
  // managers for different modes
  GlicSharingManagerImpl detached_mode_sharing_manager_;
  GlicSharingManagerImpl live_mode_sharing_manager_;
#endif
  GlicSharingManagerImpl attached_mode_sharing_manager_;

  // The source of truth sharing manager that delegates to specific
  // implementations.
  GlicStablePinningDelegatingSharingManager sharing_manager_;

  // Current state tracking
  mojom::PanelStateKind last_non_hidden_panel_state_kind_ =
      mojom::PanelStateKind::kAttached;
  mojom::WebClientMode interaction_mode_ = mojom::WebClientMode::kText;

 private:
  GlicSharingManagerCoordinator(
      Profile* profile,
      GlicInstance::UIDelegate* ui_delegate,
      GlicMetrics* metrics,
#if !BUILDFLAG(IS_ANDROID)
      GlicFocusedBrowserManager* detached_mode_focused_browser_manager,
      GlicFocusedBrowserManager* live_mode_focused_browser_manager
#else
      bool ignored
#endif
  );
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARING_MANAGER_COORDINATOR_H_
