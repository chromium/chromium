// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <algorithm>
#include <iterator>
#include <numeric>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/strings/stringprintf.h"
#include "base/types/to_address.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/screen_ai_service_router.h"
#include "chrome/browser/screen_ai/screen_ai_service_router_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/gfx/geometry/rect_f.h"

#if defined(USE_AURA)
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_event.h"
#include "ui/aura/env.h"
#include "ui/gfx/geometry/point.h"
#endif  // defined(USE_AURA)

namespace ash {

// The ID used for the AX document root.
constexpr ui::AXNodeID kDocumentRootNodeId = 1;

// The first ID at which pages start. 0 is a special ID number reserved only for
// invalid nodes, and 1 is for the AX document root. So all pages begin at ID 2.
constexpr ui::AXNodeID kStartPageAXNodeId = 2;

// The maximum number of pages supported by the OCR service. This maximum is
// used both to validate the number of pages (untrusted data) coming from the
// MediaApp and manage resources (caps the number of pages stored at a time).
constexpr size_t kMaxPages = 10000;

namespace {

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
      base::to_address(browser_context_))
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
          base::to_address(browser_context_));
  service_router->BindScreenAIAnnotator(
      screen_ai_annotator_.BindNewPipeAndPassReceiver());
  OcrNextDirtyPageIfAny();
  if (UNLIKELY(media_app_)) {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    media_app_->OcrServiceEnabledChanged(true);
  } else {
    // TODO(b/301007305): Implement `OcrServiceEnabledChanged` in the Media App.
  }
}

bool AXMediaAppUntrustedHandler::IsAccessibilityEnabled() const {
  return base::FeatureList::IsEnabled(ash::features::kMediaAppPdfA11yOcr) &&
         accessibility_state_utils::IsScreenReaderEnabled();
}

void AXMediaAppUntrustedHandler::PerformAction(
    const ui::AXActionData& action_data) {
  if (!document_.GetRoot()) {
    return;
  }
  CHECK(document_.ax_tree());
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
      return;  // Irrelevant for Backlight.
    case ax::mojom::Action::kScrollBackward:
    case ax::mojom::Action::kScrollUp: {
      float y_min = static_cast<float>(document_.GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollYMin));
      viewport_box_.set_y(
          std::max(viewport_box_.y() - viewport_box_.height(), y_min));
      if (UNLIKELY(media_app_)) {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollForward:
    case ax::mojom::Action::kScrollDown: {
      float y_max = static_cast<float>(document_.GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollYMax));
      viewport_box_.set_y(
          std::min(viewport_box_.y() + viewport_box_.height(), y_max));
      if (UNLIKELY(media_app_)) {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollLeft: {
      float x_min = static_cast<float>(document_.GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollXMin));
      viewport_box_.set_x(
          std::max(viewport_box_.x() - viewport_box_.width(), x_min));
      if (UNLIKELY(media_app_)) {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollRight: {
      float x_max = static_cast<float>(document_.GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollXMax));
      viewport_box_.set_x(
          std::min(viewport_box_.x() + viewport_box_.width(), x_max));
      if (UNLIKELY(media_app_)) {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollToMakeVisible: {
      if (!media_app_) {
        CHECK_NE(action_data.target_tree_id, ui::AXTreeIDUnknown());
      } else {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
      }
      CHECK_NE(action_data.target_node_id, ui::kInvalidAXNodeID);
      CHECK_EQ(pages_.size(), document_.GetRoot()->GetUnignoredChildCount());
      for (size_t page_index = 0u; const auto& page : pages_) {
        const std::unique_ptr<ui::AXTreeManager>& page_manager = page.second;
        if (page_manager->GetTreeID() != action_data.target_tree_id) {
          ++page_index;
          continue;
        }
        ui::AXNode* target_node =
            page_manager->GetNode(action_data.target_node_id);
        if (!target_node) {
          break;
        }
        CHECK(page_manager->ax_tree());
        // Passing an empty `RectF` for the node bounds will initialize it
        // automatically to `target_node->data().relative_bounds.bounds`.
        gfx::RectF global_bounds =
            page_manager->ax_tree()->RelativeToTreeBounds(
                target_node, /*node_bounds=*/gfx::RectF());
        global_bounds.Offset(document_.GetRoot()
                                 ->GetUnignoredChildAtIndex(page_index)
                                 ->data()
                                 .relative_bounds.bounds.OffsetFromOrigin());
        if (global_bounds.x() < viewport_box_.x()) {
          viewport_box_.set_x(global_bounds.x());
        } else if (global_bounds.right() > viewport_box_.right()) {
          viewport_box_.set_x(
              std::max(0.0f, global_bounds.right() - viewport_box_.width()));
        }
        if (global_bounds.y() < viewport_box_.y()) {
          viewport_box_.set_y(global_bounds.y());
        } else if (global_bounds.bottom() > viewport_box_.bottom()) {
          viewport_box_.set_y(
              std::max(0.0f, global_bounds.bottom() - viewport_box_.height()));
        }
        break;
      }
      if (UNLIKELY(media_app_)) {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollToPoint:
      NOTIMPLEMENTED();
      return;
      // Used only on Android.
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
      NOTIMPLEMENTED();
      return;
  }
}

void AXMediaAppUntrustedHandler::OnAXModeAdded(ui::AXMode mode) {
  if (UNLIKELY(media_app_)) {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    media_app_->AccessibilityEnabledChanged(
        accessibility_state_utils::IsScreenReaderEnabled());
  } else {
    // TODO(b/301007305): Implement `AccessibilityEnabledChanged` in the Media
    // App.
  }
}

void AXMediaAppUntrustedHandler::PageMetadataUpdated(
    const std::vector<ash::media_app_ui::mojom::PageMetadataPtr>
        page_metadata) {
  if (page_metadata.empty()) {
    mojo::ReportBadMessage(
        "`PageMetadataUpdated()` called with no page metadata");
    return;
  }

  const size_t num_pages = std::min(page_metadata.size(), kMaxPages);
  // If `page_metadata_` is empty, this is the first load of the PDF.
  const bool is_first_load = page_metadata_.empty();

  if (is_first_load) {
    for (size_t i = 0; i < num_pages; ++i) {
      AXMediaAppPageMetadata data;
      // The page IDs will never change, so this should be the only place that
      // updates them.
      data.id = page_metadata.at(i)->id;
      if (page_metadata_.contains(data.id)) {
        mojo::ReportBadMessage(
            "`PageMetadataUpdated()` called with pages with duplicate page "
            "IDs");
        return;
      }
      page_metadata_.insert(std::pair(data.id, data));
      PushDirtyPage(data.id);
    }
    // Only one page goes through OCR at a time, so start the process here.
    OcrNextDirtyPageIfAny();
  }

  // Update all page numbers and rects.
  std::set<const std::string> page_id_updated;
  for (size_t i = 0; i < page_metadata.size(); ++i) {
    const std::string& page_id = page_metadata.at(i)->id;
    if (ReportIfNonExistentPageId("PageMetadataUpdated()", page_id,
                                  page_metadata_)) {
      return;
    }
    page_metadata_.at(page_id).page_num = i + 1;  // 1-indexed.
    page_metadata_.at(page_id).rect = page_metadata.at(i)->rect;
    // Page location can only be set after the corresponding `pages_`
    // `AXTreeManager` entry has been created, so don't update it for first
    // load.
    if (!is_first_load) {
      page_id_updated.insert(page_id);
      UpdatePageLocation(page_id, page_metadata.at(i)->rect);
      SendAXTreeToAccessibilityService(*pages_.at(page_id),
                                       *page_serializers_.at(page_id));
    }
  }

  // If this is the "first load", there could be no deleted pages.
  if (is_first_load) {
    return;
  }

  // If a page was missing from `page_metadata` (its location was not updated),
  // then that means it got deleted. Set its page number to 0.
  for (auto const& [page_id, _] : page_metadata_) {
    if (!page_id_updated.contains(page_id)) {
      // Since `pages_` and `page_metadata_` are both populated from untrusted
      // code, mitigate potential security issues by never mutating the size of
      // these twocontainers. So when a page is 'deleted' by the user, keep it
      // in memory.
      page_metadata_[page_id].page_num = 0;
    }
  }
  UpdateDocumentTree();
}

void AXMediaAppUntrustedHandler::PageContentsUpdated(
    const std::string& dirty_page_id) {
  if (!page_metadata_.contains(dirty_page_id)) {
    mojo::ReportBadMessage(
        "`PageContentsUpdated()` called with a non-existent page ID");
    return;
  }
  PushDirtyPage(dirty_page_id);
  OcrNextDirtyPageIfAny();
}

content::WebContents* AXMediaAppUntrustedHandler::GetMediaAppWebContents()
    const {
  Profile* profile =
      Profile::FromBrowserContext(base::to_address(browser_context_));
  Browser* browser = chrome::FindLastActiveWithProfile(profile);
  if (!browser) {
    return nullptr;
  }
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  CHECK(web_contents);
  return web_contents;
}

content::RenderFrameHost*
AXMediaAppUntrustedHandler::GetMediaAppRenderFrameHost() const {
  content::WebContents* web_contents = GetMediaAppWebContents();
  return web_contents ? web_contents->GetPrimaryMainFrame() : nullptr;
}

ui::AXNodeID AXMediaAppUntrustedHandler::GetMediaAppRootNodeID() const {
  content::WebContents* web_contents = GetMediaAppWebContents();
  if (!web_contents) {
    return ui::kInvalidAXNodeID;
  }
  // Search for the first <canvas> element.
  for (ui::AXNode* node = web_contents->GetAccessibilityRootNode(); node;
       node = node->GetNextUnignoredInTreeOrder()) {
    if (node->GetRole() == ax::mojom::Role::kCanvas) {
      return node->id();
    }
  }
  return ui::kInvalidAXNodeID;
}

void AXMediaAppUntrustedHandler::SendAXTreeToAccessibilityService(
    const ui::AXTreeManager& manager,
    TreeSerializer& serializer) {
  CHECK(manager.GetRoot());
  ui::AXTreeUpdate update;
  if (!serializer.SerializeChanges(manager.GetRoot(), &update)) {
    NOTREACHED_NORETURN() << "Failure to serialize should have already caused "
                             "the process to crash due to the `crash_on_error` "
                             "in `AXTreeSerializer` constructor call.";
  }
  if (pending_serialized_updates_for_testing_) {
    ui::AXTreeUpdate simplified_update = update;
    simplified_update.tree_data = ui::AXTreeData();
    pending_serialized_updates_for_testing_->push_back(
        std::move(simplified_update));
  }
#if defined(USE_AURA)
  auto* event_router = extensions::AutomationEventRouter::GetInstance();
  CHECK(event_router);
  const gfx::Point& mouse_location =
      aura::Env::GetInstance()->last_mouse_location();
  event_router->DispatchAccessibilityEvents(
      update.tree_data.tree_id, {update}, mouse_location,
      {ui::AXEvent(update.root_id, ax::mojom::Event::kLayoutComplete,
                   ax::mojom::EventFrom::kNone)});
#endif  // defined(USE_AURA)
}

void AXMediaAppUntrustedHandler::ViewportUpdated(const gfx::RectF& viewport_box,
                                                 float scale_factor) {
  // TODO(nektar): Use scale factor to convert to device independent pixels.
  viewport_box_ = viewport_box;
  if (!document_.GetRoot()) {
    return;
  }
  CHECK(document_.ax_tree());
  ui::AXNodeData document_root_data = document_.GetRoot()->data();
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollX,
      base::checked_cast<int32_t>(viewport_box_.x()));
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollXMax,
      base::checked_cast<int32_t>(
          document_root_data.relative_bounds.bounds.width() -
          viewport_box_.width()));
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollY,
      base::checked_cast<int32_t>(viewport_box_.y()));
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollYMax,
      base::checked_cast<int32_t>(
          document_root_data.relative_bounds.bounds.height() -
          viewport_box_.height()));
  ui::AXTreeUpdate document_update;
  document_update.root_id = document_root_data.id;
  document_update.nodes = {document_root_data};
  if (!document_.ax_tree()->Unserialize(document_update)) {
    mojo::ReportBadMessage(document_.ax_tree()->error());
  }
}

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
  CHECK(tree->root());
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
  for (const auto& [_, page] : page_metadata_) {
    if (page.page_num != 0u) {  // Not deleted page.
      document_location.Union(page.rect);
    }
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

  std::map<const uint32_t, const AXMediaAppPageMetadata> pages_in_order;
  std::transform(
      std::begin(page_metadata_), std::end(page_metadata_),
      std::inserter(pages_in_order, std::begin(pages_in_order)),
      [](const std::pair<const std::string, const AXMediaAppPageMetadata>
             page) { return std::pair(page.second.page_num, page.second); });
  for (size_t page_index = 0;
       const auto& [page_num, page_metadata] : pages_in_order) {
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
    // TODO(b/319543924): Add a localized version of an accessible name.
    page_data.SetNameChecked(base::StringPrintf("Page %u", page_num));
    const std::string page_id = page_metadata.id;
    // If the page doesn't exist, that means it hasn't been through OCR yet.
    if (pages_.contains(page_id) && pages_.at(page_id)->ax_tree() &&
        pages_.at(page_id)->GetRoot()) {
      page_data.AddChildTreeId(pages_.at(page_id)->GetTreeID());
      page_data.relative_bounds.bounds =
          pages_.at(page_id)->GetRoot()->data().relative_bounds.bounds;
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
    if (auto* render_frame_host = GetMediaAppRenderFrameHost()) {
      document_update.tree_data.parent_tree_id =
          render_frame_host->GetAXTreeID();
    }
    document_update.tree_data.tree_id = document_tree_id_;
    // TODO(b/319543924): Add a localized version of an accessible name.
    document_update.tree_data.title = "PDF document";
    auto document_tree =
        std::make_unique<ui::AXSerializableTree>(document_update);
    document_source_ =
        base::WrapUnique<TreeSource>(document_tree->CreateTreeSource());
    document_serializer_ = std::make_unique<TreeSerializer>(
        document_source_.get(), /* crash_on_error */ true);
    document_.SetTree(std::move(document_tree));
    StitchDocumentTree();
  }
  SendAXTreeToAccessibilityService(document_, *document_serializer_);
}

void AXMediaAppUntrustedHandler::StitchDocumentTree() {
  content::RenderFrameHost* render_frame_host = GetMediaAppRenderFrameHost();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    return;
  }
  ui::AXNodeID media_app_root_node_id = GetMediaAppRootNodeID();
  if (media_app_root_node_id == ui::kInvalidAXNodeID) {
    return;
  }
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kStitchChildTree;
  CHECK(document_.ax_tree());
  action_data.target_tree_id = document_.GetParentTreeID();
  action_data.target_node_id = media_app_root_node_id;
  action_data.child_tree_id = document_.GetTreeID();
  render_frame_host->AccessibilityPerformAction(action_data);
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
    mojo::ReportBadMessage("`PopDirtyPage()` found no more dirty pages.");
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
  // asynchronously - i.e. `RequestBitmap` will be async.
  if (UNLIKELY(media_app_)) {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    // TODO(b/303133098): Change this as soon as `RequestBitmap` becomes
    // available by the Backlight team.
    SkBitmap page_bitmap = media_app_->RequestBitmap(dirty_page_id);
    screen_ai_annotator_->PerformOcrAndReturnAXTreeUpdate(
        page_bitmap,
        base::BindOnce(&AXMediaAppUntrustedHandler::OnPageOcred,
                       weak_ptr_factory_.GetWeakPtr(), dirty_page_id));
  } else {
    // TODO(b/301007305): Implement `RequestBitmap` in the Media App.
  }
}

void AXMediaAppUntrustedHandler::OnPageOcred(
    const std::string& dirty_page_id,
    const ui::AXTreeUpdate& tree_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tree_update.has_tree_data ||
      // TODO(b/319536234): Validate tree ID.
      // ui::AXTreeIDUnknown() == tree_update.tree_data.tree_id ||
      ui::kInvalidAXNodeID == tree_update.root_id) {
    mojo::ReportBadMessage("OnPageOcred() bad tree update from Screen AI.");
    return;
  }
  ui::AXTreeUpdate complete_tree_update = tree_update;
  complete_tree_update.tree_data.parent_tree_id = document_tree_id_;
  if (ReportIfNonExistentPageId("OnPageOcred()", dirty_page_id,
                                page_metadata_)) {
    return;
  }
  if (!pages_.contains(dirty_page_id)) {
    // Add a newly generated tree id to the tree update so that the new
    // AXSerializableTree that's generated as a non-empty tree id.
    CHECK(complete_tree_update.has_tree_data);
    CHECK(complete_tree_update.tree_data.tree_id.type() ==
          ax::mojom::AXTreeIDType::kUnknown)
        << "Not expected to be set yet.";
    complete_tree_update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    auto page_tree =
        std::make_unique<ui::AXSerializableTree>(complete_tree_update);
    page_sources_[dirty_page_id] =
        base::WrapUnique<TreeSource>(page_tree->CreateTreeSource());
    page_serializers_[dirty_page_id] = std::make_unique<TreeSerializer>(
        page_sources_[dirty_page_id].get(), /* crash_on_error */ true);
    pages_[dirty_page_id] =
        std::make_unique<ui::AXTreeManager>(std::move(page_tree));
    UpdatePageLocation(dirty_page_id, page_metadata_[dirty_page_id].rect);
  } else {
    complete_tree_update.tree_data.tree_id = pages_[dirty_page_id]->GetTreeID();
    if (!pages_[dirty_page_id]->ax_tree() ||
        !pages_[dirty_page_id]->ax_tree()->Unserialize(complete_tree_update)) {
      mojo::ReportBadMessage(pages_[dirty_page_id]->ax_tree()->error());
      return;
    }
  }

  CHECK(pages_[dirty_page_id]->GetTreeID().type() !=
        ax::mojom::AXTreeIDType::kUnknown);

  // Update the page location again - running the page through OCR overwrites
  // the previous `AXTree` it was given and thus the page location it was
  // already given in `PageMetadataUpdated()`. Restore it here.
  UpdatePageLocation(dirty_page_id, page_metadata_[dirty_page_id].rect);
  SendAXTreeToAccessibilityService(*pages_[dirty_page_id],
                                   *page_serializers_[dirty_page_id]);
  OcrNextDirtyPageIfAny();
}

}  // namespace ash
