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
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "content/public/browser/browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"

namespace ash {

namespace {

// The ID used for the AX document root.
const uint64_t kDocumentRootNodeId = 1;

// The first ID at which pages start. 0 is a special ID number reserved only for
// invalid nodes, and 1 is for the AX document root. So all pages begin at ID 2.
const size_t kStartPageAXNodeId = 2;

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
  screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
      std::to_address(browser_context_))
      ->GetServiceStateAsync(
          screen_ai::ScreenAIServiceRouter::Service::kOCR,
          base::BindOnce(&AXMediaAppUntrustedHandler::OnOCRServiceInitialized,
                         weak_ptr_factory_.GetWeakPtr()));

  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
}

AXMediaAppUntrustedHandler::~AXMediaAppUntrustedHandler() = default;

bool AXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
  return screen_ai_annotator_.is_bound();
}

void AXMediaAppUntrustedHandler::OnOCRServiceInitialized(bool successful) {
  if (!successful) {
    return;
  }

  // This is expected to be called only once.
  CHECK(!screen_ai_annotator_.is_bound());

  screen_ai::ScreenAIServiceRouter* service_router =
      screen_ai::ScreenAIServiceRouterFactory::GetForBrowserContext(
          std::to_address(browser_context_));
  service_router->BindScreenAIAnnotator(
      screen_ai_annotator_.BindNewPipeAndPassReceiver());
  OcrNextDirtyPageIfAny();

  if (media_app_) {
    media_app_->OcrServiceEnabledChanged(true);
  }
}

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
      PushDirtyPage(data.id);
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
  UpdateDocumentTree();

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

void AXMediaAppUntrustedHandler::PageContentsUpdated(
    const std::string& dirty_page_id) {
  if (!page_metadata_.contains(dirty_page_id)) {
    mojo::ReportBadMessage(
        "SetPageMetadata() called with a non-existent page ID");
    return;
  }
  PushDirtyPage(dirty_page_id);
  OcrNextDirtyPageIfAny();
}

void AXMediaAppUntrustedHandler::ViewportUpdated(const gfx::RectF& viewport_box,
                                                 float scale_factor) {}

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

void AXMediaAppUntrustedHandler::UpdateDocumentTree() {
  ui::AXNodeData document_root_data;
  document_root_data.id = kDocumentRootNodeId;
  document_root_data.role = ax::mojom::Role::kPdfRoot;
  // A scrollable container should (by design) also be focusable.
  document_root_data.AddState(ax::mojom::State::kFocusable);
  document_root_data.AddBoolAttribute(ax::mojom::BoolAttribute::kScrollable,
                                      true);
  document_root_data.AddBoolAttribute(ax::mojom::BoolAttribute::kClipsChildren,
                                      true);
  document_root_data.AddBoolAttribute(
      ax::mojom::BoolAttribute::kIsLineBreakingObject, true);
  // Text direction is set individually by each page element via the OCR
  // Service, so no need to set it here.
  //
  // Text alignment cannot be set in PDFs, so use left as the default alignment.
  document_root_data.SetTextAlign(ax::mojom::TextAlign::kLeft);
  // The PDF document cannot itself be modified.
  document_root_data.SetRestriction(ax::mojom::Restriction::kReadOnly);
  // TODO(b/319536234): Populate the title with the PDF's filename by
  // retrieving it from the Media App.
  document_root_data.SetNameChecked(
      base::StringPrintf("PDF document containing %zu pages", pages_.size()));

  std::vector<int32_t> child_ids(pages_.size());
  std::iota(std::begin(child_ids), std::end(child_ids), kStartPageAXNodeId);
  document_root_data.child_ids = child_ids;

  gfx::RectF document_location;
  for (auto const& [_, metadata] : page_metadata_) {
    document_location.Union(metadata.rect);
  }
  document_root_data.relative_bounds.bounds = document_location;
  document_root_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin,
                                     document_location.x());
  document_root_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin,
                                     document_location.y());

  ui::AXTreeUpdate document_update;
  document_update.root_id = document_root_data.id;
  std::vector<ui::AXNodeData> document_pages;
  document_pages.push_back(document_root_data);
  for (size_t page_index = 0; auto const& [page_id, pageTreeManager] : pages_) {
    ui::AXNodeData page_data;
    page_data.role = ax::mojom::Role::kRegion;
    base::CheckedNumeric<ui::AXNodeID> ax_page_id =
        page_index + kStartPageAXNodeId;
    if (!ax_page_id.AssignIfValid(&page_data.id)) {
      mojo::ReportBadMessage("Bad pages size from renderer.");
      return;
    }
    page_data.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                               true);
    page_data.SetRestriction(ax::mojom::Restriction::kReadOnly);
    // Page numbers are 1-indexed, so add one here.
    // TODO(b/319543924): Add a localized version of an accessible name.
    page_data.SetNameChecked(base::StringPrintf("Page %zu", page_index + 1u));
    // If the page doesn't exist, that means it hasn't been through OCR yet.
    if (pages_.contains(page_id) && pages_[page_id]->ax_tree() &&
        pages_[page_id]->GetRoot()) {
      page_data.AddChildTreeId(pages_[page_id]->GetTreeID());
      page_data.relative_bounds.bounds =
          pages_[page_id]->GetRoot()->data().relative_bounds.bounds;
    }
    document_pages.push_back(page_data);
    ++page_index;
  }
  if (document_root_data.child_ids.size() + 1u != document_pages.size()) {
    mojo::ReportBadMessage("Bad pages size from renderer.");
    return;
  }
  document_update.nodes.swap(document_pages);

  if (document_.ax_tree()) {
    if (!document_.ax_tree()->Unserialize(document_update)) {
      mojo::ReportBadMessage(document_.ax_tree()->error());
      return;
    }
  } else {
    document_update.has_tree_data = true;
    document_update.tree_data.tree_id = document_tree_id_;
    // TODO(b/319543924): Add a localized version of an accessible name.
    document_update.tree_data.title = "PDF document";
    document_.SetTree(std::make_unique<ui::AXTree>(document_update));
  }
}

void AXMediaAppUntrustedHandler::PushDirtyPage(
    const std::string& dirty_page_id) {
  // If the dirty page is already marked as dirty, move it to the back of the
  // queue.
  auto it =
      std::find(dirty_page_ids_.begin(), dirty_page_ids_.end(), dirty_page_id);
  if (it != dirty_page_ids_.end()) {
    std::rotate(it, it + 1, dirty_page_ids_.end());
    return;
  }
  dirty_page_ids_.push_back(dirty_page_id);
}

std::string AXMediaAppUntrustedHandler::PopDirtyPage() {
  if (dirty_page_ids_.empty()) {
    mojo::ReportBadMessage("PopDirtyPage() found no more dirty pages.");
  }

  auto dirty_page_id = dirty_page_ids_.front();
  dirty_page_ids_.pop_front();
  return dirty_page_id;
}

void AXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny() {
  if (!IsOcrServiceEnabled()) {
    return;
  }
  // If there are no more dirty pages, we can assume all pages have up-to-date
  // page locations. Update the document tree information to reflect that.
  if (dirty_page_ids_.empty()) {
    UpdateDocumentTree();
    return;
  }
  auto dirty_page_id = PopDirtyPage();
  // TODO(b/289012145): Refactor this code to support things happening
  // asynchronously - e.g. RequestBitmap will be async.
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
  complete_tree_update.tree_data.parent_tree_id = document_tree_id_;
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
    complete_tree_update.tree_data.tree_id = pages_[dirty_page_id]->GetTreeID();
    if (!pages_[dirty_page_id]->ax_tree() ||
        !pages_[dirty_page_id]->ax_tree()->Unserialize(complete_tree_update)) {
      mojo::ReportBadMessage(pages_[dirty_page_id]->ax_tree()->error());
      return;
    }
  }
  // Update the page location again - running the page through OCR overwrites
  // the previous AXTree it was given and thus the page location it was already
  // given in DocumentUpdated(). Restore it here.
  UpdatePageLocation(dirty_page_id, page_metadata_[dirty_page_id].rect);
  // TODO(b/289012145): Attach the page to the tree for the main PDF document.
  LOG(ERROR) << "WHAT onocr done?";
  OcrNextDirtyPageIfAny();
}

}  // namespace ash
