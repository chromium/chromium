// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_sharing_manager_coordinator.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/host/context/glic_active_pinned_focused_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_focused_browser_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#else
#include "chrome/browser/glic/host/context/glic_empty_pinned_tab_manager.h"
#endif

namespace glic {

GlicSharingManagerCoordinator::GlicSharingManagerCoordinator(
    Profile* profile,
    GlicInstance::UIDelegate* ui_delegate,
    GlicMetrics* metrics)
    : GlicSharingManagerCoordinator(
          profile,
          ui_delegate,
          metrics,
#if !BUILDFLAG(IS_ANDROID)
          new GlicFocusedBrowserManagerImpl(ui_delegate, profile),
          new GlicFocusedBrowserManagerImpl(ui_delegate, profile)
#else
          false
#endif
      ) {
}

GlicSharingManagerCoordinator::GlicSharingManagerCoordinator(
    Profile* profile,
    GlicInstance::UIDelegate* ui_delegate,
    GlicMetrics* metrics,
#if !BUILDFLAG(IS_ANDROID)
    GlicFocusedBrowserManager* detached_mode_focused_browser_manager,
    GlicFocusedBrowserManager* live_mode_focused_browser_manager
#else
    bool ignored
#endif
    )
    : pinned_tab_manager_(
          std::make_unique<GlicPinnedTabManagerImpl>(profile,
                                                     ui_delegate,
                                                     metrics)),
#if !BUILDFLAG(IS_ANDROID)
      detached_mode_sharing_manager_(
          std::make_unique<GlicPinAwareDetachedFocusedTabManager>(
              &sharing_manager_,
              detached_mode_focused_browser_manager),
          base::WrapUnique<GlicFocusedBrowserManager>(
              detached_mode_focused_browser_manager),
          pinned_tab_manager_.get(),
          profile,
          metrics),
      live_mode_sharing_manager_(std::make_unique<GlicFocusedTabManager>(
                                     live_mode_focused_browser_manager),
                                 base::WrapUnique<GlicFocusedBrowserManager>(
                                     live_mode_focused_browser_manager),
                                 pinned_tab_manager_.get(),
                                 profile,
                                 metrics),
#endif
      attached_mode_sharing_manager_(
          std::make_unique<GlicActivePinnedFocusedTabManager>(
              profile,
              &sharing_manager_),
          std::make_unique<GlicEmptyFocusedBrowserManager>(),
          pinned_tab_manager_.get(),
          profile,
          metrics),
      sharing_manager_(&attached_mode_sharing_manager_) {
}

GlicSharingManagerCoordinator::~GlicSharingManagerCoordinator() = default;

GlicSharingManager& GlicSharingManagerCoordinator::GetActiveSharingManager() {
  return sharing_manager_;
}

GlicPinnedTabManager* GlicSharingManagerCoordinator::GetPinnedTabManager() {
  return pinned_tab_manager_.get();
}

void GlicSharingManagerCoordinator::UpdateState(
    mojom::PanelStateKind panel_state_kind,
    mojom::WebClientMode interaction_mode) {
  if (panel_state_kind != mojom::PanelStateKind::kHidden) {
    last_non_hidden_panel_state_kind_ = panel_state_kind;
  }
  interaction_mode_ = interaction_mode;
  UpdateSharingManagerDelegate();
}

void GlicSharingManagerCoordinator::OnGlicWindowActivationChanged(bool active) {
  sharing_manager_.OnGlicWindowActivationChanged(active);
}

void GlicSharingManagerCoordinator::UpdateSharingManagerDelegate() {
  if (last_non_hidden_panel_state_kind_ == mojom::PanelStateKind::kAttached) {
    sharing_manager_.SetDelegate(&attached_mode_sharing_manager_);
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (interaction_mode_ == mojom::WebClientMode::kAudio) {
    sharing_manager_.SetDelegate(&live_mode_sharing_manager_);
    return;
  }

  sharing_manager_.SetDelegate(&detached_mode_sharing_manager_);
#else
  NOTREACHED() << "Android only has attached mode";
#endif
}

}  // namespace glic
