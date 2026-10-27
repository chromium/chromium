// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/glic/glic_selection_observer.h"
#include "chrome/browser/glic/glic_warming_checks.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/glic/selection/quick_answers_tool.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/webview/webview.h"
#include "url/gurl.h"

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

// Kill switch for dropping the caller's screenshot size cap when capturing for
// the selection overlay. https://crbug.com/512915349
BASE_FEATURE(kGlicSelectionOverlayFullSizeScreenshot,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStaticSelectionSuggestions, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kQuickAnswersSelectionSuggestions,
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsSelectionOverlayPromptEnabled() {
  return base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPrompt) ||
         base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox);
}

selection::SuggestedActionIcon GetIconForTool(
    ::selection::Suggestion::ToolId tool_id) {
  switch (tool_id) {
    case optimization_guide::proto::SMART_SELECTION_TOOL_QUICK_ANSWERS:
      return selection::SuggestedActionIcon::kExplain;
    case optimization_guide::proto::SMART_SELECTION_TOOL_GEMINI_IN_CHROME:
      return selection::SuggestedActionIcon::kSpark;
    default:
      return selection::SuggestedActionIcon::kNone;
  }
}

gfx::RectF GetRectForRegion(const SkBitmap& image, const gfx::RectF& region) {
  double x_scale = image.width();
  double y_scale = image.height();
  return gfx::RectF((region.x() - 0.5 * region.width()) * x_scale,
                    (region.y() - 0.5 * region.height()) * y_scale,
                    region.width() * x_scale, region.height() * y_scale);
}

constexpr int kSelectionPaddingDip = 5;
constexpr int kMaxSurroundingTextLength = 2000;
constexpr base::TimeDelta kSurroundingTextTimeout = base::Seconds(1);

selection::SelectedRegionPtr CreateRegionFromBounds(
    gfx::Rect selection_bounds,
    const gfx::Rect& tab_bounds) {
  if (selection_bounds.IsEmpty() || tab_bounds.IsEmpty()) {
    return nullptr;
  }

  selection_bounds.Outset(kSelectionPaddingDip);

  float left = static_cast<float>(selection_bounds.x() - tab_bounds.x()) /
               tab_bounds.width();
  float right = static_cast<float>(selection_bounds.right() - tab_bounds.x()) /
                tab_bounds.width();
  float top = static_cast<float>(selection_bounds.y() - tab_bounds.y()) /
              tab_bounds.height();
  float bottom =
      static_cast<float>(selection_bounds.bottom() - tab_bounds.y()) /
      tab_bounds.height();

  // Clip to remain inside tab bounds.
  left = std::max(0.0f, left);
  right = std::min(1.0f, right);
  top = std::max(0.0f, top);
  bottom = std::min(1.0f, bottom);

  float width = right - left;
  float height = bottom - top;
  if (width <= 0.0f || height <= 0.0f) {
    return nullptr;
  }

  float center_x = (left + right) / 2.0f;
  float center_y = (top + bottom) / 2.0f;

  auto region = selection::SelectedRegion::New();
  region->id = base::UnguessableToken::Create();
  // Note that the rect is normalized against the tab's view bounds, and its
  // `(x, y)` is the center of the region rather than the top-left corner.
  // Both `GetRectForRegion()` and `post_selection_renderer.ts` convert back
  // from the center, so the center needs to be stored here.
  region->shape = selection::RegionShape::NewRect(
      gfx::RectF(center_x, center_y, width, height));
  return region;
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

// Mirrors the size math in `PageContextFetcher::GetScreenshotSize()`.
bool WouldCapDownscaleCapture(
    tabs::TabInterface* tab,
    const page_content_annotations::ScreenshotOptions::
        ScreenshotCollectionOptions& options) {
  int max_width = options.max_width.value_or(0);
  int max_height = options.max_height.value_or(0);
  if (max_width == 0 || max_height == 0) {
    return false;
  }

  content::RenderWidgetHostView* view =
      tab->GetContents()->GetRenderWidgetHostView();
  if (!view) {
    return false;
  }

  gfx::Size view_size_pixels = gfx::ScaleToRoundedSize(
      view->GetViewBounds().size(), view->GetDeviceScaleFactor());
  return view_size_pixels.width() > max_width ||
         view_size_pixels.height() > max_height;
}

}  // namespace

DEFINE_USER_DATA(SelectionOverlayController);

SelectionOverlayController::SelectedRegionData::SelectedRegionData(
    selection::SelectedRegionPtr region)
    : region(std::move(region)) {}

SelectionOverlayController::SelectedRegionData::SelectedRegionData(
    SelectedRegionData&&) = default;

SelectionOverlayController::SelectedRegionData&
SelectionOverlayController::SelectedRegionData::operator=(
    SelectedRegionData&&) = default;

SelectionOverlayController::SelectedRegionData::~SelectedRegionData() = default;

SelectionOverlayController::Session::Session(
    selection::SelectionOverlayPageHandler* handler)
    : receiver(handler) {}

SelectionOverlayController::Session::~Session() = default;

SelectionOverlayController::SelectionOverlayController(
    tabs::TabInterface* tab,
    PrefService* pref_service)
    : OverlayBaseController(tab, pref_service),
      scoped_unowned_user_data_(tab->GetUnownedUserDataHost(), *this) {
  if (base::FeatureList::IsEnabled(kQuickAnswersSelectionSuggestions) &&
      base::FeatureList::IsEnabled(
          ::selection::kSmartSelectionServerSuggestions)) {
    quick_answers_tool_ = std::make_unique<QuickAnswersTool>(CHECK_DEREF(tab_));
    if (auto* suggestion_service = ::selection::SuggestionService::From(tab_)) {
      suggestion_service->RegisterTool(quick_answers_tool_.get());
    }
  }
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&SelectionOverlayController::WillDiscardContents,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &SelectionOverlayController::WillDetach, weak_factory_.GetWeakPtr())));
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  CHECK(service);
  tab_subscriptions_.push_back(
      service->active_instance_sharing_manager().AddFocusedTabChangedCallback(
          base::BindRepeating(&SelectionOverlayController::OnFocusedTabChanged,
                              weak_factory_.GetWeakPtr())));
  BrowserWindowInterface* window = tab_->GetBrowserWindowInterface();
  CHECK(window);
  TabStripModel* tab_strip_model = window->GetTabStripModel();
  CHECK(tab_strip_model);
  tab_strip_model->AddObserver(this);
  SetOnEscCallback(base::BindRepeating(
      [](base::WeakPtr<SelectionOverlayController> controller) {
        if (controller && controller->state() == State::kOverlay) {
          controller->Close(CloseReason::kEscapeKeyPress);
          return true;
        }
        return false;
      },
      weak_factory_.GetWeakPtr()));
}

SelectionOverlayController::~SelectionOverlayController() {
  if (quick_answers_tool_) {
    if (auto* suggestion_service = ::selection::SuggestionService::From(tab_)) {
      suggestion_service->UnregisterTool(quick_answers_tool_.get());
    }
  }
  if (tab_ && tab_->GetBrowserWindowInterface()) {
    if (auto* tab_strip_model =
            tab_->GetBrowserWindowInterface()->GetTabStripModel()) {
      tab_strip_model->RemoveObserver(this);
    }
  }
}

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

void SelectionOverlayController::ObserveActiveTabChanges() {
  BrowserWindowInterface* window = tab_->GetBrowserWindowInterface();
  if (!window) {
    session_->active_tab_subscription = base::CallbackListSubscription();
    return;
  }
  session_->active_tab_subscription = window->RegisterActiveTabDidChange(
      base::BindRepeating(&SelectionOverlayController::OnActiveTabChanged,
                          session_weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::OnActiveTabChanged(
    BrowserWindowInterface* window) {
  UpdateForTabVisibility();
}

void SelectionOverlayController::ObserveSidePanelState() {
  GlicSidePanelCoordinator* coordinator =
      GlicSidePanelCoordinator::GetForTab(tab_);
  if (!coordinator) {
    session_->side_panel_state_subscription = base::CallbackListSubscription();
    return;
  }
  session_->side_panel_state_subscription = coordinator->AddStateCallback(
      base::BindRepeating(&SelectionOverlayController::OnSidePanelStateChanged,
                          session_weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::OnSidePanelStateChanged(
    GlicSidePanelCoordinator::State) {
  if (session_->page) {
    session_->page->SetSidePanelOpen(IsSidePanelOpen());
  }
}

bool SelectionOverlayController::IsSidePanelOpen() const {
  // A backgrounded panel (its tab isn't in front) isn't closed: it shows again
  // with the tab. Not `kShown` only, which would flip the overlay to closed and
  // back on every tab switch.
  return GlicSidePanelCoordinator::IsShowingOrBackgrounded(tab_);
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

content::RenderFrameHost* SelectionOverlayController::GetOverlayMainFrame()
    const {
  views::WebView* web_view = GetOverlayWebView();
  if (!web_view || !web_view->web_contents()) {
    return nullptr;
  }
  return web_view->web_contents()->GetPrimaryMainFrame();
}

std::vector<int> SelectionOverlayController::GetPolylineCounts() const {
  std::vector<int> polyline_counts;
  for (const auto& [id, region_data] : session_->selected_regions) {
    if (region_data.region->shape->is_polyline()) {
      polyline_counts.push_back(
          region_data.region->shape->get_polyline().size());
    }
  }
  return polyline_counts;
}

void SelectionOverlayController::BindOverlay(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  CHECK_EQ(state(), State::kStartingWebUI) << base::ToString(state());

  session_->receiver.Bind(std::move(receiver));
  session_->receiver.set_disconnect_handler(base::BindOnce(
      &SelectionOverlayController::Reset, session_weak_factory_.GetWeakPtr()));
  session_->page.Bind(std::move(page));

  if (overlay_web_view_) {
    session_->overlay_web_view_focus_subscription =
        overlay_web_view_->AddWebContentsFocusedCallback(base::BindRepeating(
            &SelectionOverlayController::OnOverlayWebViewFocused,
            session_weak_factory_.GetWeakPtr()));
  }

  InitializeOverlay();
}

void SelectionOverlayController::BindCaptureRegionObserver(
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  if (session_->capture_region_observer.is_bound()) {
    // TODO(b/452032491): This should only happen in a compromised renderer.
    // Since `mojom::CaptureRegionObserver` will be deprecated, using
    // kUnknown with a log message is acceptable.
    LOG(ERROR) << "capture_region_observer is already bound. State " << state();
    session_->capture_region_observer->OnUpdate(
        mojom::CaptureRegionResultPtr(),
        mojom::CaptureRegionErrorReason::kUnknown);
    session_->capture_region_observer.reset();
  }
  session_->capture_region_observer.Bind(std::move(observer));
  session_->capture_region_observer.set_disconnect_handler(
      base::BindOnce(&SelectionOverlayController::CloseUI,
                     session_weak_factory_.GetWeakPtr()));
}

// static
void SelectionOverlayController::CaptureRegion(
    tabs::TabInterface* tab,
    GlicSharingManagerInternal& sharing_manager,
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer,
    mojom::TabContextOptionsPtr options) {
  content::WebContents* web_contents = tab ? tab->GetContents() : nullptr;
  if (!web_contents) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kNoFocusableTab);
    return;
  }
  auto* selection_overlay_controller =
      SelectionOverlayController::FromTabWebContents(web_contents);
  if (!selection_overlay_controller) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    LOG(ERROR) << "SelectionOverlayController not found for tab "
               << web_contents->GetURL();
    return;
  }
  if (selection_overlay_controller->state() != State::kOff) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    LOG(ERROR) << "Overlay is still showing for " << web_contents->GetURL();
    return;
  }
  if (!sharing_manager.IsTabFocused(tab->GetHandle())) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kNoFocusableTab);
    LOG(ERROR) << "Tab " << web_contents->GetURL() << " is not focused";
    return;
  }
  std::optional<GlicGetContextError> eligibility_error =
      sharing_manager.CheckPreliminaryContextSharingEligibility(
          tab->GetHandle());
  if (eligibility_error.has_value()) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    LOG(ERROR) << "Cannot share tab context for " << web_contents->GetURL();
    return;
  }
  if (!selection_overlay_controller->CanStartSession()) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    return;
  }
  // When the small chip is enabled, pre-select the tab's current text
  // selection.
  if (base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip)) {
    if (auto* selection_observer = GlicSelectionObserver::From(tab)) {
      if (std::optional<gfx::Rect> bounds =
              selection_observer->GetCurrentSelectionBounds()) {
        selection_overlay_controller->SetRegionFromBounds(
            selection_observer->GetSelectedFrame(), *bounds);
        const bool has_region =
            selection_overlay_controller->GetSelectedRegionCount() > 0;
        if (has_region) {
          // Nothing is fetched yet, so this only queues the region for the
          // web client until the page context is here.
          selection_overlay_controller->SyncRegions(
              /*should_focus_panel=*/false);
          // Same as the other small chip paths: the text selection is fixed,
          // and a click on the background doesn't start a new region.
          selection_overlay_controller->session_->interaction_options =
              selection::InteractionOptions::New(
                  /*hide_handles=*/true, /*disable_multi_select=*/true,
                  /*skip_intro_animation=*/true);
        }
        selection_observer->DismissUI(
            GlicSelectionObserver::DismissReason::kActionTaken);
      }
    }
  }
  selection_overlay_controller->session_->owner = Session::Owner::kPanel;
  selection_overlay_controller->BindCaptureRegionObserver(std::move(observer));
  selection_overlay_controller->ShowImpl(std::move(options));
}

bool SelectionOverlayController::CanStartSession() {
  content::WebContents* web_contents = tab_->GetContents();
  if (state() != State::kOff) {
    LOG(ERROR) << "Overlay is still showing for " << web_contents->GetURL();
    return false;
  }
  auto* actor_service =
      actor::ActorKeyedService::Get(web_contents->GetBrowserContext());
  if (actor_service && actor_service->IsActiveOnTab(*tab_)) {
    LOG(ERROR) << "Tab has active actuation task: " << web_contents->GetURL();
    return false;
  }
  // `ShowModalUI()` silently returns when this fails, which would leave the
  // state staged by the caller behind while the controller is still `kOff`.
  if (!CanShowModalUI()) {
    LOG(ERROR) << "Cannot show modal UI for " << web_contents->GetURL();
    return false;
  }
  return true;
}

void SelectionOverlayController::Show(mojom::TabContextOptionsPtr options) {
  if (!CanStartSession()) {
    return;
  }
  ShowImpl(std::move(options));
}

void SelectionOverlayController::ShowImpl(mojom::TabContextOptionsPtr options) {
  session_->options = std::move(options);
  if (GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
      service && !service->GetInstanceForTab(tab_)) {
    service->TryPreload(GlicWarmingTrigger::kSelectionOverlay);
  }
  ObserveActiveTabChanges();
  ObserveSidePanelState();
  ShowModalUI();
}

// TODO(b/556786015): After requirement is finalized, reconsider if we should
// pass full interaction_options or just the selected media, and let this
// function decide which options to send to web.
void SelectionOverlayController::ShowWithSelection(
    content::RenderFrameHost* selected_frame,
    const gfx::Rect& selection_bounds,
    selection::InteractionOptionsPtr interaction_options) {
  if (!CanStartSession()) {
    return;
  }
  session_->interaction_options = std::move(interaction_options);
  SetRegionFromBounds(selected_frame, selection_bounds);
  ShowImpl(/*options=*/nullptr);
}

void SelectionOverlayController::SetRegionFromBounds(
    content::RenderFrameHost* selected_frame,
    const gfx::Rect& selection_bounds) {
  session_->selected_regions.clear();
  session_->active_region_id.reset();
  session_->surrounding_text_timer.Stop();
  if (tab_ && tab_->GetContents()) {
    if (auto region = CreateRegionFromBounds(
            selection_bounds, tab_->GetContents()->GetViewBounds())) {
      base::UnguessableToken id = region->id;
      session_->active_region_id = id;
      auto [it, _] = session_->selected_regions.emplace(
          id, SelectedRegionData(std::move(region)));
      if (selected_frame && selected_frame->IsRenderFrameLive()) {
        it->second.waiting_for_surrounding_text = true;
        selected_frame->RequestTextSurroundingSelection(
            base::BindOnce(&SelectionOverlayController::
                               OnTextSurroundingSelectionAvailable,
                           session_weak_factory_.GetWeakPtr(), id,
                           it->second.generation),
            kMaxSurroundingTextLength);
        session_->surrounding_text_timer.Start(
            FROM_HERE, kSurroundingTextTimeout,
            base::BindOnce(&SelectionOverlayController::
                               OnTextSurroundingSelectionAvailable,
                           session_weak_factory_.GetWeakPtr(), id,
                           it->second.generation, std::u16string(), 0, 0));
      }
    }
  }
}

void SelectionOverlayController::Close(CloseReason reason) {
  session_->close_reason = reason;
  CloseUI();
}

base::CallbackListSubscription
SelectionOverlayController::RegisterOverlayClosedCallback(
    OverlayClosedCallback callback) {
  return overlay_closed_callbacks_.Add(std::move(callback));
}

void SelectionOverlayController::OnFocusedTabChanged(
    const FocusedTabData& tab_data) {
  UpdateForTabVisibility();
}

void SelectionOverlayController::UpdateForTabVisibility() {
  // When a Glic instance is active, both `OnFocusedTabChanged()` and
  // `OnActiveTabChanged()` run for the same tab switch.
  const bool is_backgrounded = state() == State::kBackground;
  if (tab_->IsVisible() != is_backgrounded) {
    return;
  }
  if (tab_->IsVisible()) {
    TabForegrounded(tab_);
  } else if (!tab_->IsActivated()) {
    TabDeactivated(tab_);
  }
}

void SelectionOverlayController::OnOverlayWebViewFocused(
    views::WebView* web_view) {
  CHECK(tab_);
  if (!tab_->IsVisible() || tab_->IsActivated()) {
    return;
  }
  TabStripModel* tab_strip_model =
      tab_->GetBrowserWindowInterface()->GetTabStripModel();
  tab_strip_model->ActivateTabAt(tab_strip_model->GetIndexOfTab(tab_));
}

void SelectionOverlayController::OnSplitTabChanged(
    const SplitTabChange& change) {
  if (IsOverlayShowing()) {
    SetOverlayRoundedCorner();
  }
  if (!tab_->IsSplit()) {
    return;
  }
  if (tab_->GetSplit() != change.split_id) {
    return;
  }
  // Not all split view changes require re-parenting. Only reparent the overlay
  // view if the overlay view's parent is different from the container view that
  // the current WebContents is inside.
  if (overlay_view_ && overlay_view_->parent() != GetHostView()) {
    TabDeactivated(tab_);
    TabForegrounded(tab_);
  }
}

void SelectionOverlayController::CloseUI() {
  if (state() == State::kOff) {
    return;
  }
  CloseReason close_reason =
      session_->close_reason.value_or(CloseReason::kOther);
  Reset();
  OverlayBaseController::CloseUI();
  overlay_closed_callbacks_.Notify(close_reason);
}

void SelectionOverlayController::RequestSyncClose(
    DismissalSource dismissal_source) {
  switch (dismissal_source) {
    case DismissalSource::kPreselectionToastExitButton:
      Close(CloseReason::kCloseButton);
      break;
    case DismissalSource::kPreselectionToastEscapeKeyPress:
      Close(CloseReason::kEscapeKeyPress);
      break;
    default:
      Close(CloseReason::kOther);
      break;
  }
}

void SelectionOverlayController::InitializeOverlay() {
  // We can only continue once both the WebUI is bound and the initialization
  // data is processed and ready. If either of those conditions aren't met, we
  // exit early and wait for the other condition to call this method again.
  if (!session_->page || !session_->screenshot_available) {
    return;
  }

  InitializeOverlayImpl();

  CHECK(session_->page);
  session_->page->SetInteractionOptions(session_->interaction_options.Clone());
  session_->page->SetSidePanelOpen(IsSidePanelOpen());
  session_->page->ScreenshotReceived(session_->initial_rgb_screenshot);

  // Forward any pre-existing selections (e.g. from a text selection prompt) to
  // the WebUI so they are rendered immediately upon initialization. Not
  // `SyncRegions()`: the redacted screenshot that it checks the regions
  // against may not be here yet.
  if (!session_->selected_regions.empty()) {
    std::vector<selection::SelectedRegionPtr> regions;
    regions.reserve(session_->selected_regions.size());
    for (const auto& [id, region_data] : session_->selected_regions) {
      regions.push_back(region_data.region.Clone());
    }
    session_->page->SetPostRegionSelections(std::move(regions));
  }
}

void SelectionOverlayController::StartScreenshotFlow() {
  mojom::TabContextOptionsPtr options;
  if (session_->options) {
    options = session_->options->Clone();
  } else {
    options = mojom::TabContextOptions::New();
    options->viewport_screenshot = true;
    options->annotated_page_content = true;
  }

  // For region selection overlay, skip the screenshot size cap. See
  // https://crbug.com/512915349
  if (base::FeatureList::IsEnabled(kGlicSelectionOverlayFullSizeScreenshot) &&
      WouldCapDownscaleCapture(tab_, options->screenshot_collection_options)) {
    options->screenshot_collection_options.max_width = 0;
    options->screenshot_collection_options.max_height = 0;
  }

  auto progress_listener =
      std::make_unique<SelectionOverlayFetchPageProgressListener>(
          base::BindOnce(&SelectionOverlayController::OnScreenshotTaken,
                         session_weak_factory_.GetWeakPtr()),
          base::BindOnce(&SelectionOverlayController::OnScreenshotRedacted,
                         session_weak_factory_.GetWeakPtr()));
  FetchPageContext(tab_, *options,
                   base::BindOnce(&SelectionOverlayController::PageContextReady,
                                  session_weak_factory_.GetWeakPtr()),
                   std::move(progress_listener),
                   /*is_screenshot_annotated=*/true);
}

void SelectionOverlayController::NotifyOverlayClosing() {}

void SelectionOverlayController::OnScreenshotTaken(const SkBitmap& bitmap) {
  InitializeScreenshot(
      bitmap, base::BindOnce(&SelectionOverlayController::SetScreenshot,
                             session_weak_factory_.GetWeakPtr(), bitmap));
}

void SelectionOverlayController::OnScreenshotRedacted(const SkBitmap& bitmap) {
  session_->redacted_screenshot = bitmap;
  SyncRegionsToPanelIfPending();
}

void SelectionOverlayController::PageContextReady(
    base::expected<glic::mojom::GetContextResultPtr,
                   page_content_annotations::FetchPageContextErrorDetails>
        fetch_result) {
  if (!fetch_result.has_value() || !fetch_result.value()->is_tab_context()) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  session_->tab_context = std::move(fetch_result.value()->get_tab_context());
  if (!session_->tab_context->annotated_page_data ||
      !session_->tab_context->viewport_screenshot) {
    RequestSyncClose(DismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  if (IsSelectionOverlayPromptEnabled()) {
    if (auto* suggestion_service = ::selection::SuggestionService::From(tab_);
        suggestion_service && !session_->redacted_screenshot.empty()) {
      optimization_guide::proto::AnnotatedPageContent apc;
      if (session_->tab_context->annotated_page_data->annotated_page_content
              .has_value()) {
        if (auto unwrapped_apc =
                session_->tab_context->annotated_page_data
                    ->annotated_page_content
                    ->As<optimization_guide::proto::AnnotatedPageContent>()) {
          apc = std::move(*unwrapped_apc);
        }
      }
      suggestion_service->UpdateScreenContent(session_->redacted_screenshot,
                                              apc);
    }
  }

  SyncRegionsToPanelIfPending();
}

void SelectionOverlayController::SetScreenshot(const SkBitmap& screenshot,
                                               SkBitmap rgb_screenshot) {
  session_->initial_rgb_screenshot = std::move(rgb_screenshot);
  session_->screenshot_available = true;
  InitializeOverlay();
}

bool SelectionOverlayController::IsResultsSidePanelShowing() {
  return GlicSidePanelCoordinator::IsShowing(tab_);
}

GURL SelectionOverlayController::GetInitialURL() {
  return GURL(chrome::kChromeUIGlicSelectionOverlayURL);
}

int SelectionOverlayController::GetToolResourceId() {
  return IDS_GLIC_SELECTION_OVERLAY_RENDERER_LABEL;
}

ui::ElementIdentifier SelectionOverlayController::GetViewContainerId() const {
  return kGlicSelectionOverlayViewElementId;
}

SidePanelType SelectionOverlayController::GetSidePanelType() {
  return SidePanelType::kContent;
}

bool SelectionOverlayController::ShouldCloseSidePanel() {
  return false;
}

bool SelectionOverlayController::ShouldShowPreselectionBubble() {
  return IsOverlayActive() && session_->selected_regions.empty();
}

bool SelectionOverlayController::UseOverlayBlur() {
  return true;
}

void SelectionOverlayController::NotifyPageNavigated() {
  CloseUI();
}

void SelectionOverlayController::NotifyTabForegrounded() {}

void SelectionOverlayController::NotifyTabWillEnterBackground() {
  // Closing the preselection bubble hides its widget synchronously, which can
  // hand activation back to the browser window and re-enter `TabDeactivated()`.
  // Closing the bubble at the end of the callstack so the reentrance to
  // `TabDeactivated()` with kBackground is an early-out.
  ClosePreselectionBubbleImpl();
}

OverlayBaseController::PreselectionUIConfig
SelectionOverlayController::GetPreselectionBubbleConfig() {
  return {
      .message_string_id = IDS_GLIC_SELECTION_OVERLAY_PRESELECTION_BUBBLE_TEXT,
      .bubble_background_color = kColorGlicSelectionOverlayToast,
      .icon =
          &(features::IsRoundedIconsEnabled() ? vector_icons::kCropFreeIcon
                                              : vector_icons::kCropFreeOldIcon),
      .cancel_button_config = CancelButtonConfig{
          .color = kColorGlicSelectionOverlayToastCancelButton,
          .padding = gfx::Insets::VH(8, 16),
          .bubble_margins = gfx::Insets::VH(6, 14)}};
}

bool SelectionOverlayController::IsOverlayViewShared() const {
  // Glic's selection overlay's WebView is attached to the ContentsContainerView
  // which cannot be shared across multiple tabs. It also means glic's selection
  // overlay respects the split view.
  return false;
}

void SelectionOverlayController::ShowPreselectionBubble() {
  if (!ShouldShowPreselectionBubble()) {
    return;
  }
  OverlayBaseController::ShowPreselectionBubble();
}

void SelectionOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  OverlayBaseController::TabForegrounded(tab);
  // Layout can be happening as a result of
  // `OverlayBaseController::TabForegrounded()`, which means
  // `preselection_widget_anchor_` (from the base class) might not return the
  // correct coord in the screen. Post a task to let the layout finish then
  // reshow the bubble.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&SelectionOverlayController::ShowPreselectionBubble,
                     weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::DismissOverlay(
    selection::DismissOverlayReason reason) {
  switch (reason) {
    case selection::DismissOverlayReason::kCloseButton:
      Close(CloseReason::kCloseButton);
      break;
    case selection::DismissOverlayReason::kBackgroundClick:
      Close(CloseReason::kOther);
      break;
  }
}

void SelectionOverlayController::AdjustRegion(
    selection::SelectedRegionPtr target,
    bool is_using_keyboard) {
  session_->active_region_id = target->id;
  if (SelectedRegionData* region_data =
          base::FindOrNull(session_->selected_regions, target->id)) {
    region_data->region = std::move(target);
    region_data->selected_text.reset();
    region_data->text_surrounding_selection.reset();
    region_data->waiting_for_surrounding_text = false;
    region_data->suggestions.clear();
    region_data->suggestions_requested = false;
    region_data->suggestions_complete = false;
    region_data->generation++;
  } else {
    base::UnguessableToken id = target->id;
    session_->selected_regions.emplace(id,
                                       SelectedRegionData(std::move(target)));
  }

  SyncRegions(/*should_focus_panel=*/!is_using_keyboard);
}

void SelectionOverlayController::DeleteRegion(const base::UnguessableToken& id,
                                              bool is_using_keyboard) {
  if (session_->selected_regions.erase(id)) {
    if (session_->selected_regions.empty()) {
      session_->active_region_id.reset();
      Close(CloseReason::kCloseButton);
      return;
    }
    if (session_->active_region_id == id) {
      session_->active_region_id = session_->selected_regions.begin()->first;
    }
    SyncRegions(/*should_focus_panel=*/!is_using_keyboard);
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

void SelectionOverlayController::SubmitPrompt(const std::string& prompt) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox)) {
    return;
  }
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  if (service) {
    GlicInvokeOptions options(glic::Target(*tab_),
                              mojom::InvocationSource::kTextSelectionWidget);
    options.prompts.push_back(prompt);
    // TODO(b/556786015): Fix issue when side panel is not open.
    service->InvokeWithAutoSubmit(
        InvokeWithAutoSubmitPasskeyProvider::GetPassKey(), std::move(options));
    if (session_->owner == Session::Owner::kDirect) {
      Close();
    }
  }
}

void SelectionOverlayController::GetSuggestedActions(
    mojo::PendingRemote<selection::SuggestedActionsListener> listener) {
  session_->suggested_actions_listener.reset();
  session_->suggested_actions_listener.Bind(std::move(listener));
  GetSuggestedActionsImpl(base::BindRepeating(
      [](mojo::Remote<selection::SuggestedActionsListener>* remote,
         const std::vector<selection::SuggestedActionPtr>& actions) {
        if (remote->is_bound()) {
          (*remote)->OnSuggestedActionsAvailable(base::ToVector(
              actions, [](const auto& action) { return action.Clone(); }));
        }
      },
      base::Unretained(&session_->suggested_actions_listener)));
}

void SelectionOverlayController::GetSuggestedActionsForTesting(
    SuggestedActionsCallback callback) {
  session_->suggested_actions_callback_for_testing = std::move(callback);
  GetSuggestedActionsImpl(session_->suggested_actions_callback_for_testing);
}

void SelectionOverlayController::GetSuggestedActionsImpl(
    SuggestedActionsCallback callback) {
  if (!IsSelectionOverlayPromptEnabled() ||
      !session_->active_region_id.has_value()) {
    callback.Run({});
    return;
  }

  SelectedRegionData* region_data =
      base::FindOrNull(session_->selected_regions, *session_->active_region_id);
  if (!region_data) {
    callback.Run({});
    return;
  }

  if (region_data->suggestions_requested) {
    if (!region_data->suggestions.empty() ||
        region_data->suggestions_complete) {
      std::vector<selection::SuggestedActionPtr> actions =
          base::ToVector(region_data->suggestions, [](const auto& item) {
            return selection::SuggestedAction::New(
                item.first, base::UTF16ToUTF8(item.second->GetLabel()),
                GetIconForTool(item.second->GetToolId()),
                item.second->GetAction());
          });
      callback.Run(std::move(actions));
    }
    return;
  }

  ::selection::SuggestionService* suggestion_service =
      ::selection::SuggestionService::From(tab_);
  if (!suggestion_service || session_->redacted_screenshot.empty()) {
    callback.Run({});
    return;
  }

  RequestNewSuggestions(*region_data);
}

void SelectionOverlayController::OnTextSurroundingSelectionAvailable(
    const base::UnguessableToken& region_id,
    uint64_t generation,
    const std::u16string& content,
    uint32_t start_offset,
    uint32_t end_offset) {
  session_->surrounding_text_timer.Stop();
  SelectedRegionData* region_data =
      base::FindOrNull(session_->selected_regions, region_id);
  if (!region_data || region_data->generation != generation ||
      !region_data->waiting_for_surrounding_text) {
    return;
  }
  region_data->waiting_for_surrounding_text = false;
  if (!content.empty() && start_offset < end_offset &&
      end_offset <= content.length()) {
    region_data->selected_text =
        content.substr(start_offset, end_offset - start_offset);
    region_data->text_surrounding_selection = content;
  }
  if (session_->active_region_id == region_id &&
      !region_data->suggestions_requested &&
      (session_->suggested_actions_listener.is_bound() ||
       session_->suggested_actions_callback_for_testing)) {
    RequestNewSuggestions(*region_data);
  }
}

void SelectionOverlayController::RequestNewSuggestions(
    SelectedRegionData& region_data) {
  ::selection::SuggestionService* suggestion_service =
      ::selection::SuggestionService::From(tab_);
  CHECK(suggestion_service);
  CHECK(!session_->redacted_screenshot.empty());
  if (region_data.waiting_for_surrounding_text) {
    return;
  }

  region_data.suggestions_requested = true;
  uint64_t generation = ++region_data.generation;
  base::UnguessableToken region_id = region_data.region->id;

  ::selection::AreaOfInterest aoi;
  aoi.screenshot = session_->redacted_screenshot;
  aoi.selected_text = region_data.selected_text;
  aoi.text_surrounding_selection = region_data.text_surrounding_selection;
  const auto& region = region_data.region;
  if (region->shape->is_rect()) {
    gfx::RectF gfx_rect = GetRectForRegion(session_->redacted_screenshot,
                                           region->shape->get_rect());
    aoi.bounds = gfx::ToEnclosingRect(gfx_rect);
  } else if (region->shape->is_polyline()) {
    aoi.bounds = base::ToVector(
        region->shape->get_polyline(), [&](const gfx::PointF& pt) {
          return gfx::Point(
              static_cast<int>(pt.x() * session_->redacted_screenshot.width()),
              static_cast<int>(pt.y() *
                               session_->redacted_screenshot.height()));
        });
  }
  if (session_->tab_context && session_->tab_context->annotated_page_data &&
      session_->tab_context->annotated_page_data->annotated_page_content
          .has_value()) {
    if (auto apc =
            session_->tab_context->annotated_page_data->annotated_page_content
                ->As<optimization_guide::proto::AnnotatedPageContent>()) {
      aoi.apc = std::move(*apc);
    }
  }

  if (base::FeatureList::IsEnabled(kStaticSelectionSuggestions)) {
    std::vector<std::unique_ptr<::selection::Suggestion>> static_suggestions;
    if (!base::FeatureList::IsEnabled(
            features::kGlicSelectionOverlayPromptBox)) {
      static_suggestions.emplace_back(std::make_unique<SelectionSuggestion>(
          CHECK_DEREF(tab_), u"Ask Gemini"));
    }
    if (base::FeatureList::IsEnabled(kQuickAnswersSelectionSuggestions) &&
        !base::FeatureList::IsEnabled(
            ::selection::kSmartSelectionServerSuggestions)) {
      if (auto suggestion =
              QuickAnswersTool::CreateSuggestion(CHECK_DEREF(tab_), aoi)) {
        static_suggestions.push_back(std::move(suggestion));
      }
    }
    if (!static_suggestions.empty()) {
      OnSuggestionsReceived(region_id, generation, /*complete=*/false,
                            std::move(static_suggestions));
    }
  }

  suggestion_service->RequestSuggestions(
      std::move(aoi),
      base::BindOnce(&SelectionOverlayController::OnSuggestionsReceived,
                     session_weak_factory_.GetWeakPtr(), region_id, generation,
                     /*complete=*/true));
}

void SelectionOverlayController::OnSuggestionsReceived(
    const base::UnguessableToken& region_id,
    uint64_t generation,
    bool complete,
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions) {
  SelectedRegionData* region_data =
      base::FindOrNull(session_->selected_regions, region_id);
  if (!region_data || region_data->generation != generation) {
    return;
  }

  if (complete) {
    region_data->suggestions_complete = true;
  }

  region_data->suggestions.reserve(region_data->suggestions.size() +
                                   suggestions.size());
  std::vector<selection::SuggestedActionPtr> actions;
  actions.reserve(suggestions.size());
  for (auto& suggestion : suggestions) {
    auto action_id = base::UnguessableToken::Create();
    std::string label = base::UTF16ToUTF8(suggestion->GetLabel());
    selection::SuggestedActionIcon icon =
        GetIconForTool(suggestion->GetToolId());
    ::selection::mojom::ActionPtr action = suggestion->GetAction();
    suggestion->OnSuggestionPresented();
    region_data->suggestions.emplace_back(action_id, std::move(suggestion));
    actions.push_back(selection::SuggestedAction::New(
        action_id, std::move(label), icon, std::move(action)));
  }

  // This appends the new ones. The suggested actions listener will be
  // unbound when the user makes a new selection.
  if (session_->active_region_id == region_id &&
      (complete || !suggestions.empty())) {
    if (session_->suggested_actions_callback_for_testing) {
      session_->suggested_actions_callback_for_testing.Run(actions);
    }
    if (session_->suggested_actions_listener.is_bound()) {
      session_->suggested_actions_listener->OnSuggestedActionsAvailable(
          std::move(actions));
    }
  }
}

void SelectionOverlayController::ExecuteSuggestedAction(
    const base::UnguessableToken& action_id,
    mojo::GenericPendingAssociatedReceiver channel) {
  if (!IsSelectionOverlayPromptEnabled()) {
    return;
  }
  ::selection::Suggestion* matched_suggestion = nullptr;
  for (auto& [region_id, region_data] : session_->selected_regions) {
    for (auto& [id, suggestion] : region_data.suggestions) {
      if (id == action_id) {
        matched_suggestion = suggestion.get();
        break;
      }
    }
    if (matched_suggestion) {
      break;
    }
  }
  if (!matched_suggestion) {
    session_->receiver.ReportBadMessage("Unknown suggested action ID.");
    return;
  }

  auto tag = matched_suggestion->GetAction()->which();

  switch (tag) {
    case ::selection::mojom::Action::Tag::kHandoff:
      // kHandoff should not have a channel since the surface is going away.
      if (channel) {
        session_->receiver.ReportBadMessage(
            "Channel supplied for a handoff action.");
        return;
      }
      matched_suggestion->Execute(mojo::GenericPendingAssociatedReceiver());
      if (session_->owner == Session::Owner::kDirect) {
        Close();
      }
      return;
    case ::selection::mojom::Action::Tag::kInlineFulfillment:
      // Fulfillment must have a channel.
      if (!channel) {
        session_->receiver.ReportBadMessage(
            "Inline fulfillment requires a channel.");
        return;
      }
      if (channel.interface_name() != matched_suggestion->interface_name()) {
        session_->receiver.ReportBadMessage(
            "Channel interface does not match.");
        return;
      }
      matched_suggestion->Execute(std::move(channel));
      return;
  }
}

void SelectionOverlayController::OpenSettings() {
  Close();
  if (BrowserWindowInterface* browser = tab_->GetBrowserWindowInterface()) {
    chrome::ShowContentSettingsExceptions(browser,
                                          ContentSettingsType::INLINE_CUE_MENU);
  }
}

void SelectionOverlayController::OpenDogfoodFeedback() {
  if (!base::FeatureList::IsEnabled(
          features::kGlicSelectionOverlayDogfoodFeedback)) {
    return;
  }
  const GURL url(features::kGlicSelectionOverlayDogfoodFeedbackUrl.Get());
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return;
  }
  Close();
  CHECK_DEREF(tab_->GetBrowserWindowInterface())
      .OpenGURL(url, WindowOpenDisposition::kNewForegroundTab);
}

void SelectionOverlayController::Reset() {
  // Drop the callbacks still pending for the old session (e.g. the page context
  // fetch), so they can't land in the next one.
  session_weak_factory_.InvalidateWeakPtrs();
  session_ = std::make_unique<Session>(this);
}

SelectionOverlayController::CapturedRegions
SelectionOverlayController::GetCapturedRegions() const {
  CHECK(!session_->redacted_screenshot.empty());
  CapturedRegions captured_regions;
  // TODO(http://b/452032491): Reconsider what happens if the regions overlap.
  // TODO(http://b/452032491): Currently this class is only used once per
  // selection and only one region is supported, so it is fine to always loop
  // through all the regions. Revisit once we expand the selections.
  for (const auto& [id, region_data] : session_->selected_regions) {
    const auto& region = region_data.region;
    if (region->shape->is_rect()) {
      gfx::RectF gfx_rect_on_canvas = GetRectForRegion(
          session_->redacted_screenshot, region->shape->get_rect());
      SkRect rect_on_canvas = gfx::RectFToSkRect(gfx_rect_on_canvas);
      if (!rect_on_canvas.isEmpty() &&
          session_->redacted_screenshot.bounds().contains(rect_on_canvas)) {
        captured_regions.emplace_back(
            id, glic::mojom::CapturedRegion::NewRect(
                    gfx::ToEnclosingRect(gfx_rect_on_canvas)));
      } else {
        // TODO(b/485358530): Record proper histograms for the error case.
        LOG(ERROR) << "Invalid region selected "
                   << region->shape->get_rect().ToString();
      }
    } else if (region->shape->is_polyline()) {
      if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionLine)) {
        std::vector<gfx::Point> line_points;
        bool all_points_valid = true;
        for (const auto& point : region->shape->get_polyline()) {
          int x = std::round(point.x() * session_->redacted_screenshot.width());
          int y =
              std::round(point.y() * session_->redacted_screenshot.height());

          if (x >= 0 && x <= session_->redacted_screenshot.width() && y >= 0 &&
              y <= session_->redacted_screenshot.height()) {
            // Map width/height to width-1/height-1 to be valid pixel indices.
            int pixel_x =
                (x == session_->redacted_screenshot.width()) ? x - 1 : x;
            int pixel_y =
                (y == session_->redacted_screenshot.height()) ? y - 1 : y;
            line_points.emplace_back(pixel_x, pixel_y);
          } else {
            all_points_valid = false;
            break;
          }
        }
        if (all_points_valid && !line_points.empty()) {
          captured_regions.emplace_back(
              id,
              glic::mojom::CapturedRegion::NewPolyline(std::move(line_points)));
        } else {
          LOG(ERROR) << "Invalid polyline selected";
        }
      } else {
        LOG(ERROR) << "Received polyline but kGlicRegionSelectionLine feature "
                      "is disabled.";
      }
    }
  }
  return captured_regions;
}

void SelectionOverlayController::SyncRegions(bool should_focus_panel) {
  // The regions are checked against the redacted screenshot, so nothing can
  // be sent before it's here. The overlay gets them when it initializes; the
  // panel once `SyncRegionsToPanelIfPending()` runs.
  if (session_->redacted_screenshot.empty()) {
    session_->panel_sync_pending = true;
    return;
  }
  CapturedRegions captured_regions = GetCapturedRegions();

  if (session_->page) {
    std::vector<selection::SelectedRegionPtr> regions;
    regions.reserve(captured_regions.size());
    for (const auto& [id, captured_region] : captured_regions) {
      regions.push_back(
          CHECK_DEREF(base::FindOrNull(session_->selected_regions, id))
              .region.Clone());
    }
    session_->page->SetPostRegionSelections(std::move(regions));
  }

  // The web client can't use the regions without the page context.
  if (!session_->tab_context) {
    session_->panel_sync_pending = true;
    return;
  }
  SendRegionsToPanel(std::move(captured_regions), should_focus_panel);
}

void SelectionOverlayController::SyncRegionsToPanelIfPending() {
  if (!session_->panel_sync_pending || !session_->tab_context ||
      session_->redacted_screenshot.empty()) {
    return;
  }
  // Keep focus on the overlay so that the user can adjust the region.
  SendRegionsToPanel(GetCapturedRegions(), /*should_focus_panel=*/false);
}

void SelectionOverlayController::SendRegionsToPanel(
    CapturedRegions captured_regions,
    bool should_focus_panel) {
  session_->panel_sync_pending = false;
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  GlicInstance* instance = service->GetInstanceForTab(tab_);
  if (!instance) {
    return;
  }
  instance->SendAdditionalContext(
      CreateAdditionalContext(std::move(captured_regions)));
  // If the event that triggered this was initiated via keyboard, do not
  // focus the panel to avoid stealing focus away from the selection pane.
  // Also, if line selection or the selection prompt box is enabled, keep
  // focus on the selection pane to allow subsequent keyboard interactions.
  bool line_selection_enabled =
      base::FeatureList::IsEnabled(features::kGlicRegionSelectionLine);
  bool prompt_box_enabled =
      base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox);
  if (should_focus_panel && !line_selection_enabled && !prompt_box_enabled) {
    instance->FocusIfActive();
  }
}

glic::mojom::AdditionalContextPtr
SelectionOverlayController::CreateAdditionalContext(CapturedRegions regions) {
  auto context = glic::mojom::AdditionalContext::New();
  std::vector<glic::mojom::AdditionalContextPartPtr> parts;
  mojom::TabContextResultPtr tab_context = session_->tab_context.Clone();
  parts.push_back(glic::mojom::AdditionalContextPart::NewTabContext(
      std::move(tab_context)));
  for (auto& region : regions) {
    parts.push_back(glic::mojom::AdditionalContextPart::NewPendingRegion(
        glic::mojom::PendingCapturedRegion::New(region.first,
                                                region.second.Clone())));
    parts.push_back(glic::mojom::AdditionalContextPart::NewRegion(
        std::move(region.second)));
  }
  context->source = glic::mojom::AdditionalContextSource::kRegionSelection;
  context->tab_id = tab_->GetHandle().raw_value();
  context->parts = std::move(parts);
  return context;
}

}  // namespace glic
