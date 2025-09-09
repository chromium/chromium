// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_instance_components.h"

#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_occlusion_notifier.h"
#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"

namespace glic {
GlicInstanceComponents::GlicInstanceComponents(
    Profile* profile,
    GlicWindowController& window_controller,
    Host& host,
    GlicMetrics& metrics,
    contextual_cueing::ContextualCueingService* contextual_cueing_service)
    : sharing_manager_(
          std::make_unique<GlicSharingManagerImpl>(profile,
                                                   &window_controller,
                                                   &metrics)),
      zero_state_suggestions_manager_(
          std::make_unique<GlicZeroStateSuggestionsManager>(
              sharing_manager_.get(),
              &window_controller,
              contextual_cueing_service)),
      screenshot_capturer_(std::make_unique<GlicScreenshotCapturer>()) {}

GlicInstanceComponents::~GlicInstanceComponents() = default;

GlicSharingManager& GlicInstanceComponents::sharing_manager() {
  return *sharing_manager_;
}

}  // namespace glic
