// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_handler.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
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

AXMediaAppHandler::~AXMediaAppHandler() {
  ui::AXPlatformNode::RemoveAXModeObserver(this);
}

bool AXMediaAppHandler::IsOcrServiceEnabled() const {
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
void AXMediaAppHandler::SetScreenAIAnnotatorForTesting(
    mojo::PendingRemote<screen_ai::mojom::ScreenAIAnnotator>
        screen_ai_annotator) {
  screen_ai_annotator_.reset();
  screen_ai_annotator_.Bind(std::move(screen_ai_annotator));
}

void AXMediaAppHandler::FlushForTesting() {
  screen_ai_annotator_.FlushForTesting();  // IN-TEST
}

void AXMediaAppHandler::StateChanged(ScreenAIInstallState::State state) {
  CHECK(features::IsBacklightOcrEnabled());
  if (screen_ai_install_state_ == state) {
    return;
  }
  screen_ai_install_state_ = state;
  content::BrowserContext* browser_context = media_app_->GetBrowserContext();
  bool is_ocr_service_enabled = IsOcrServiceEnabled();
  if (browser_context && is_ocr_service_enabled &&
      !screen_ai_annotator_.is_bound()) {
    screen_ai::ScreenAIServiceRouter* service_router =
        screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
            browser_context);
    service_router->BindScreenAIAnnotator(
        screen_ai_annotator_.BindNewPipeAndPassReceiver());
    OcrNextDirtyPageIfAny();
  }
  media_app_->OcrServiceEnabledChanged(is_ocr_service_enabled);
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
    const std::vector<uint64_t>& dirty_pages) {
  CHECK_EQ(page_locations.size(), dirty_pages.size());
  if (!features::IsBacklightOcrEnabled()) {
    return;
  }
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  for (size_t i = 0; i < page_locations.size(); ++i) {
    dirty_pages_.emplace(page_locations[i], dirty_pages[i]);
  }
  OcrNextDirtyPageIfAny();
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
}

void AXMediaAppHandler::ViewportUpdated(const gfx::Insets& viewport_box,
                                        float scaleFactor) {}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXMediaAppHandler::OcrNextDirtyPageIfAny() {
  CHECK(features::IsBacklightOcrEnabled());
  if (dirty_pages_.empty() || !IsOcrServiceEnabled()) {
    return;
  }
  DirtyPageInfo dirty_page_info = dirty_pages_.front();
  dirty_pages_.pop();
  SkBitmap page_bitmap =
      media_app_->RequestBitmap(dirty_page_info.dirty_page_index);
  screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
      page_bitmap, base::BindOnce(&AXMediaAppHandler::OnPageOcred,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  std::move(dirty_page_info)));
}

void AXMediaAppHandler::OnPageOcred(DirtyPageInfo dirty_page_info,
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
  auto [iter, inserted] = pages_.insert(std::make_pair(
      dirty_page_info.dirty_page_index, std::make_unique<ui::AXTreeManager>()));
  if (inserted) {
    iter->second->SetTree(std::make_unique<ui::AXTree>(complete_tree_update));
  } else {
    CHECK(iter->second->ax_tree());
    CHECK(iter->second->ax_tree()->Unserialize(complete_tree_update))
        << iter->second->ax_tree()->error();
  }
  auto* tree = iter->second->ax_tree();
  CHECK(tree->root());
  ui::AXNodeData root_data = tree->root()->data();
  // TODO(nektar): Why are we passing `gfx::Insets` instead of `gfx::Rect`? Talk
  // to Backlight Team.
  root_data.relative_bounds.bounds = gfx::RectF(
      dirty_page_info.page_location.left(), dirty_page_info.page_location.top(),
      dirty_page_info.page_location.width(),
      dirty_page_info.page_location.height());
  ui::AXTreeUpdate location_update;
  location_update.root_id = tree->root()->id();
  location_update.nodes = {root_data};
  CHECK(tree->Unserialize(location_update)) << tree->error();
  // TODO(nektar): Attach the page to the tree for the main PDF document.
  OcrNextDirtyPageIfAny();
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash
