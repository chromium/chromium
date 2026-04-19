// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include "base/strings/to_string.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_view_host.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/webview/webview.h"

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

bool IsEscapeEvent(const input::NativeWebKeyboardEvent& event) {
  return event.GetType() == input::NativeWebKeyboardEvent::Type::kRawKeyDown &&
         event.windows_key_code == ui::VKEY_ESCAPE;
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

  void BeginScreenshot() override {}
  void EndScreenshot(std::optional<std::string> error) override {}
  void BeginAPC() override {}
  void EndAPC(std::optional<std::string> error) override {}

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
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&SelectionOverlayController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&SelectionOverlayController::TabDeactivated,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&SelectionOverlayController::WillDiscardContents,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &SelectionOverlayController::WillDetach, weak_factory_.GetWeakPtr())));
}

SelectionOverlayController::~SelectionOverlayController() = default;

void SelectionOverlayController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  CloseUI();
}

void SelectionOverlayController::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  CloseUI();
}

void SelectionOverlayController::TabDeactivated(tabs::TabInterface* tab) {
  if (state() == State::kBackground) {
    return;
  }
  TabWillEnterBackground(tab);
}

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

void SelectionOverlayController::BindCaptureRegionObserver(
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  if (capture_region_observer_.is_bound()) {
    // TODO(b/452032491): This should only happen in a compromised renderer.
    // Since `mojom::CaptureRegionObserver` will be deprecated, using
    // kUnknown with a log message is acceptable.
    LOG(ERROR) << "capture_region_observer_ is already bound. State "
               << state();
    capture_region_observer_->OnUpdate(
        mojom::CaptureRegionResultPtr(),
        mojom::CaptureRegionErrorReason::kUnknown);
    capture_region_observer_.reset();
  }
  capture_region_observer_.Bind(std::move(observer));
  capture_region_observer_.set_disconnect_handler(base::BindOnce(
      &SelectionOverlayController::CloseUI, weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::Show() {
  ShowModalUI();
}

void SelectionOverlayController::Close() {
  CloseUI();
}

void SelectionOverlayController::CloseUI() {
  if (state() == State::kOff) {
    return;
  }
  Reset();
  OverlayBaseController::CloseUI();
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

bool SelectionOverlayController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  if (!overlay_web_view_ || state() != State::kOverlay) {
    return false;
  }
  views::FocusManager* focus_manager = overlay_web_view_->GetFocusManager();
  if (!focus_manager) {
    return false;
  }
  if (IsEscapeEvent(event)) {
    CloseUI();
    return true;
  }
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(event,
                                                               focus_manager);
}

void SelectionOverlayController::StartScreenshotFlow() {
  auto options = mojom::GetTabContextOptions::New();
  options->include_viewport_screenshot = true;
  options->include_annotated_page_content = true;

  auto progress_listener =
      std::make_unique<SelectionOverlayFetchPageProgressListener>(
          base::BindOnce(&SelectionOverlayController::OnScreenshotTaken,
                         weak_factory_.GetWeakPtr()),
          base::BindOnce(&SelectionOverlayController::OnScreenshotRedacted,
                         weak_factory_.GetWeakPtr()));
  FetchPageContext(tab_, *options,
                   base::BindOnce(&SelectionOverlayController::PageContextReady,
                                  weak_factory_.GetWeakPtr()),
                   std::move(progress_listener),
                   /*is_screenshot_annotated=*/true);
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

void SelectionOverlayController::PageContextReady(
    base::expected<glic::mojom::GetContextResultPtr,
                   page_content_annotations::FetchPageContextErrorDetails>
        fetch_result) {
  if (!fetch_result.has_value() || !fetch_result.value()->is_tab_context()) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  tab_context_ = std::move(fetch_result.value()->get_tab_context());
  if (!tab_context_->annotated_page_data ||
      !tab_context_->viewport_screenshot) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }
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

void SelectionOverlayController::NotifyIsOverlayShowing(bool is_showing) {
  if (!is_showing) {
    GlicKeyedService* service =
        GlicKeyedService::Get(tab_->GetBrowserWindowInterface()->GetProfile());
    if (service) {
      if (GlicInstance* instance = service->GetInstanceForTab(tab_)) {
        instance->OnSelectionAreasChanged(0);
      }
    }
  }
}

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

void SelectionOverlayController::NotifyPageNavigated() {
  CloseUI();
}

void SelectionOverlayController::NotifyTabForegrounded() {}

void SelectionOverlayController::NotifyTabWillEnterBackground() {}

OverlayBaseController::PreselectionUIConfig
SelectionOverlayController::GetPreselectionBubbleConfig() {
  return {
      .message_string_id = IDS_GLIC_SELECTION_OVERLAY_PRESELECTION_BUBBLE_TEXT,
      .show_cancel_button = true,
      // TODO(b:503000173): Add a new color code alias for this.
      .cancel_button_color = ui::kColorSysInversePrimary,
      .bubble_background_color = kColorGlicSelectionOverlayToast,
      .icon = &vector_icons::kCropFreeIcon};
}

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
    if (selected_regions_.empty()) {
      CloseUI();
      return;
    }
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
  selected_regions_.clear();
  tab_context_.reset();
  capture_region_observer_.reset();
}

void SelectionOverlayController::RenderRegions() {
  if (redacted_screenshot_.empty()) {
    return;
  }

  std::vector<std::pair<base::UnguessableToken, gfx::Rect>> gfx_regions;
  std::vector<selection::SelectedRegionPtr> regions_mojo;
  // TODO(http://b/452032491): Reconsider what happens if the regions overlap.
  // TODO(http://b/452032491): Currently this class is only used once per
  // selection and only one region is supported, so it is fine to always loop
  // through all the regions. Revisit once we expand the selections.
  for (const auto& [id, region] : selected_regions_) {
    gfx::RectF gfx_rect_on_canvas =
        GetRectForRegion(redacted_screenshot_, region->region);
    SkRect rect_on_canvas = gfx::RectFToSkRect(gfx_rect_on_canvas);
    if (!rect_on_canvas.isEmpty() &&
        redacted_screenshot_.bounds().contains(rect_on_canvas)) {
      gfx_regions.emplace_back(id, gfx::ToEnclosingRect(gfx_rect_on_canvas));
      regions_mojo.push_back(region.Clone());
    } else {
      // TODO(http://b/485358530): Record proper histograms for the error case.
      LOG(ERROR) << "Invalid region selected " << region->region.ToString();
    }
  }

  page_->SetPostRegionSelections(std::move(regions_mojo));

  mojom::AdditionalContextPtr additional_context =
      CreateAdditionalContext(gfx_regions);
  GlicKeyedService* service =
      GlicKeyedService::Get(tab_->GetBrowserWindowInterface()->GetProfile());
  service->SendAdditionalContext(tab_->GetHandle(),
                                 std::move(additional_context));
  if (GlicInstance* instance = service->GetInstanceForTab(tab_)) {
    instance->OnSelectionAreasChanged(selected_regions_.size());
    if (instance->IsActive()) {
      if (content::WebContents* web_contents =
              instance->host().webui_contents()) {
        web_contents->Focus();
      }
    }
  }

}

glic::mojom::AdditionalContextPtr
SelectionOverlayController::CreateAdditionalContext(
    const std::vector<std::pair<base::UnguessableToken, gfx::Rect>>& regions) {
  auto context = glic::mojom::AdditionalContext::New();
  std::vector<glic::mojom::AdditionalContextPartPtr> parts;
  mojom::TabContextPtr tab_context = tab_context_.Clone();
  parts.push_back(glic::mojom::AdditionalContextPart::NewTabContext(
      std::move(tab_context)));
  for (const auto& region : regions) {
    parts.push_back(glic::mojom::AdditionalContextPart::NewPendingRegion(
        glic::mojom::PendingCapturedRegion::New(
            region.first,
            glic::mojom::CapturedRegion::NewRect(region.second))));
    parts.push_back(glic::mojom::AdditionalContextPart::NewRegion(
        glic::mojom::CapturedRegion::NewRect(region.second)));
  }
  context->source = glic::mojom::AdditionalContextSource::kRegionSelection;
  context->tab_id = tab_->GetHandle().raw_value();
  context->parts = std::move(parts);
  return context;
}

}  // namespace glic
