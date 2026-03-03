// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include "base/strings/to_string.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkDashPathEffect.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/skia_conversions.h"

// TODO(http://b/485358530): Consider `OverlayBaseController::State` to the
// mojom file so the << operator is auto generated.
std::ostream& operator<<(std::ostream& os, OverlayBaseController::State value) {
  switch (value) {
    case OverlayBaseController::State::kOff:
      return os << "kOff";
    case OverlayBaseController::State::kClosingOpenedSidePanel:
      return os << "kClosingOpenedSidePanel";
    case OverlayBaseController::State::kScreenshot:
      return os << "kScreenshot";
    case OverlayBaseController::State::kStartingWebUI:
      return os << "kStartingWebUI";
    case OverlayBaseController::State::kOverlay:
      return os << "kOverlay";
    case OverlayBaseController::State::kHidden:
      return os << "kHidden";
    case OverlayBaseController::State::kBackground:
      return os << "kBackground";
    case OverlayBaseController::State::kClosing:
      return os << "kClosing";
    case OverlayBaseController::State::kIsReshowing:
      return os << "kIsReshowing";
    case OverlayBaseController::State::kHiding:
      return os << "kHiding";
  }
}

namespace glic {

namespace {

gfx::RectF GetRectForRegion(const SkBitmap& image, const gfx::RectF& region) {
  double x_scale = image.width();
  double y_scale = image.height();
  return gfx::RectF((region.x() - 0.5 * region.width()) * x_scale,
                    (region.y() - 0.5 * region.height()) * y_scale,
                    region.width() * x_scale, region.height() * y_scale);
}

class SelectionOverlayFetchPageProgressListener
    : public page_content_annotations::FetchPageProgressListener {
 public:
  using ScreenshotCallback = base::OnceCallback<void(const SkBitmap&)>;

  SelectionOverlayFetchPageProgressListener(
      ScreenshotCallback screenshot_ready_callback,
      ScreenshotCallback screenshot_redacted_callback)
      : screenshot_ready_callback_(std::move(screenshot_ready_callback)),
        screenshot_redacted_callback_(std::move(screenshot_redacted_callback)) {
  }

  ~SelectionOverlayFetchPageProgressListener() override = default;

  void ScreenshotCaptured(const SkBitmap& bitmap) override {
    std::move(screenshot_ready_callback_).Run(bitmap);
  }

  void ScreenshotRedacted(const SkBitmap& bitmap) override {
    std::move(screenshot_redacted_callback_).Run(bitmap);
  }

 private:
  ScreenshotCallback screenshot_ready_callback_;
  ScreenshotCallback screenshot_redacted_callback_;
};

}  // namespace

DEFINE_USER_DATA(SelectionOverlayController);

SelectionOverlayController::SelectionOverlayController(
    tabs::TabInterface* tab,
    PrefService* pref_service)
    : OverlayBaseController(tab, pref_service),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {}

SelectionOverlayController::~SelectionOverlayController() = default;

// static.
SelectionOverlayController* SelectionOverlayController::FromOverlayWebContents(
    content::WebContents* overlay_web_contents) {
  return Get(
      webui::GetTabInterface(overlay_web_contents)->GetUnownedUserDataHost());
}

// static.
SelectionOverlayController* SelectionOverlayController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  return Get(tabs::TabInterface::GetFromContents(tab_web_contents)
                 ->GetUnownedUserDataHost());
}

void SelectionOverlayController::BindOverlay(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  CHECK_EQ(state(), State::kStartingWebUI) << base::ToString(state());

  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &SelectionOverlayController::Reset, weak_factory_.GetWeakPtr()));
  page_.Bind(std::move(page));

  InitializeOverlay();
}

void SelectionOverlayController::Show() {
  ShowModalUI();
}

void SelectionOverlayController::Hide() {
  HideOverlay();
}

void SelectionOverlayController::RequestSyncClose(
    DismissalSource dismissal_source) {
  CloseUI();
}

void SelectionOverlayController::InitializeOverlay() {
  // We can only continue once both the WebUI is bound and the initialization
  // data is processed and ready. If either of those conditions aren't met, we
  // exit early and wait for the other condition to call this method again.
  if (!page_ || !screenshot_available_) {
    return;
  }

  InitializeOverlayImpl();

  CHECK(page_);
  page_->ScreenshotReceived(initial_rgb_screenshot_);
}

void SelectionOverlayController::StartScreenshotFlow() {
  page_content_annotations::FetchPageContextOptions options;
  options.screenshot_options =
      page_content_annotations::ScreenshotOptions::ViewportOnly(
          /*paint_preview_options=*/std::nullopt,
          /*screenshot_collection_options=*/std::nullopt);
  options.annotated_page_content_options =
      optimization_guide::DefaultAIPageContentOptions(
          /*on_critical_path=*/true);
  auto progress_listener =
      std::make_unique<SelectionOverlayFetchPageProgressListener>(
          base::BindOnce(&SelectionOverlayController::OnScreenshotTaken,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&SelectionOverlayController::OnScreenshotRedacted,
                         weak_factory_.GetWeakPtr()));
  page_content_annotations::FetchPageContext(
      *tab_->GetContents(), options, std::move(progress_listener),
      base::BindOnce(&SelectionOverlayController::PageAnnotationReady,
                     weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::NotifyOverlayClosing() {}

void SelectionOverlayController::OnScreenshotTaken(const SkBitmap& bitmap) {
  InitializeScreenshot(
      bitmap, base::BindOnce(&SelectionOverlayController::SetScreenshot,
                             weak_factory_.GetWeakPtr(), bitmap));
}

void SelectionOverlayController::OnScreenshotRedacted(const SkBitmap& bitmap) {
  redacted_screenshot_ = bitmap;
}

void SelectionOverlayController::PageAnnotationReady(
    page_content_annotations::FetchPageContextResultCallbackArg fetch_result) {
  if (!fetch_result.has_value()) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  page_content_annotations::FetchPageContextResult& page_context =
      **fetch_result;

  if (!page_context.annotated_page_content_result.has_value() ||
      !page_context.screenshot_result.has_value()) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  page_context.annotated_page_content_result->proto
      .mutable_gemini_in_chrome_page_metadata()
      ->mutable_screenshot_info()
      ->set_has_selection_region_in_screenshot(true);

  ai_page_content_ =
      std::move(page_context.annotated_page_content_result->proto);
}

void SelectionOverlayController::SetScreenshot(const SkBitmap& screenshot,
                                               SkBitmap rgb_screenshot) {
  initial_rgb_screenshot_ = std::move(rgb_screenshot);
  screenshot_available_ = true;
  InitializeOverlay();
}

bool SelectionOverlayController::IsResultsSidePanelShowing() {
  return true;
}

GURL SelectionOverlayController::GetInitialURL() {
  return GURL(chrome::kChromeUIGlicSelectionOverlayURL);
}

void SelectionOverlayController::NotifyIsOverlayShowing(bool is_showing) {}

int SelectionOverlayController::GetToolResourceId() {
  return IDS_GLIC_SELECTION_OVERLAY_RENDERER_LABEL;
}

ui::ElementIdentifier SelectionOverlayController::GetViewContainerId() {
  return kGlicSelectionOverlayViewElementId;
}

SidePanelEntry::PanelType SelectionOverlayController::GetSidePanelType() {
  return SidePanelEntry::PanelType::kContent;
}

bool SelectionOverlayController::ShouldCloseSidePanel() {
  return false;
}

bool SelectionOverlayController::ShouldShowPreselectionBubble() {
  return true;
}

bool SelectionOverlayController::UseOverlayBlur() {
  return true;
}

void SelectionOverlayController::NotifyPageNavigated() {}

void SelectionOverlayController::NotifyTabForegrounded() {}

void SelectionOverlayController::NotifyTabWillEnterBackground() {}

bool SelectionOverlayController::IsOverlayViewShared() const {
  // Glic's selection overlay's WebView is attached to the ContentsContainerView
  // which cannot be shared across multiple tabs.
  return false;
}

void SelectionOverlayController::DismissOverlay(
    selection::DismissOverlayReason reason) {
  CloseUI();
}

void SelectionOverlayController::AdjustRegion(
    selection::SelectedRegionPtr target) {
  auto it = selected_regions_.find(target->id);
  if (it != selected_regions_.end()) {
    it->second = std::move(target);
  } else {
    selected_regions_[target->id] = std::move(target);
  }

  RenderRegions();
}

void SelectionOverlayController::DeleteRegion(
    const base::UnguessableToken& id) {
  if (selected_regions_.erase(id)) {
    RenderRegions();
  }
}

void SelectionOverlayController::ClosePreselectionBubble() {
  ClosePreselectionBubbleImpl();
}

void SelectionOverlayController::AddBackgroundBlur() {
  AddBackgroundBlurImpl();
}

void SelectionOverlayController::SetLiveBlur(bool enabled) {
  SetLiveBlurImpl(enabled);
}

void SelectionOverlayController::Reset() {
  receiver_.reset();
  page_.reset();
  initial_rgb_screenshot_.reset();
  redacted_screenshot_.reset();
  screenshot_available_ = false;
  encoded_.reset();
  selected_regions_.clear();
}

void SelectionOverlayController::RenderRegions() {
  if (redacted_screenshot_.empty()) {
    return;
  }

  std::vector<SkRect> regions;
  std::vector<selection::SelectedRegionPtr> regions_mojo;
  // TODO(http://b/452032491): Reconsider what happens if the regions overlap.
  // TODO(http://b/452032491): Currently this class is only used once per
  // selection and only one region is supported, so it is fine to always loop
  // through all the regions. Revisit once we expand the selections.
  for (const auto& [id, region] : selected_regions_) {
    SkRect rect_on_canvas = gfx::RectFToSkRect(
        GetRectForRegion(redacted_screenshot_, region->region));
    if (!rect_on_canvas.isEmpty() &&
        redacted_screenshot_.bounds().contains(rect_on_canvas)) {
      regions.push_back(rect_on_canvas);
      regions_mojo.push_back(region.Clone());
    } else {
      // TODO(http://b/485358530): Record proper histograms for the error case.
      LOG(ERROR) << "Invalid region selected " << region->region.ToString();
    }
  }

  page_->SetPostRegionSelections(std::move(regions_mojo));

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(
          [](const SkBitmap& bitmap, std::vector<SkRect> regions) {
            SkBitmap deep_copy_bitmap;
            std::optional<std::vector<uint8_t>> result;
            // TODO(http://b/485358530): Record proper histograms for the error
            // case. Allocate memory for the deep copy.
            if (!deep_copy_bitmap.tryAllocPixels(bitmap.info())) {
              LOG(ERROR) << "Alloc failure";
              return result;
            }

            SkCanvas canvas(deep_copy_bitmap);
            canvas.drawImage(bitmap.asImage(), 0, 0);
            SkPaint paint;
            const SkScalar intervals[] = {5.0f, 5.0f};
            paint.setStyle(SkPaint::kStroke_Style);
            paint.setStrokeWidth(2.0f);

            for (const auto& region : regions) {
              paint.setColor(SK_ColorMAGENTA);
              paint.setPathEffect(SkDashPathEffect::Make(intervals, 0.0f));
              canvas.drawRect(region, paint);
              paint.setPathEffect(SkDashPathEffect::Make(intervals, -5.0f));
              paint.setColor(SK_ColorCYAN);
              canvas.drawRect(region, paint);
            }
            // TODO(https://b/485548840): Pass in the screenshot collection
            // options.
            result = page_content_annotations::EncodeScreenshot(
                deep_copy_bitmap, std::nullopt);
            return result;
          },
          redacted_screenshot_, std::move(regions)),
      base::BindOnce(&SelectionOverlayController::RegionsRendererd,
                     weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::RegionsRendererd(
    std::optional<std::vector<uint8_t>> encoded) {
  encoded_ = encoded;
}

}  // namespace glic
