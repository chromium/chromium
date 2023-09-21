// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "ui/accessibility/accessibility_features.h"

namespace ash {

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
using screen_ai::ScreenAIInstallState;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

AXMediaAppHandler::AXMediaAppHandler(AXMediaApp* media_app)
    : media_app_(media_app) {
  CHECK(media_app_);
  if (features::IsBacklightOcrEnabled()) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    CHECK(ScreenAIInstallState::GetInstance())
        << "`ScreenAIInstallState` should always be created on browser "
           "startup.";
    screen_ai_component_state_observer_.Observe(
        ScreenAIInstallState::GetInstance());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

    // `BrowserAccessibilityState` needs to be constructed for `AXModeObserver`s
    // to work.
    auto* ax_state = content::BrowserAccessibilityState::GetInstance();
    if (!ax_state) {
      NOTREACHED_NORETURN();
    }
    ui::AXPlatformNode::AddAXModeObserver(this);
  }
}

AXMediaAppHandler::~AXMediaAppHandler() {
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

bool AXMediaAppHandler::IsOcrServiceEnabled() const {
  if (!features::IsBacklightOcrEnabled()) {
    return false;
  }
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  CHECK(ScreenAIInstallState::GetInstance())
      << "`ScreenAIInstallState` should always be created on browser startup.";
  ScreenAIInstallState::State install_state =
      ScreenAIInstallState::GetInstance()->get_state();
  switch (install_state) {
    case ScreenAIInstallState::State::kNotDownloaded:
      ScreenAIInstallState::GetInstance()->DownloadComponent();
      [[fallthrough]];
    case ScreenAIInstallState::State::kFailed:
    case ScreenAIInstallState::State::kDownloading:
      return false;
    case ScreenAIInstallState::State::kDownloaded:
    case ScreenAIInstallState::State::kReady:
      return true;
  }
#else
  return false;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXMediaAppHandler::StateChanged(ScreenAIInstallState::State state) {
  CHECK(features::IsBacklightOcrEnabled());
  ScreenAIInstallState::State new_state =
      ScreenAIInstallState::GetInstance()->get_state();
  if (previous_ocr_install_state_ != new_state) {
    previous_ocr_install_state_ = new_state;
    media_app_->OcrServiceEnabledChanged(IsOcrServiceEnabled());
  }
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

bool AXMediaAppHandler::IsAccessibilityEnabled() const {
  return features::IsBacklightOcrEnabled() &&
         accessibility_state_utils::IsScreenReaderEnabled();
}

void AXMediaAppHandler::OnAXModeAdded(ui::AXMode mode) {
  CHECK(features::IsBacklightOcrEnabled());
  media_app_->AccessibilityEnabledChanged(
      accessibility_state_utils::IsScreenReaderEnabled());
}

void AXMediaAppHandler::DocumentUpdated(
    const std::vector<gfx::Insets>& page_locations,
    const std::vector<uint64_t>& dirty_pages) {}

void AXMediaAppHandler::ViewportUpdated(const gfx::Insets& viewport_box,
                                        float scaleFactor) {}

}  // namespace ash
