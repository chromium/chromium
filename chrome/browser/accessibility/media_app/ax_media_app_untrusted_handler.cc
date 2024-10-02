// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/media_app/ax_media_app_untrusted_handler.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/types/to_address.h"
#include "chrome/browser/accessibility/accessibility_state_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/screen_ai/public/optical_character_recognizer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/screen_ai/public/cpp/metrics.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_action_handler_registry.h"
#include "ui/accessibility/ax_enum_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_manager.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_updates_and_events.h"
#include "ui/accessibility/platform/inspect/ax_inspect.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/strings/grit/auto_image_annotation_strings.h"

#if defined(USE_AURA)
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "ui/accessibility/ax_event.h"
#include "ui/aura/env.h"
#include "ui/gfx/geometry/point.h"
#endif  // defined(USE_AURA)

namespace ash {

// The ID used for the AX document root.
constexpr ui::AXNodeID kDocumentRootNodeId = 1;

// The first ID at which pages start. Zero is a special ID number reserved only
// for invalid nodes, one is for the AX document root. Status nodes start at
// `kMaxPages` (see `CreateStatusNodesWithLandmark`), so that they will have no
// chance of conflicting with page IDs. All pages begin at ID three.
constexpr ui::AXNodeID kStartPageAXNodeId = kDocumentRootNodeId + 1;

// The maximum number of pages supported by the OCR service. This maximum is
// used both to validate the number of pages (untrusted data) coming from the
// MediaApp, and manage resources (i.e. caps the number of pages stored at a
// time).
constexpr size_t kMaxPages = 10000u;

// In the case of large PDFs, pages are OCRed in patches in order to improve the
// user experience.
constexpr size_t kMaxPagesPerBatch = 20u;

AXMediaAppUntrustedHandler::AXMediaAppUntrustedHandler(
    content::BrowserContext& context,
    gfx::NativeWindow native_window,
    mojo::PendingRemote<media_app_ui::mojom::OcrUntrustedPage> page)
    : browser_context_(context),
      native_window_(native_window),
      media_app_page_(std::move(page)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (auto* accessibility_manager = ash::AccessibilityManager::Get()) {
    // Unretained is safe because `this` owns the subscription.
    accessibility_status_subscription_ =
        accessibility_manager->RegisterCallback(base::BindRepeating(
            &AXMediaAppUntrustedHandler::OnAshAccessibilityModeChanged,
            base::Unretained(this)));
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  ax_mode_observation_.Observe(&ui::AXPlatform::GetInstance());
#endif
  if (IsAccessibilityEnabled()) {
    ToggleAccessibilityState();
  }
}

AXMediaAppUntrustedHandler::~AXMediaAppUntrustedHandler() {
  if (!start_reading_time_.is_null() && !latest_reading_time_.is_null() &&
      start_reading_time_ < latest_reading_time_) {
    // Record time difference between `start_reading_time_` and
    // `latest_reading_time_`. This is considered as active time.
    base::TimeDelta active_time = latest_reading_time_ - start_reading_time_;
    base::UmaHistogramLongTimes100("Accessibility.PdfOcr.MediaApp.ActiveTime",
                                   active_time);
  }

  if (page_metadata_.size()) {
    const float reading_progression_in_ratio =
        static_cast<float>(greatest_visited_page_number_) /
        page_metadata_.size();
    DCHECK_LE(reading_progression_in_ratio, 1.0f);
    base::UmaHistogramPercentage(
        "Accessibility.PdfOcr.MediaApp.PercentageReadingProgression",
        reading_progression_in_ratio * 100);
  }

  RemoveAllAXTreesFromAccessibilityService();
}

void AXMediaAppUntrustedHandler::OnOCRServiceInitialized(bool is_successful) {
  if (!is_successful) [[unlikely]] {
    // Regardless of the previous `ocr_status_`, a failure should always stop
    // any use of the Service, overwriting the document contents with the
    // appropriate error message, in order to prevent further edits to the
    // content.
    ocr_status_ = OcrStatus::kInitializationFailed;
    ShowOcrServiceFailedToInitializeMessage();
    return;
  }
  // The OCR Service might have been initialized before, since accessibility
  // could have been turned off temporarily and then back on. During such
  // events, we release the memory used by the Service, but do not change
  // `ocr_status_`.
  if (ocr_status_ == OcrStatus::kUninitialized) {
    ocr_status_ = OcrStatus::kInProgressWithNoTextExtractedYet;
  }
  if (media_app_) [[unlikely]] {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    media_app_->OcrServiceEnabledChanged(is_successful);
  }
  if (!dirty_page_ids_.empty()) {
    OcrNextDirtyPageIfAny();
  }
}

bool AXMediaAppUntrustedHandler::IsAccessibilityEnabled() const {
  return accessibility_state_utils::IsScreenReaderEnabled() ||
         accessibility_state_utils::IsSelectToSpeakEnabled();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void AXMediaAppUntrustedHandler::OnAshAccessibilityModeChanged(
    const ash::AccessibilityStatusEventDetails& details) {
  if (details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSpokenFeedback ||
      details.notification_type ==
          ash::AccessibilityNotificationType::kToggleSelectToSpeak) {
    ToggleAccessibilityState();
  }
  if (media_app_) [[unlikely]] {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    media_app_->AccessibilityEnabledChanged(IsAccessibilityEnabled());
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void AXMediaAppUntrustedHandler::OnAXModeAdded(ui::AXMode mode) {
  ToggleAccessibilityState();
  if (media_app_) [[unlikely]] {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    media_app_->AccessibilityEnabledChanged(IsAccessibilityEnabled());
    return;
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

void AXMediaAppUntrustedHandler::PerformAction(
    const ui::AXActionData& action_data) {
  if (!IsAccessibilityEnabled()) {
    return;
  }
  if (!document_ || !document_->GetRoot()) {
    return;
  }
  DCHECK(document_->ax_tree());
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
      float y_min = static_cast<float>(document_->GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollYMin));
      viewport_box_.set_y(
          std::max(viewport_box_.y() - viewport_box_.height(), y_min));
      if (media_app_) [[unlikely]] {
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
      float y_max = static_cast<float>(document_->GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollYMax));
      viewport_box_.set_y(
          std::min(viewport_box_.y() + viewport_box_.height(), y_max));
      if (media_app_) [[unlikely]] {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollLeft: {
      float x_min = static_cast<float>(document_->GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollXMin));
      viewport_box_.set_x(
          std::max(viewport_box_.x() - viewport_box_.width(), x_min));
      if (media_app_) [[unlikely]] {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
        media_app_->SetViewport(viewport_box_);
      } else {
        media_app_page_->SetViewport(viewport_box_);
      }
      return;
    }
    case ax::mojom::Action::kScrollRight: {
      float x_max = static_cast<float>(document_->GetRoot()->GetIntAttribute(
          ax::mojom::IntAttribute::kScrollXMax));
      viewport_box_.set_x(
          std::min(viewport_box_.x() + viewport_box_.width(), x_max));
      if (media_app_) [[unlikely]] {
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
        DCHECK_NE(action_data.target_tree_id, ui::AXTreeIDUnknown());
      } else {
        // `media_app_` is only used for testing.
        CHECK_IS_TEST();
      }

      // that the user starts navigating content and the most recent time that
      // the user navigates it as well.
      if (start_reading_time_.is_null()) {
        start_reading_time_ = base::TimeTicks::Now();
        latest_reading_time_ = start_reading_time_;
      } else {
        // Keep tracking of most recent time that the user navigates content.
        latest_reading_time_ = base::TimeTicks::Now();
      }

      DCHECK_NE(action_data.target_node_id, ui::kInvalidAXNodeID);
      // Some pages might not be in the document yet, because of page
      // batching.
      DCHECK_GE(pages_.size(), document_->GetRoot()->GetUnignoredChildCount() -
                                   (has_landmark_node_ ? 1u : 0u) -
                                   (has_postamble_page_ ? 1u : 0u));
      for (const auto& page : pages_) {
        const std::unique_ptr<ui::AXTreeManager>& page_manager = page.second;
        if (page_manager->GetTreeID() != action_data.target_tree_id) {
          continue;
        }
        ui::AXNode* target_node =
            page_manager->GetNode(action_data.target_node_id);
        if (!target_node) {
          break;
        }
        DCHECK(page_manager->ax_tree());

        if (page_metadata_.contains(page.first) &&
            page_metadata_.at(page.first).page_num >
                greatest_visited_page_number_) {
          greatest_visited_page_number_ =
              page_metadata_.at(page.first).page_num;
        }

        auto child_iter = target_node->UnignoredChildrenBegin();
        for (; child_iter != target_node->UnignoredChildrenEnd();
             ++child_iter) {
          const std::optional<ui::AXTreeID> child_tree_id =
              target_node->data().GetChildTreeID();
          if (child_tree_id && *child_tree_id == action_data.target_tree_id) {
            break;
          }
        }
        size_t page_index =
            std::distance(target_node->UnignoredChildrenBegin(), child_iter);
        // Passing an empty `RectF` for the node bounds will initialize it
        // automatically to `target_node->data().relative_bounds.bounds`.
        gfx::RectF global_bounds =
            page_manager->ax_tree()->RelativeToTreeBounds(
                target_node, /*node_bounds=*/gfx::RectF());
        global_bounds.Offset(document_->GetRoot()
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
      if (media_app_) [[unlikely]] {
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

void AXMediaAppUntrustedHandler::AccessibilityEventReceived(
    const ui::AXUpdatesAndEvents& details) {
  if (!document_ || !GetMediaAppWebContents() ||
      !GetMediaAppWebContents()
           ->IsDocumentOnLoadCompletedInPrimaryMainFrame()) {
    return;
  }
  // Accessibility for the Media App's 'RenderFrameHost' may not become ready as
  // soon as the assistive software is turned on. Hence we use any event on the
  // host as a proxy for determining whether the host's accessibility is ready.
  if (const ui::AXNode* parent_node = document_->GetParentNodeFromParentTree();
      !parent_node || !parent_node->data().HasChildTreeID()) {
    StitchDocumentTree();
    DCHECK(document_serializer_);
    // It turns out that the document serializer does not send the updated tree
    // data containing the Media App's render frame host's tree ID, so stitching
    // won't work unless we first reset it.
    document_serializer_->Reset();
    SendAXTreeToAccessibilityService(*document_, *document_serializer_);
  } else {
    StopWatchingForAccessibilityEvents();
  }
}

void AXMediaAppUntrustedHandler::PageMetadataUpdated(
    const std::vector<ash::media_app_ui::mojom::PageMetadataPtr>
        page_metadata) {
  // `mojo::GetBadMessageCallback` only works when in a non-test environment.
  base::AutoReset<std::optional<mojo::ReportBadMessageCallback>> call_resetter(
      &bad_message_callback_,
      !media_app_ && mojo::IsInMessageDispatch()
          ? std::make_optional(mojo::GetBadMessageCallback())
          : std::nullopt);
  if (page_metadata.empty()) {
    mojo::ReportBadMessage(
        "`PageMetadataUpdated()` called with no page metadata");
    return;
  }

  const size_t num_pages = std::min(page_metadata.size(), kMaxPages);
  // If `page_metadata_` is empty, this is the first load of the PDF.
  const bool is_first_load = page_metadata_.empty();

  if (is_first_load) {
    base::UmaHistogramBoolean("Accessibility.PdfOcr.MediaApp.PdfLoaded", true);
    for (size_t i = 0; i < num_pages; ++i) {
      if (page_metadata_.contains(page_metadata.at(i)->id)) {
        mojo::ReportBadMessage(
            "`PageMetadataUpdated()` called with pages with duplicate page "
            "IDs");
        return;
      }
      AXMediaAppPageMetadata metadata;
      // The page IDs will never change, so this should be the only place that
      // updates them.
      metadata.id = page_metadata.at(i)->id;
      page_metadata_.insert(std::pair(metadata.id, metadata));
      PushDirtyPage(metadata.id);
    }
    // Only one page goes through OCR at a time, so start the process here.
    OcrNextDirtyPageIfAny();
    ShowDocumentTree();
  }

  // Update all page numbers and rects.
  std::set<std::string> page_id_updated;
  for (size_t i = 0; i < page_metadata.size(); ++i) {
    const std::string& page_id = page_metadata.at(i)->id;
    if (HasRendererTerminatedDueToBadPageId("PageMetadataUpdated", page_id)) {
      return;
    }
    page_metadata_.at(page_id).page_num = i + 1;  // 1-indexed.
    page_metadata_.at(page_id).rect = page_metadata.at(i)->rect;
    // Page location can only be set after the corresponding `pages_`
    // `AXTreeManager` entry has been created.
    if (pages_.contains(page_id)) {
      UpdatePageLocation(page_id, page_metadata.at(i)->rect);
      SendAXTreeToAccessibilityService(*pages_.at(page_id),
                                       *page_serializers_.at(page_id));
    }
    page_id_updated.insert(page_id);
  }

  // If this is the "first load", there could be no deleted pages.
  if (is_first_load) {
    return;
  }

  // If a page was missing from `page_metadata` (its location was not updated),
  // then that means it got deleted. Set its page number to 0.
  for (auto& [page_id, page_info] : page_metadata_) {
    if (!page_id_updated.contains(page_id)) {
      // Since `pages_` and `page_metadata_` are both populated from untrusted
      // code, mitigate potential security issues by never mutating the size of
      // these two containers. So when a page is 'deleted' by the user, keep it
      // in memory. Also, no need to update `greatest_visited_page_number_` as
      // `page_metadata_` still keeps the deleted page.
      page_info.page_num = 0;
    }
  }
  ShowDocumentTree();
}

void AXMediaAppUntrustedHandler::PageContentsUpdated(
    const std::string& dirty_page_id) {
  // `mojo::GetBadMessageCallback` only works when in a non-test environment.
  base::AutoReset<std::optional<mojo::ReportBadMessageCallback>> call_resetter(
      &bad_message_callback_,
      !media_app_ && mojo::IsInMessageDispatch()
          ? std::make_optional(mojo::GetBadMessageCallback())
          : std::nullopt);
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
  DCHECK(web_contents);
  return web_contents;
}

content::RenderFrameHost*
AXMediaAppUntrustedHandler::GetMediaAppRenderFrameHost() const {
  content::WebContents* web_contents = GetMediaAppWebContents();
  content::RenderFrameHost* media_app_render_frame_host =
      web_contents->GetPrimaryMainFrame();
  // Return the last inner iframe.
  web_contents->ForEachRenderFrameHost(
      [&media_app_render_frame_host](content::RenderFrameHost* rfh) {
        media_app_render_frame_host = rfh;
      });
  return media_app_render_frame_host;
}

size_t AXMediaAppUntrustedHandler::ComputePagesPerBatch() const {
  DCHECK_LE(min_pages_per_batch_, kMaxPagesPerBatch);
  size_t page_count = page_metadata_.size();
  return std::clamp<size_t>(page_count * 0.1, min_pages_per_batch_,
                            kMaxPagesPerBatch);
}

std::vector<ui::AXNodeData>
AXMediaAppUntrustedHandler::CreateStatusNodesWithLandmark() const {
  ui::AXNodeData banner;
  banner.role = ax::mojom::Role::kBanner;
  banner.id = kMaxPages;
  banner.relative_bounds.bounds = gfx::RectF(-1, -1, 1, 1);
  banner.relative_bounds.offset_container_id = kDocumentRootNodeId;
  banner.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "div");
  banner.SetTextAlign(ax::mojom::TextAlign::kLeft);
  banner.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject,
                          true);
  banner.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                          true);
  banner.AddBoolAttribute(ax::mojom::BoolAttribute::kHasAriaAttribute, true);

  ui::AXNodeData status;
  status.role = ax::mojom::Role::kStatus;
  status.id = banner.id + 1;
  status.relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);
  status.relative_bounds.offset_container_id = banner.id;
  status.AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveRelevant,
                            "additions text");
  status.AddStringAttribute(ax::mojom::StringAttribute::kContainerLiveStatus,
                            "polite");
  status.AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                            "additions text");
  status.AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus, "polite");
  status.AddStringAttribute(ax::mojom::StringAttribute::kHtmlTag, "div");
  status.AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveAtomic, true);
  status.AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveBusy, false);
  status.AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
  status.SetTextAlign(ax::mojom::TextAlign::kLeft);
  status.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                          true);
  status.AddBoolAttribute(ax::mojom::BoolAttribute::kHasAriaAttribute, true);
  banner.child_ids = {status.id};

  ui::AXNodeData static_text;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.id = status.id + 1;
  static_text.relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);
  static_text.relative_bounds.offset_container_id = status.id;
  static_text.AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveRelevant, "additions text");
  static_text.AddStringAttribute(
      ax::mojom::StringAttribute::kContainerLiveStatus, "polite");
  static_text.AddStringAttribute(ax::mojom::StringAttribute::kLiveRelevant,
                                 "additions text");
  static_text.AddStringAttribute(ax::mojom::StringAttribute::kLiveStatus,
                                 "polite");
  static_text.AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveAtomic,
                               true);
  static_text.AddBoolAttribute(ax::mojom::BoolAttribute::kContainerLiveBusy,
                               false);
  static_text.AddBoolAttribute(ax::mojom::BoolAttribute::kLiveAtomic, true);
  static_text.SetTextAlign(ax::mojom::TextAlign::kLeft);
  static_text.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  status.child_ids = {static_text.id};

  ui::AXNodeData inline_text_box;
  inline_text_box.role = ax::mojom::Role::kInlineTextBox;
  inline_text_box.id = static_text.id + 1;
  inline_text_box.relative_bounds.bounds = gfx::RectF(0, 0, 1, 1);
  inline_text_box.relative_bounds.offset_container_id = static_text.id;
  inline_text_box.SetTextAlign(ax::mojom::TextAlign::kLeft);
  static_text.child_ids = {inline_text_box.id};

  std::string message;
  switch (ocr_status_) {
    case OcrStatus::kUninitialized:
      return {};
    case OcrStatus::kInitializationFailed:
      message = l10n_util::GetStringUTF8(IDS_PDF_OCR_FEATURE_ALERT);
      break;
    case OcrStatus::kInProgressWithNoTextExtractedYet:
    case OcrStatus::kInProgressWithTextExtracted:
      message = l10n_util::GetStringUTF8(IDS_PDF_OCR_IN_PROGRESS);
      break;
    case OcrStatus::kCompletedWithNoTextExtracted:
      message = l10n_util::GetStringUTF8(IDS_PDF_OCR_NO_RESULT);
      break;
    case OcrStatus::kCompletedWithTextExtracted:
      message = l10n_util::GetStringUTF8(IDS_PDF_OCR_COMPLETED);
      break;
  }

  static_text.SetNameChecked(message);
  inline_text_box.SetNameChecked(message);

  return {banner, status, static_text, inline_text_box};
}

std::vector<ui::AXNodeData> AXMediaAppUntrustedHandler::CreatePostamblePage()
    const {
  ui::AXNodeData page;
  page.id = kMaxPages + 4;
  page.role = ax::mojom::Role::kRegion;
  page.SetRestriction(ax::mojom::Restriction::kReadOnly);
  page.AddBoolAttribute(ax::mojom::BoolAttribute::kIsPageBreakingObject, true);

  ui::AXNodeData paragraph;
  paragraph.id = page.id + 1;
  paragraph.role = ax::mojom::Role::kParagraph;
  paragraph.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                             true);
  page.child_ids = {paragraph.id};

  const std::string postamble_message =
      l10n_util::GetStringUTF8(IDS_PDF_OCR_POSTAMBLE_PAGE);

  ui::AXNodeData static_text;
  static_text.id = paragraph.id + 1;
  static_text.role = ax::mojom::Role::kStaticText;
  static_text.SetRestriction(ax::mojom::Restriction::kReadOnly);
  static_text.SetNameChecked(postamble_message);
  paragraph.child_ids = {static_text.id};

  ui::AXNodeData inline_text_box;
  inline_text_box.id = static_text.id + 1;
  inline_text_box.role = ax::mojom::Role::kInlineTextBox;
  inline_text_box.SetRestriction(ax::mojom::Restriction::kReadOnly);
  inline_text_box.SetNameChecked(postamble_message);
  static_text.child_ids = {inline_text_box.id};

  return {page, paragraph, static_text, inline_text_box};
}

void AXMediaAppUntrustedHandler::ToggleAccessibilityState() {
  if (IsAccessibilityEnabled()) {
    StartWatchingForAccessibilityEvents();
    SendAllAXTreesToAccessibilityService();
    InitializeOcrService();
  } else {
    StopWatchingForAccessibilityEvents();
    DisconnectFromOcrService();
    RemoveAllAXTreesFromAccessibilityService();
  }
  media_app_page_->SetPdfOcrEnabled(IsAccessibilityEnabled());
}

void AXMediaAppUntrustedHandler::SendAllAXTreesToAccessibilityService() {
  RemoveDocumentTree();
  switch (ocr_status_) {
    case OcrStatus::kUninitialized:
      return;
    case OcrStatus::kInitializationFailed:
      ShowOcrServiceFailedToInitializeMessage();
      break;
    case OcrStatus::kInProgressWithNoTextExtractedYet:
    case OcrStatus::kInProgressWithTextExtracted:
    case OcrStatus::kCompletedWithNoTextExtracted:
    case OcrStatus::kCompletedWithTextExtracted:
      for (auto& [page_id, tree_manager] : pages_) {
        DCHECK(tree_manager);
        const auto iter = page_serializers_.find(page_id);
        DCHECK(iter != std::cend(page_serializers_));
        std::unique_ptr<TreeSerializer>& serializer = iter->second;
        DCHECK(serializer);
        DCHECK(page_sources_.contains(page_id));
        auto new_serializer = std::make_unique<TreeSerializer>(
            page_sources_.at(page_id).get(), /* crash_on_error */ true);
        serializer.swap(new_serializer);
        ui::AXActionHandlerRegistry::GetInstance()->SetAXTreeID(
            tree_manager->GetTreeID(), this);
        SendAXTreeToAccessibilityService(*tree_manager, *serializer);
      }
      ShowDocumentTree();
      break;
  }
}

void AXMediaAppUntrustedHandler::RemoveAllAXTreesFromAccessibilityService() {
  for (auto& [page_id, tree_manager] : pages_) {
    DCHECK(tree_manager);
    // Keep the OCR results to avoid recomputing them in case accessibility is
    // turned on again.
    ui::AXActionHandlerRegistry::GetInstance()->RemoveAXTreeID(
        tree_manager->GetTreeID());
  }
  RemoveDocumentTree();
}

void AXMediaAppUntrustedHandler::RemoveDocumentTree() {
  if (!document_) {
    return;
  }
  RemoveAXTreeID();
  document_serializer_.reset();
  document_source_.reset();
  document_.reset();
}

void AXMediaAppUntrustedHandler::SendAXTreeToAccessibilityService(
    const ui::AXTreeManager& manager,
    TreeSerializer& serializer) {
  DCHECK(manager.GetRoot());
  ui::AXTreeUpdate update;
  serializer.MarkSubtreeDirty(manager.GetRoot()->id());
  if (!serializer.SerializeChanges(manager.GetRoot(), &update)) {
    NOTREACHED() << "Failure to serialize should have already caused "
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
  DCHECK(event_router);
  const gfx::Point& mouse_location =
      aura::Env::GetInstance()->last_mouse_location();
  event_router->DispatchAccessibilityEvents(manager.GetTreeID(), {update},
                                            mouse_location, {});
#endif  // defined(USE_AURA)
}

void AXMediaAppUntrustedHandler::InitializeOcrService() {
  if (IsOcrServiceEnabled()) {
    return;
  }
  auto* profile =
      Profile::FromBrowserContext(base::to_address(browser_context_));
  ocr_ = screen_ai::OpticalCharacterRecognizer::CreateWithStatusCallback(
      profile, screen_ai::mojom::OcrClientType::kMediaApp,
      base::BindOnce(&AXMediaAppUntrustedHandler::OnOCRServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AXMediaAppUntrustedHandler::DisconnectFromOcrService() {
  ocr_.reset();
  // To avoid redoing OCR on the content if accessibility is temporarily turned
  // off / on, we keep the existing OCR results and do not reset the
  // `ocr_state_`.
}

void AXMediaAppUntrustedHandler::StartWatchingForAccessibilityEvents() {
  if (auto* web_contents = GetMediaAppWebContents()) {
    // Accessibility for the Media App's 'RenderFrameHost' may not become ready
    // as soon as the assistive software is turned on. Hence we use any event on
    // the host as a proxy for determining whether the host's accessibility is
    // ready, in order to stitch in our content.
    Observe(web_contents);
  }
}

void AXMediaAppUntrustedHandler::StopWatchingForAccessibilityEvents() {
  Observe(nullptr);
}

void AXMediaAppUntrustedHandler::ViewportUpdated(const gfx::RectF& viewport_box,
                                                 float scale_factor) {
  viewport_box_ = viewport_box;
  scale_factor_ = scale_factor;
  if (!document_ || !document_->GetRoot()) {
    return;
  }
  DCHECK(document_->ax_tree());
  ui::AXNodeData document_root_data = document_->GetRoot()->data();
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollXMax,
      base::checked_cast<int32_t>(
          document_root_data.relative_bounds.bounds.width() -
          viewport_box_.width()));
  document_root_data.AddIntAttribute(
      ax::mojom::IntAttribute::kScrollYMax,
      base::checked_cast<int32_t>(
          document_root_data.relative_bounds.bounds.height() -
          viewport_box_.height()));
  document_root_data.relative_bounds.transform =
      MakeTransformFromOffsetAndScale();

  ui::AXTreeUpdate document_update;
  document_update.root_id = document_root_data.id;
  document_update.nodes = {document_root_data};
  if (!document_->ax_tree()->Unserialize(document_update)) {
    mojo::ReportBadMessage(document_->ax_tree()->error());
  }
  SendAXTreeToAccessibilityService(*document_, *document_serializer_);
}

void AXMediaAppUntrustedHandler::UpdatePageLocation(
    const std::string& page_id,
    const gfx::RectF& page_location) {
  // `bad_message_callback_` (used by `HasRendererTerminatedDueToBadPageId`)
  // should have been set by `PageMetadataUpdated`, which calls this method.
  if (HasRendererTerminatedDueToBadPageId("UpdatePageLocation", page_id)) {
    return;
  }
  if (!pages_.contains(page_id)) {
    DCHECK(page_metadata_.contains(page_id));
    page_metadata_[page_id].rect = page_location;
    return;
  }
  ui::AXTree* tree = pages_.at(page_id)->ax_tree();
  DCHECK(tree->root());
  ui::AXNodeData root_data = tree->root()->data();
  root_data.relative_bounds.bounds = page_location;
  ui::AXTreeUpdate location_update;
  location_update.root_id = tree->root()->id();
  location_update.nodes = {root_data};
  ui::AXNode* image = tree->root()->GetFirstUnignoredChild();
  if (image && image->GetRole() == ax::mojom::Role::kImage) {
    // We auto-generate an unlabeled image if the OCR Service has returned no
    // results for a particular page.
    ui::AXNodeData image_data = image->data();
    image_data.relative_bounds.bounds = page_location;
    image_data.relative_bounds.bounds.set_origin({0, 0});
    location_update.nodes.push_back(image_data);
  }
  if (!tree->Unserialize(location_update)) {
    mojo::ReportBadMessage(tree->error());
    return;
  }
}

void AXMediaAppUntrustedHandler::ShowOcrServiceFailedToInitializeMessage() {
  DCHECK_EQ(ocr_status_, OcrStatus::kInitializationFailed);
  ui::AXTreeUpdate document_update;
  document_update.nodes = CreateStatusNodesWithLandmark();
  DCHECK_GT(document_update.nodes.size(), 0u);
  document_update.root_id = document_update.nodes[0].id;
  UpdateDocumentTree(document_update);
}

void AXMediaAppUntrustedHandler::ShowDocumentTree() {
  if (ocr_status_ == OcrStatus::kUninitialized) {
    return;
  }
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

  std::map<const uint32_t, const AXMediaAppPageMetadata> pages_in_order;
  auto end_iter = std::begin(page_metadata_);
  std::advance(end_iter, pages_ocred_on_initial_load_);
  std::transform(
      std::begin(page_metadata_), end_iter,
      std::inserter(pages_in_order, std::begin(pages_in_order)),
      [](const std::pair<const std::string, const AXMediaAppPageMetadata>
             page) { return std::pair(page.second.page_num, page.second); });
  // Remove all the deleted pages.
  std::erase_if(pages_in_order, [](const auto& page) { return !page.first; });

  if (pages_in_order.size() > 0u) {
    // TODO(b/319536234): Populate the title with the PDF's filename by
    // retrieving it from the Media App.
    document_root_data.SetNameChecked(base::StringPrintf(
        "PDF document containing %zu pages", pages_in_order.size()));
  }
  std::vector<int32_t> child_ids((has_landmark_node_ ? 1u : 0u) +
                                 pages_in_order.size());
  std::vector<ui::AXNodeData> status_nodes;
  if (has_landmark_node_) {
    status_nodes = CreateStatusNodesWithLandmark();
    DCHECK_GE(status_nodes.size(), 1u);
    child_ids.at(0) = status_nodes.at(0).id;
  }
  std::iota(std::begin(child_ids) + (has_landmark_node_ ? 1u : 0u),
            std::end(child_ids), kStartPageAXNodeId);
  std::vector<ui::AXNodeData> postamble_page_nodes;
  if (has_postamble_page_) {
    postamble_page_nodes = CreatePostamblePage();
    DCHECK_GE(postamble_page_nodes.size(), 1u);
    child_ids.push_back(postamble_page_nodes.at(0).id);
  }
  document_root_data.child_ids.swap(child_ids);

  gfx::RectF document_location;
  for (const auto& [_, page] : pages_in_order) {
    document_location.Union(page.rect);
  }
  document_root_data.relative_bounds.bounds = document_location;
  if (!viewport_box_.IsEmpty() && scale_factor_ > 0.0f) {
    document_root_data.relative_bounds.transform =
        MakeTransformFromOffsetAndScale();
  }
  document_root_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollXMin,
                                     document_location.x());
  document_root_data.AddIntAttribute(ax::mojom::IntAttribute::kScrollYMin,
                                     document_location.y());

  ui::AXTreeUpdate document_update;
  document_update.root_id = document_root_data.id;
  document_update.nodes.push_back(document_root_data);
  if (has_landmark_node_) {
    document_update.nodes.insert(std::end(document_update.nodes),
                                 std::begin(status_nodes),
                                 std::end(status_nodes));
  }
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
    const std::string& page_id = page_metadata.id;
    // If the page doesn't exist, that means it hasn't been through OCR yet.
    if (pages_.contains(page_id) && pages_.at(page_id)->ax_tree() &&
        pages_.at(page_id)->GetRoot()) {
      page_data.AddChildTreeId(pages_.at(page_id)->GetTreeID());
      const gfx::RectF& page_bounds =
          pages_.at(page_id)->GetRoot()->data().relative_bounds.bounds;
      // Set its origin to be (0,0) as the root node in a child tree for each
      // page will have a correct offset.
      page_data.relative_bounds.bounds =
          gfx::RectF(0, 0, page_bounds.width(), page_bounds.height());
    }
    document_update.nodes.push_back(page_data);
    ++page_index;
  }
  if (has_postamble_page_) {
    document_update.nodes.insert(std::end(document_update.nodes),
                                 std::begin(postamble_page_nodes),
                                 std::end(postamble_page_nodes));
  }
  UpdateDocumentTree(document_update);
}

void AXMediaAppUntrustedHandler::UpdateDocumentTree(
    ui::AXTreeUpdate& document_update) {
  // It wouldn't make sense to send an update with only a root node in it.
  if (document_update.nodes.size() <= 1u) {
    return;
  }

  if (document_ && document_->ax_tree()) {
    if (!document_->ax_tree()->Unserialize(document_update)) {
      mojo::ReportBadMessage(document_->ax_tree()->error());
      return;
    }
  } else {
    document_update.has_tree_data = true;
    if (auto* render_frame_host = GetMediaAppRenderFrameHost()) {
      DCHECK_NE(render_frame_host->GetAXTreeID(), ui::AXTreeIDUnknown())
          << "Accessibility should have been enabled by this point.";
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
    document_ = std::make_unique<ui::AXTreeManager>();
    document_->SetTree(std::move(document_tree));
    SetAXTreeID(document_tree_id_);
    StitchDocumentTree();
  }
  SendAXTreeToAccessibilityService(*document_, *document_serializer_);
}

void AXMediaAppUntrustedHandler::StitchDocumentTree() {
  content::RenderFrameHost* render_frame_host = GetMediaAppRenderFrameHost();
  if (!render_frame_host || !render_frame_host->IsRenderFrameLive()) {
    return;
  }
  if (render_frame_host->GetAXTreeID() == ui::AXTreeIDUnknown()) {
    return;
  }
  if (!document_ || !document_->ax_tree()) {
    return;
  }
  ui::AXActionData action_data;
  action_data.action = ax::mojom::Action::kStitchChildTree;
  action_data.target_tree_id = render_frame_host->GetAXTreeID();
  action_data.target_role = ax::mojom::Role::kGraphicsDocument;
  action_data.child_tree_id = document_->GetTreeID();
  render_frame_host->AccessibilityPerformAction(action_data);

  ui::AXTreeUpdate document_update;
  document_update.has_tree_data = true;
  document_update.tree_data = document_->GetTreeData();
  document_update.tree_data.parent_tree_id = render_frame_host->GetAXTreeID();
  if (!document_->ax_tree()->Unserialize(document_update)) {
    mojo::ReportBadMessage(document_->ax_tree()->error());
    return;
  }
}

bool AXMediaAppUntrustedHandler::IsOcrServiceEnabled() const {
  switch (ocr_status_) {
    case OcrStatus::kUninitialized:
    case OcrStatus::kInitializationFailed:
      DCHECK(!ocr_ || !ocr_->is_ready());
      return false;
    case OcrStatus::kInProgressWithNoTextExtractedYet:
    case OcrStatus::kInProgressWithTextExtracted:
    case OcrStatus::kCompletedWithNoTextExtracted:
    case OcrStatus::kCompletedWithTextExtracted:
      // OCR results might be in progress or completed, but accessibility might
      // have been turned off temporarily. In this case, we would release the
      // OCR Service to save memory.
      return ocr_ && ocr_->is_ready();
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
    mojo::ReportBadMessage("`PopDirtyPage()` found no more dirty pages.");
  }
  std::string dirty_page_id = dirty_page_ids_.front();
  dirty_page_ids_.pop_front();
  return dirty_page_id;
}

void AXMediaAppUntrustedHandler::OcrNextDirtyPageIfAny() {
  if (!IsOcrServiceEnabled()) {
    return;
  }
  if (pages_ocred_on_initial_load_ == page_metadata_.size()) {
    has_postamble_page_ = false;
    if (ocr_status_ == OcrStatus::kInProgressWithNoTextExtractedYet) {
      ocr_status_ = OcrStatus::kCompletedWithNoTextExtracted;
    } else if (ocr_status_ == OcrStatus::kInProgressWithTextExtracted) {
      ocr_status_ = OcrStatus::kCompletedWithTextExtracted;
    }
  }
  // If there are no more dirty pages, we can assume all pages have up-to-date
  // page locations. Update the document tree information to reflect that.
  if (dirty_page_ids_.empty() ||
      (pages_ocred_on_initial_load_ &&
       pages_ocred_on_initial_load_ % ComputePagesPerBatch() == 0u)) {
    ShowDocumentTree();
    if (dirty_page_ids_.empty()) {
      return;
    }
  }
  const std::string dirty_page_id = PopDirtyPage();
  // TODO(b/289012145): Refactor this code to support things happening
  // asynchronously - i.e. `RequestBitmap` will be async.
  if (media_app_) [[unlikely]] {
    // `media_app_` is only used for testing.
    CHECK_IS_TEST();
    SkBitmap page_bitmap = media_app_->RequestBitmap(dirty_page_id);
    // TODO - b/289012145: screen_ai_annotator_ is only bound in builds with
    // the ENABLE_SCREEN_AI_SERVICE buildflag. We should figure out a way to
    // mock it in tests running on bots without this flag and call
    // OnBitmapReceived() here.
    ocr_->PerformOCR(
        page_bitmap,
        base::BindOnce(&AXMediaAppUntrustedHandler::OnPageOcred,
                       weak_ptr_factory_.GetWeakPtr(), dirty_page_id));
  } else {
    media_app_ui::mojom::OcrUntrustedPage::RequestBitmapCallback cb =
        base::BindOnce(&AXMediaAppUntrustedHandler::OnBitmapReceived,
                       weak_ptr_factory_.GetWeakPtr(), dirty_page_id);
    media_app_page_->RequestBitmap(dirty_page_id, std::move(cb));
  }
}

void AXMediaAppUntrustedHandler::OnBitmapReceived(
    const std::string& dirty_page_id,
    const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    OnPageOcred(dirty_page_id, ui::AXTreeUpdate());
    return;
  }
  ocr_->PerformOCR(
      bitmap, base::BindOnce(&AXMediaAppUntrustedHandler::OnPageOcred,
                             weak_ptr_factory_.GetWeakPtr(), dirty_page_id));
}

void AXMediaAppUntrustedHandler::OnPageOcred(
    const std::string& dirty_page_id,
    const ui::AXTreeUpdate& tree_update) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!tree_update.nodes.empty() &&
      (
          // TODO(b/319536234): Validate tree ID.
          // !tree_update.has_tree_data ||
          // ui::AXTreeIDUnknown() == tree_update.tree_data.tree_id ||
          ui::kInvalidAXNodeID == tree_update.root_id)) {
    mojo::ReportBadMessage("OnPageOcred() bad tree update from Screen AI.");
    return;
  }
  ui::AXTreeUpdate complete_tree_update = tree_update;
  if (!tree_update.nodes.empty()) {
    ocr_status_ = OcrStatus::kInProgressWithTextExtracted;
    screen_ai::RecordMostDetectedLanguageInOcrData(
        "Accessibility.PdfOcr.MediaApp.MostDetectedLanguageInOcrData",
        tree_update);
  } else {
    // The most meaningful result to present to the user is that there is an
    // unlabeled image.
    ui::AXNodeData paragraph;
    paragraph.id = 1;
    paragraph.role = ax::mojom::Role::kParagraph;
    // The paragraph's bounds are set by `ShowDocumentTree`, so no need to
    // set them here.
    paragraph.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);

    ui::AXNodeData unlabeled_image;
    unlabeled_image.id = 2;
    unlabeled_image.role = ax::mojom::Role::kImage;
    unlabeled_image.relative_bounds.bounds =
        page_metadata_.at(dirty_page_id).rect;
    unlabeled_image.relative_bounds.bounds.set_origin({0, 0});
    unlabeled_image.relative_bounds.offset_container_id = paragraph.id;
    unlabeled_image.SetRestriction(ax::mojom::Restriction::kReadOnly);
    unlabeled_image.SetNameChecked(
        l10n_util::GetStringUTF8(IDS_AX_UNLABELED_IMAGE_ROLE_DESCRIPTION));
    paragraph.child_ids = {unlabeled_image.id};

    complete_tree_update.root_id = paragraph.id;
    complete_tree_update.nodes = {paragraph, unlabeled_image};
  }
  complete_tree_update.has_tree_data = true;
  complete_tree_update.tree_data.parent_tree_id = document_tree_id_;
  if (HasRendererTerminatedDueToBadPageId("OnPageOcred", dirty_page_id)) {
    return;
  }
  auto pages_it = pages_.find(dirty_page_id);
  if (pages_it == pages_.end()) {
    // Add a newly generated tree id to the tree update so that the new
    // `AXSerializableTree` that's generated has a non-empty tree id.
    complete_tree_update.tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
    auto page_tree =
        std::make_unique<ui::AXSerializableTree>(complete_tree_update);
    page_sources_[dirty_page_id] =
        base::WrapUnique<TreeSource>(page_tree->CreateTreeSource());
    page_serializers_[dirty_page_id] = std::make_unique<TreeSerializer>(
        page_sources_[dirty_page_id].get(), /* crash_on_error */ true);
    pages_it =
        pages_
            .insert({dirty_page_id,
                     std::make_unique<ui::AXTreeManager>(std::move(page_tree))})
            .first;
    ui::AXActionHandlerRegistry::GetInstance()->SetAXTreeID(
        complete_tree_update.tree_data.tree_id, this);
  } else {
    std::unique_ptr<ui::AXTreeManager>& page = pages_it->second;
    complete_tree_update.tree_data.tree_id = page->GetTreeID();
    if (!page->ax_tree() ||
        !page->ax_tree()->Unserialize(complete_tree_update)) {
      mojo::ReportBadMessage(page->ax_tree() ? page->ax_tree()->error()
                                             : "Missing page ax_tree");
      return;
    }
  }
  DCHECK_NE(pages_it->second->GetTreeID().type(),
            ax::mojom::AXTreeIDType::kUnknown);

  // Update the page location again - running the page through OCR overwrites
  // the previous `AXTree` it was given and thus the page location it was
  // already given in `PageMetadataUpdated()`. Restore it here.
  UpdatePageLocation(dirty_page_id, page_metadata_[dirty_page_id].rect);
  SendAXTreeToAccessibilityService(*pages_it->second,
                                   *page_serializers_.at(dirty_page_id));
  if (pages_ocred_on_initial_load_ < page_metadata_.size()) {
    ++pages_ocred_on_initial_load_;
  }
  OcrNextDirtyPageIfAny();
}

bool AXMediaAppUntrustedHandler::HasRendererTerminatedDueToBadPageId(
    const std::string& method_name,
    const std::string& page_id) {
  if (!page_metadata_.contains(page_id)) {
    const std::string error_str =
        base::StringPrintf("`%s` called with previously non-existent page ID",
                           method_name.c_str());
    if (bad_message_callback_ && !(*bad_message_callback_).is_null()) {
      std::move(*bad_message_callback_).Run(error_str);
    } else {
      mojo::ReportBadMessage(error_str);
    }
    return true;
  }
  return false;
}

std::unique_ptr<gfx::Transform>
AXMediaAppUntrustedHandler::MakeTransformFromOffsetAndScale() const {
  auto transform = std::make_unique<gfx::Transform>();
  float device_pixel_ratio = 1.0f;
  if (native_window_) {
    const auto maybe_device_pixel_ratio =
        display::Screen::GetScreen()->GetPreferredScaleFactorForWindow(
            native_window_);
    device_pixel_ratio = maybe_device_pixel_ratio.value_or(device_pixel_ratio);
  }
  transform->Scale(device_pixel_ratio);
  transform->Scale(scale_factor_);
  // `viewport_box_.origin()` represents the offset from which the viewport
  // starts, based on the origin of PDF content; e.g. if it's (-100, -10), it
  // indicates that PDF content starts at (100, 10) from the viewport's origin.
  transform->Translate(-viewport_box_.origin().x(),
                       -viewport_box_.origin().y());
  return transform;
}

}  // namespace ash
