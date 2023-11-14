// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_f.h"
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

namespace ash {

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
using screen_ai::ScreenAIInstallState;
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

AXMediaAppUntrustedHandler::AXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : browser_context_(context), media_app_page_(std::move(page)) {
  if (features::IsBacklightOcrEnabled()) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    CHECK(ScreenAIInstallState::GetInstance())
        << "`ScreenAIInstallState` should always be created on browser "
           "startup.";
    screen_ai_install_state_ = ScreenAIInstallState::GetInstance()->get_state();
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

AXMediaAppUntrustedHandler::~AXMediaAppUntrustedHandler() {
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

bool AXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
  if (!features::IsBacklightOcrEnabled()) {
    return false;
  }
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (is_ocr_service_enabled_for_testing_) {
    return true;
  }
  CHECK(ScreenAIInstallState::GetInstance())
      << "`ScreenAIInstallState` should always be created on browser startup.";
  switch (screen_ai_install_state_) {
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
void AXMediaAppUntrustedHandler::SetScreenAIAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator) {
  screen_ai_annotator_.reset();
  screen_ai_annotator_.Bind(std::move(screen_ai_annotator));
}

void AXMediaAppUntrustedHandler::FlushForTesting() {
  screen_ai_annotator_.FlushForTesting();  // IN-TEST
}

void AXMediaAppUntrustedHandler::StateChanged(
    ScreenAIInstallState::State state) {
  CHECK(features::IsBacklightOcrEnabled());
  if (screen_ai_install_state_ == state) {
    return;
  }
  screen_ai_install_state_ = state;
  bool is_ocr_service_enabled = IsOcrServiceEnabled();
  if (is_ocr_service_enabled && !screen_ai_annotator_.is_bound()) {
    screen_ai::ScreenAIServiceRouter* service_router =
        screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
            std::to_address(browser_context_));
    service_router->BindScreenAIAnnotator(
        screen_ai_annotator_.BindNewPipeAndPassReceiver());
    OcrNextDirtyPageIfAny();
  }
  if (media_app_) {
    media_app_->OcrServiceEnabledChanged(is_ocr_service_enabled);
  }
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

bool AXMediaAppUntrustedHandler::IsAccessibilityEnabled() const {
  return features::IsBacklightOcrEnabled() &&
         accessibility_state_utils::IsScreenReaderEnabled();
}

void AXMediaAppUntrustedHandler::PerformAction(
    const ui::AXActionData& action_data) {
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

void AXMediaAppUntrustedHandler::OnAXModeAdded(ui::AXMode mode) {
  CHECK(features::IsBacklightOcrEnabled());
  if (media_app_) {
    media_app_->AccessibilityEnabledChanged(
        accessibility_state_utils::IsScreenReaderEnabled());
  }
}

void AXMediaAppUntrustedHandler::DocumentUpdated(
    const std::vector<gfx::Insets>& page_locations,
    const std::vector<uint64_t>& dirty_pages) {
  // `page_locations` should contain the new locations of all pages, whilst
  // `dirty_pages` only the indices of all the pages that need to be OCRed.
  CHECK_GE(page_locations.size(), dirty_pages.size());
  if (!features::IsBacklightOcrEnabled()) {
    return;
  }
  page_locations_ = page_locations;
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (dirty_pages.empty()) {
    for (size_t i = 0; i < page_locations_.size(); ++i) {
      UpdatePageLocation(static_cast<uint64_t>(i), page_locations_[i]);
    }
  } else {
    size_t page_locations_size = page_locations.size();
    pages_.resize(page_locations_size);
    for (uint64_t dirty_page_index : dirty_pages) {
      if (dirty_page_index >= static_cast<uint64_t>(page_locations_size)) {
        continue;
      }
      dirty_page_indices_.push(dirty_page_index);
    }
    OcrNextDirtyPageIfAny();
  }
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

void AXMediaAppUntrustedHandler::ViewportUpdated(
    const gfx::Insets& viewport_box, float scaleFactor) {}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXMediaAppUntrustedHandler::UpdatePageLocation(uint64_t page_index,
                                           const gfx::Insets& page_location) {
  CHECK_LT(page_index, static_cast<uint64_t>(pages_.size()));
  ui::AXTreeManager* tree_manager = pages_[page_index].get();
  if (!tree_manager) {
    return;
  }
  ui::AXTree* tree = tree_manager->ax_tree();
  CHECK(tree->root());
  ui::AXNodeData root_data = tree->root()->data();
  root_data.relative_bounds.bounds =
      gfx::RectF(page_location.left(), page_location.top(),
                 page_location.width(), page_location.height());
  ui::AXTreeUpdate location_update;
  location_update.root_id = tree->root()->id();
  location_update.nodes = {root_data};
  CHECK(tree->Unserialize(location_update)) << tree->error();
}

void AXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny() {
  CHECK(features::IsBacklightOcrEnabled());
  if (!IsOcrServiceEnabled()) {
    return;
  }
  if (dirty_page_indices_.empty()) {
    for (size_t i = 0; i < page_locations_.size(); ++i) {
      UpdatePageLocation(static_cast<uint64_t>(i), page_locations_[i]);
    }
    return;
  }
  uint64_t dirty_page_index = dirty_page_indices_.front();
  dirty_page_indices_.pop();
  SkBitmap page_bitmap = media_app_->RequestBitmap(dirty_page_index);
  screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
      page_bitmap,
      base::BindOnce(&AXMediaAppUntrustedHandler::OnPageOcred,
                     weak_ptr_factory_.GetWeakPtr(), dirty_page_index));
}

void AXMediaAppUntrustedHandler::OnPageOcred(uint64_t dirty_page_index,
                                    const ui::AXTreeUpdate& tree_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(features::IsBacklightOcrEnabled());
  // The tree update that comes from the OCR Service is only a list of nodes,
  // and it's missing valid tree data.
  //
  // TODO(nektar): Investigate if we can fix this in the OCR Service.
  CHECK(!tree_update.has_tree_data);
  CHECK_EQ(ui::AXTreeIDUnknown(), tree_update.tree_data.tree_id);
  CHECK_NE(ui::kInvalidAXNodeID, tree_update.root_id);
  ui::AXTreeUpdate complete_tree_update;
  complete_tree_update.has_tree_data = true;
  complete_tree_update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  complete_tree_update.tree_data.title = "OCR results";
  complete_tree_update.root_id = tree_update.root_id;
  complete_tree_update.nodes = tree_update.nodes;
  CHECK_LT(dirty_page_index, static_cast<uint64_t>(pages_.size()));
  if (!pages_[dirty_page_index]) {
    pages_[dirty_page_index] = std::make_unique<ui::AXTreeManager>(
        std::make_unique<ui::AXTree>(complete_tree_update));
  } else {
    CHECK(pages_[dirty_page_index]->ax_tree());
    CHECK(
        pages_[dirty_page_index]->ax_tree()->Unserialize(complete_tree_update))
        << pages_[dirty_page_index]->ax_tree()->error();
  }
  // TODO(nektar): Attach the page to the tree for the main PDF document.
  OcrNextDirtyPageIfAny();
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash
