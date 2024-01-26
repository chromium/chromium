// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
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

namespace {

// The maximum number of pages supported by the OCR service. This maximum is
// used both to validate the number of pages (untrusted data) coming from the
// MediaApp and manage resources (caps the number of pages stored at a time).
const size_t kMaxPages = 10000;

bool ReportIfNonExistentPageId(
    const std::string& context,
    const std::string& page_id,
    const std::map<const std::string, AXMediaAppPageMetadata>& metadata) {
  if (!metadata.contains(page_id)) {
    mojo::ReportBadMessage(
        std::format("{} called with previously non-existent page ID", context));
    return true;
  }
  return false;
}

}  // namespace

AXMediaAppUntrustedHandler::AXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : browser_context_(context), media_app_page_(std::move(page)) {
  if (!base::FeatureList::IsEnabled(ash::features::kMediaAppPdfA11yOcr)) {
    return;
  }
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
    CHECK(ScreenAIInstallState::GetInstance())
        << "`ScreenAIInstallState` should always be created on browser "
           "startup.";
    screen_ai_install_state_ = ScreenAIInstallState::GetInstance()->get_state();
    screen_ai_component_state_observer_.Observe(
        ScreenAIInstallState::GetInstance());
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

    ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
}

AXMediaAppUntrustedHandler::~AXMediaAppUntrustedHandler() = default;

bool AXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
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
  return base::FeatureList::IsEnabled(ash::features::kMediaAppPdfA11yOcr) &&
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
  if (media_app_) {
    media_app_->AccessibilityEnabledChanged(
        accessibility_state_utils::IsScreenReaderEnabled());
  }
}
void AXMediaAppUntrustedHandler::PageMetadataUpdated(
    const std::vector<ash::media_app_ui::mojom::PageMetadataPtr>
        page_metadata) {
  if (page_metadata.empty()) {
    mojo::ReportBadMessage("SetPageMetadata() called with no page metadata");
    return;
  }

  const size_t num_pages = std::min(page_metadata.size(), kMaxPages);
  // If page_metadata_ is empty, this is the first load of the PDF.
  const bool is_first_load = page_metadata_.empty();

  if (is_first_load) {
    for (size_t i = 0; i < num_pages; ++i) {
      AXMediaAppPageMetadata data;
      // The page IDs will never change, so this should be the only place that
      // updates them.
      data.id = page_metadata[i]->id;
      if (page_metadata_.contains(data.id)) {
        mojo::ReportBadMessage(
            "SetPageMetadata() called with pages with duplicate page IDs");
        return;
      }
      page_metadata_[data.id] = data;
      dirty_page_ids_.push(data.id);
    }
    // Only one page goes through OCR at a time, so start the process here.
    OcrNextDirtyPageIfAny();
  }

  // Update all page numbers and rects.
  std::set<const std::string> page_id_updated;
  for (size_t i = 0; i < page_metadata.size(); ++i) {
    const std::string& page_id = page_metadata[i]->id;
    if (ReportIfNonExistentPageId("SetPageMetadata()", page_id,
                                  page_metadata_)) {
      return;
    }
    page_metadata_[page_id].page_num = i + 1;  // 1-indexed.
    page_metadata_[page_id].rect = page_metadata[i]->rect;

    // Page location can only be set after the corresponding |pages_|
    // AXTreeManager entry has been created, so don't update it for first load.
    if (!is_first_load) {
      page_id_updated.insert(page_id);
      UpdatePageLocation(page_id, page_metadata[i]->rect);
    }
  }

  // Skip all further processing that applies to only updates (not first load).
  if (is_first_load) {
    return;
  }
  // If a page was missing from `page_metadata` (its location was not updated),
  // then that means it got deleted. Set its page number to 0.
  for (auto const& [page_id, _] : page_metadata_) {
    if (!page_id_updated.contains(page_id)) {
      // Since `pages_` and `page_metadata_` are both populated from untrusted
      // code, mitigate potential issues by never mutating the size of these two
      // containers. So when a page is 'deleted' by the user, keep it in memory.
      page_metadata_[page_id].page_num = 0;
    }
  }
}

void AXMediaAppUntrustedHandler::ViewportUpdated(const gfx::RectF& viewport_box,
                                                 float scale_factor) {}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXMediaAppUntrustedHandler::UpdatePageLocation(
    const std::string& page_id,
    const gfx::RectF& page_location) {
  if (ReportIfNonExistentPageId("UpdatePageLocation()", page_id,
                                page_metadata_)) {
    return;
  }
  if (!pages_.contains(page_id)) {
    return;
  }
  ui::AXTree* tree = pages_[page_id]->ax_tree();
  if (!tree->root()) {
    return;
  }
  ui::AXNodeData root_data = tree->root()->data();
  root_data.relative_bounds.bounds = page_location;
  ui::AXTreeUpdate location_update;
  location_update.root_id = tree->root()->id();
  location_update.nodes = {root_data};
  if (!tree->Unserialize(location_update)) {
    mojo::ReportBadMessage(tree->error());
    return;
  }
}

void AXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny() {
  if (!IsOcrServiceEnabled()) {
    return;
  }
  if (dirty_page_ids_.empty()) {
    return;
  }
  auto dirty_page_id = dirty_page_ids_.front();
  dirty_page_ids_.pop();
  SkBitmap page_bitmap = media_app_->RequestBitmap(dirty_page_id);
  screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
      page_bitmap,
      base::BindOnce(&AXMediaAppUntrustedHandler::OnPageOcred,
                     weak_ptr_factory_.GetWeakPtr(), dirty_page_id));
}

void AXMediaAppUntrustedHandler::OnPageOcred(
    const std::string& dirty_page_id,
    const ui::AXTreeUpdate& tree_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The tree update that comes from the OCR Service is only a list of nodes,
  // and it's missing valid tree data.
  //
  // TODO(b/289012145): Investigate if we can fix this in the OCR Service.
  if (tree_update.has_tree_data ||
      ui::kInvalidAXNodeID == tree_update.root_id) {
    mojo::ReportBadMessage("OnPageOcred() bad tree update from Screen AI.");
    return;
  }
  ui::AXTreeUpdate complete_tree_update;
  complete_tree_update.has_tree_data = true;
  complete_tree_update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  complete_tree_update.tree_data.title = "OCR results";
  complete_tree_update.root_id = tree_update.root_id;
  complete_tree_update.nodes = tree_update.nodes;
  if (ReportIfNonExistentPageId("OnPageOcred()", dirty_page_id,
                                page_metadata_)) {
    return;
  }
  if (!pages_.contains(dirty_page_id)) {
    pages_[dirty_page_id] = std::make_unique<ui::AXTreeManager>(
        std::make_unique<ui::AXTree>(complete_tree_update));
    UpdatePageLocation(dirty_page_id, page_metadata_[dirty_page_id].rect);
  } else {
    if (!pages_[dirty_page_id]->ax_tree() ||
        !pages_[dirty_page_id]->ax_tree()->Unserialize(complete_tree_update)) {
      mojo::ReportBadMessage(pages_[dirty_page_id]->ax_tree()->error());
      return;
    }
  }
  // TODO(nektar): Attach the page to the tree for the main PDF document.
  OcrNextDirtyPageIfAny();
}
#endif  // BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)

}  // namespace ash
