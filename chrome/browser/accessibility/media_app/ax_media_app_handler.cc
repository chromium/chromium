// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

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

void AXMediaAppHandler::PerformAction(const ui::AXActionData& action_data) {
  switch (action_data.action) {
    case ax::mojom::Action::kBlur:
    case ax::mojom::Action::kClearAccessibilityFocus:
    case ax::mojom::Action::kCollapse:
    case ax::mojom::Action::kDecrement:
    case ax::mojom::Action::kDoDefault:
    case ax::mojom::Action::kExpand:
    case ax::mojom::Action::kFocus:
    case ax::mojom::Action::kGetImageData:
    case ax::mojom::Action::kIncrement:
    case ax::mojom::Action::kLoadInlineTextBoxes:
      return;
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollUp:
    case ax::mojom::Action::kScrollDown:
    case ax::mojom::Action::kScrollLeft:
    case ax::mojom::Action::kScrollRight:
    case ax::mojom::Action::kScrollToMakeVisible:
      NOTIMPLEMENTED();
      return;
    case ax::mojom::Action::kScrollToPoint:
    case ax::mojom::Action::kScrollToPositionAtRowColumn:
    case ax::mojom::Action::kSetAccessibilityFocus:
    case ax::mojom::Action::kSetScrollOffset:
    case ax::mojom::Action::kSetSelection:
    case ax::mojom::Action::kSetSequentialFocusNavigationStartingPoint:
    case ax::mojom::Action::kSetValue:
    case ax::mojom::Action::kShowContextMenu:
    case ax::mojom::Action::kStitchChildTree:
    case ax::mojom::Action::kCustomAction:
    case ax::mojom::Action::kHitTest:
    case ax::mojom::Action::kReplaceSelectedText:
    case ax::mojom::Action::kNone:
    case ax::mojom::Action::kGetTextLocation:
    case ax::mojom::Action::kAnnotatePageImages:
    case ax::mojom::Action::kSignalEndOfTest:
    case ax::mojom::Action::kShowTooltip:
    case ax::mojom::Action::kHideTooltip:
    case ax::mojom::Action::kInternalInvalidateTree:
    case ax::mojom::Action::kResumeMedia:
    case ax::mojom::Action::kStartDuckingMedia:
    case ax::mojom::Action::kStopDuckingMedia:
    case ax::mojom::Action::kSuspendMedia:
    case ax::mojom::Action::kLongClick:
      return;
  }
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
