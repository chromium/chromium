// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/service/glic_offscreen_rendering_controller.h"

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/android/background_tabs/offscreen_rendering_manager_android.h"
#endif

namespace glic {

GlicOffscreenRenderingController::GlicOffscreenRenderingController(
    Delegate& delegate)
    : delegate_(delegate) {}

GlicOffscreenRenderingController::~GlicOffscreenRenderingController() = default;

void GlicOffscreenRenderingController::SetHiddenInitializing(
    bool initializing) {
  if (is_hidden_initializing_ == initializing) {
    return;
  }
  is_hidden_initializing_ = initializing;
  UpdateState();
}

void GlicOffscreenRenderingController::UpdateState() {
#if BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(features::kGlicAndroidOffscreenRendering)) {
    return;
  }

  const bool should_render_offscreen =
      !delegate_->HasActiveEmbedder() &&
      (is_hidden_initializing_ || delegate_->IsActuating());

  if (should_render_offscreen == is_active_) {
    return;
  }

  if (!should_render_offscreen) {
    Stop();
    return;
  }

  // TODO(b/572001459): Re-attach offscreen rendering when the host contents
  // are recreated (e.g. Host::Reload()) while offscreen rendering is active.
  delegate_->EnsureHostAwake();
  if (content::WebContents* contents =
          delegate_->GetOffscreenRenderingContents()) {
    actor::StartOffscreenRenderingForWebContents(contents);
    is_active_ = true;
  }
#endif  // BUILDFLAG(IS_ANDROID)
}

void GlicOffscreenRenderingController::Stop() {
#if BUILDFLAG(IS_ANDROID)
  if (!is_active_) {
    return;
  }
  if (content::WebContents* contents =
          delegate_->GetOffscreenRenderingContents()) {
    actor::StopOffscreenRenderingForWebContents(contents);
  }
  is_active_ = false;
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace glic
