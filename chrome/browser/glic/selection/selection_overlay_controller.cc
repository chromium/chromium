// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/selection/selection_overlay_controller.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "base/check_deref.h"
#include "base/containers/map_util.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/glic/glic_pref_names.h"
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
#include "chrome/browser/glic/selection/gemini_prompt_endpoint.h"
#include "chrome/browser/glic/selection/quick_answers_tool.h"
#include "chrome/browser/glic/selection/selection_overlay_untrusted_ui.h"
#include "chrome/browser/glic/selection/selection_suggestion.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/selection/features.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_observer.h"
#include "components/optimization_guide/core/feature_registry/feature_keys.h"
#include "components/optimization_guide/proto/features/smart_selection_suggestions.pb.h"
#include "components/page_content_annotations/content/page_context_fetcher_options.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "third_party/blink/public/mojom/content_extraction/ai_page_content.mojom.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/actions/actions.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
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

// Kill switch for loading the overlay WebUI while the selection chip is
// hovered, so a click can show it without waiting for the WebUI to boot.
// Default-enabled platforms: desktop only (selection target is
// assert(!is_android)).
BASE_FEATURE(kGlicSelectionOverlayHoverPrewarm,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kStaticSelectionSuggestions, base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for sending the selected region with the overlay's prompt.
// Default-enabled platforms: desktop only (selection target is
// assert(!is_android)).
BASE_FEATURE(kGlicSelectionOverlaySendRegionWithPrompt,
             base::FEATURE_ENABLED_BY_DEFAULT);

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
      return base::FeatureList::IsEnabled(
                 features::kGlicSelectionOverlayPromptBox)
                 ? selection::SuggestedActionIcon::kNone
                 : selection::SuggestedActionIcon::kSpark;
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

std::optional<gfx::Rect> ConvertRectToPixels(const gfx::RectF& region,
                                             const SkBitmap& screenshot) {
  if (screenshot.empty()) {
    return std::nullopt;
  }
  const gfx::RectF gfx_rect = GetRectForRegion(screenshot, region);
  const SkRect rect_on_canvas = gfx::RectFToSkRect(gfx_rect);
  if (rect_on_canvas.isEmpty() ||
      !screenshot.bounds().contains(rect_on_canvas)) {
    return std::nullopt;
  }
  return gfx::ToEnclosingRect(gfx_rect);
}

std::optional<std::vector<gfx::Point>> ConvertPolylineToPixels(
    const std::vector<gfx::PointF>& polyline,
    const SkBitmap& screenshot) {
  if (polyline.empty() || screenshot.empty()) {
    return std::nullopt;
  }
  const int width = screenshot.width();
  const int height = screenshot.height();
  std::vector<gfx::Point> line_points;
  line_points.reserve(polyline.size());
  for (const gfx::PointF& pt : polyline) {
    if (!std::isfinite(pt.x()) || !std::isfinite(pt.y())) {
      return std::nullopt;
    }
    const double scaled_x = std::round(static_cast<double>(pt.x()) * width);
    const double scaled_y = std::round(static_cast<double>(pt.y()) * height);
    if (scaled_x < 0.0 || scaled_x > width || scaled_y < 0.0 ||
        scaled_y > height) {
      return std::nullopt;
    }
    const int x = static_cast<int>(scaled_x);
    const int y = static_cast<int>(scaled_y);
    // Map width/height to width-1/height-1 to be valid pixel indices.
    const int pixel_x = (x == width) ? x - 1 : x;
    const int pixel_y = (y == height) ? y - 1 : y;
    line_points.emplace_back(pixel_x, pixel_y);
  }
  return line_points;
}

constexpr int kSelectionPaddingDip = 5;
constexpr int kMaxSurroundingTextLength = 2000;
constexpr base::TimeDelta kSurroundingTextTimeout = base::Seconds(1);

selection::SelectedRegionPtr CreateRegionFromBounds(gfx::Rect selection_bounds,
                                                    const gfx::Rect& tab_bounds,
                                                    bool apply_padding) {
  if (selection_bounds.IsEmpty() || tab_bounds.IsEmpty()) {
    return nullptr;
  }

  if (apply_padding) {
    selection_bounds.Outset(kSelectionPaddingDip);
  }

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
  prompt_endpoints_.push_back(std::make_unique<GeminiPromptEndpoint>(
      InvokeWithAutoSubmitPasskeyProvider::GetPassKey()));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&SelectionOverlayController::WillDiscardContents,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &SelectionOverlayController::WillDetach, weak_factory_.GetWeakPtr())));
  // Unlike the session's active-tab subscription, this also fires while idle,
  // so an unused prewarm is only kept for the active tab.
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&SelectionOverlayController::WillDeactivate,
                          weak_factory_.GetWeakPtr())));
  if (auto* lens_search_controller = LensSearchController::From(tab_)) {
    tab_subscriptions_.push_back(
        lens_search_controller->RegisterWillOpenCallback(base::BindRepeating(
            &SelectionOverlayController::CloseUI, weak_factory_.GetWeakPtr())));
  }
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
        if (controller && controller->IsOverlayShowing()) {
          controller->Close(CloseReason::kEscapeKeyPress);
          return true;
        }
        return false;
      },
      weak_factory_.GetWeakPtr()));
}

SelectionOverlayController::~SelectionOverlayController() {
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
  CancelPendingEndpointRequests();
  CloseUI();
  Observe(new_contents);
}

void SelectionOverlayController::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  CancelPendingEndpointRequests();
  CloseUI();
}

void SelectionOverlayController::WillDeactivate(tabs::TabInterface* tab) {
  if (state() == State::kOff) {
    prewarmer_.Cancel();
  }
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
  tabs::TabInterface* const active_tab = window->GetActiveTabInterface();
  SelectionOverlayController* const active_controller =
      active_tab ? FromTabWebContents(active_tab->GetContents()) : nullptr;
  UpdateToolbarButtonHighlight(active_controller &&
                               active_controller->IsOverlayShowing());
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

void SelectionOverlayController::ObservePageContextEligibility() {
  auto* helper = tabs::PageContextEligibilityHelper::From(tab_);
  if (!helper) {
    session_->eligibility_subscription = base::CallbackListSubscription();
    return;
  }
  session_->eligibility_subscription =
      helper->RegisterEligibilityChangeCallback(base::BindRepeating(
          &SelectionOverlayController::OnPageContextEligibilityChanged,
          session_weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::OnSidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (session_->page) {
    session_->page->SetSidePanelOpen(IsSidePanelOpen());
  }
  if (state == GlicSidePanelCoordinator::State::kShown) {
    PostHandSessionToPanel();
  }
}

void SelectionOverlayController::PostHandSessionToPanel() {
  if (session_->owner != Session::Owner::kDirect ||
      session_->handoff_to_panel_pending) {
    return;
  }
  session_->handoff_to_panel_pending = true;
  // Not right away: the panel may be opening for this session's own
  // hand-off (`SubmitPrompt()`, a suggested action), which closes the
  // session before this stack unwinds. `Reset()` then drops the task.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SelectionOverlayController::HandSessionToPanel,
                                session_weak_factory_.GetWeakPtr()));
}

void SelectionOverlayController::HandSessionToPanel() {
  if (session_->owner != Session::Owner::kDirect ||
      !GlicSidePanelCoordinator::IsShowing(tab_)) {
    session_->handoff_to_panel_pending = false;
    return;
  }
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  if (!service) {
    session_->handoff_to_panel_pending = false;
    return;
  }
  // The same request the capture hotkey makes while the panel is open, marked
  // as a hand-off so `CaptureRegion()` can tell its answer apart from a user
  // clicking "+". The glic web client enters its capture mode and calls
  // `CaptureRegion()`, which adopts this session instead of starting one.
  // TODO(b:579008191): Use a hand-off invocation source so hand-offs don't
  // count as hotkey presses in metrics.
  GlicInvokeOptions options(Target(*tab_),
                            mojom::InvocationPayload::NewCaptureRegion(
                                mojom::CaptureRegionPayload::New(
                                    /*capture_region_handoff=*/true)));
  options.wait_for_panel_open = true;
  options.on_success =
      base::BindOnce(&SelectionOverlayController::OnHandSessionToPanelComplete,
                     session_weak_factory_.GetWeakPtr());
  options.on_error = base::IgnoreArgs<GlicInvokeError>(
      base::BindOnce(&SelectionOverlayController::OnHandSessionToPanelComplete,
                     session_weak_factory_.GetWeakPtr()));
  service->Invoke(std::move(options));
}

void SelectionOverlayController::OnHandSessionToPanelComplete() {
  session_->handoff_to_panel_pending = false;
}

void SelectionOverlayController::AdoptSession(
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  session_->owner = Session::Owner::kPanel;
  session_->handoff_to_panel_pending = false;
  BindCaptureRegionObserver(std::move(observer));
  // Note: do not invalidate cached or in-flight region suggestions here.
  // `SetPromptEndpoints()` only updates the available endpoint list and does
  // not tell the overlay to drop or refetch displayed chips; clearing them here
  // would drop in-flight requests and leave stale chip IDs that hit
  // `ReportBadMessage` on click. `SuggestionService::RequestSuggestions()` does
  // not currently vary results by endpoint.
  // TODO(b/587425707): If suggestions become endpoint-dependent, have the
  // overlay refetch on `SetPromptEndpoints()` before invalidating here.
  session_->active_endpoint_id = ResolveInitialPromptEndpointId();
  if (session_->page && session_->active_endpoint_id.has_value()) {
    session_->page->SetPromptEndpoints(GetAvailablePromptEndpointInfos(),
                                       *session_->active_endpoint_id);
  }
  if (!session_->selected_regions.empty() && !HasOnlyTextSelection()) {
    SyncRegionsToPanel(/*should_focus_panel=*/false);
  }
}

bool SelectionOverlayController::IsSidePanelOpen() const {
  // A backgrounded panel (its tab isn't in front) isn't closed: it shows again
  // with the tab. Not `kShown` only, which would flip the overlay to closed and
  // back on every tab switch.
  return GlicSidePanelCoordinator::IsShowingOrBackgrounded(tab_);
}

void SelectionOverlayController::TabDeactivated(tabs::TabInterface* tab) {
  // Time spent in the background is not render latency, so drop the sample.
  session_->show_requested_time = base::TimeTicks();
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
  if (!tab_web_contents) {
    return nullptr;
  }
  auto* tab = tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  if (!tab) {
    return nullptr;
  }
  return Get(tab->GetUnownedUserDataHost());
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

void SelectionOverlayController::MaybePrewarmOverlay(
    selection::InteractionOptionsPtr interaction_options) {
  if (!base::FeatureList::IsEnabled(kGlicSelectionOverlayHoverPrewarm) ||
      state() != State::kOff || !tab_->IsActivated() || !CanStartSession()) {
    return;
  }
  if (prewarmer_.IsUsable(interaction_options)) {
    prewarmer_.Extend();
    return;
  }
  views::View* host_view = GetHostView();
  if (!host_view || !host_view->GetWidget()) {
    return;
  }
  prewarmer_.Start(CreateOverlayWebContents(), *host_view->GetWidget(),
                   std::move(interaction_options));
}

void SelectionOverlayController::CancelPrewarm() {
  prewarmer_.Cancel();
}

selection::InteractionOptionsPtr
SelectionOverlayController::GetPrewarmInteractionOptions() const {
  return prewarmer_.GetInteractionOptions();
}

bool SelectionOverlayController::CanBindOverlay(
    content::WebContents* overlay_contents) {
  views::WebView* active_web_view = GetOverlayWebView();
  return !overlay_page_bound_ &&
         (state() == State::kStartingWebUI || state() == State::kBackground) &&
         active_web_view &&
         active_web_view->GetWebContents() == overlay_contents;
}

bool SelectionOverlayController::IsPrewarmedOverlay(
    content::WebContents* overlay_contents) const {
  return prewarmer_.Contains(overlay_contents);
}

std::unique_ptr<content::WebContents>
SelectionOverlayController::TakePrewarmedOverlayWebContents() {
  if (!prewarmer_.IsUsable(session_->interaction_options)) {
    prewarmer_.Cancel();
    return nullptr;
  }
  return prewarmer_.Take();
}

void SelectionOverlayController::MaybeAdoptPrewarmedOverlay() {
  if (state() != State::kScreenshot) {
    return;
  }
  if (!prewarmer_.IsUsable(session_->interaction_options)) {
    prewarmer_.Cancel();
    return;
  }
  MaybeStartOverlayWebUI();
  session_->prewarm_ready = BindPendingPageHandler();
}

bool SelectionOverlayController::BindPendingPageHandler() {
  views::WebView* web_view = GetOverlayWebView();
  if (!web_view || !web_view->GetWebContents()) {
    return false;
  }
  content::WebUI* web_ui = web_view->GetWebContents()->GetWebUI();
  if (!web_ui || !web_ui->GetController()) {
    return false;
  }
  auto* untrusted_ui =
      web_ui->GetController()->GetAs<SelectionOverlayUntrustedUI>();
  return untrusted_ui && untrusted_ui->BindPendingPageHandler();
}

void SelectionOverlayController::BindOverlay(
    mojo::PendingReceiver<selection::SelectionOverlayPageHandler> receiver,
    mojo::PendingRemote<selection::SelectionOverlayPage> page) {
  // The tab may have been backgrounded while the WebUI was starting.
  CHECK(state() == State::kStartingWebUI || state() == State::kBackground)
      << base::ToString(state());

  overlay_page_bound_ = true;
  session_->receiver.Bind(std::move(receiver));
  session_->receiver.set_disconnect_handler(base::BindOnce(
      &SelectionOverlayController::Reset, session_weak_factory_.GetWeakPtr()));
  session_->page.Bind(std::move(page));
  session_->page->SetInteractionOptions(session_->interaction_options.Clone());
  if (session_->active_endpoint_id.has_value()) {
    session_->page->SetPromptEndpoints(GetAvailablePromptEndpointInfos(),
                                       *session_->active_endpoint_id);
  }

  if (views::WebView* web_view = GetOverlayWebView()) {
    session_->overlay_web_view_focus_subscription =
        web_view->AddWebContentsFocusedCallback(base::BindRepeating(
            &SelectionOverlayController::OnOverlayWebViewFocused,
            session_weak_factory_.GetWeakPtr()));
  }

  InitializeOverlay();
}

void SelectionOverlayController::BindCaptureRegionObserver(
    mojo::PendingRemote<mojom::CaptureRegionObserver> observer) {
  if (session_->capture_region_observer.is_bound()) {
    // `CaptureRegion()` rejects a second capture on a session the client
    // already has before getting here, so this is a misbehaving client. Fail
    // the new request and leave the session as it is. No CHECK: this is
    // renderer input.
    LOG(ERROR) << "capture_region_observer is already bound. State " << state();
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    return;
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
    mojom::TabContextOptionsPtr options,
    bool capture_region_handoff) {
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
  if (selection_overlay_controller->state() != State::kOff) {
    if (selection_overlay_controller->session_->capture_region_observer
            .is_bound()) {
      // The web client already has this session.
      mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
      remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                       mojom::CaptureRegionErrorReason::kUnknown);
      LOG(ERROR) << "Overlay is still showing for " << web_contents->GetURL();
      return;
    }
    // The session started from the page while the panel was closed, and the
    // panel asked for a capture once it opened (`HandSessionToPanel()`). It
    // takes this session over instead of starting another. The session keeps
    // the page context it fetched; `options` is not applied to it.
    selection_overlay_controller->AdoptSession(std::move(observer));
    return;
  }
  if (capture_region_handoff) {
    // The glic web client is answering `HandSessionToPanel()`, but the user
    // dismissed the overlay in the meantime. Don't bring it back.
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    return;
  }
  selection_overlay_controller->CloseLensOverlayIfNeeded();
  if (!selection_overlay_controller->CanStartSession()) {
    mojo::Remote<mojom::CaptureRegionObserver> remote(std::move(observer));
    remote->OnUpdate(mojom::CaptureRegionResultPtr(),
                     mojom::CaptureRegionErrorReason::kUnknown);
    return;
  }
  // When the small chip or overlay prompt box is enabled, pre-select the tab's
  // current text selection.
  if (base::FeatureList::IsEnabled(features::kGlicSelectionSmallChip) ||
      base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox)) {
    if (auto* selection_observer = GlicSelectionObserver::From(tab)) {
      if (std::optional<gfx::Rect> bounds =
              selection_observer->GetCurrentSelectionBounds()) {
        selection_overlay_controller->SetRegionFromBounds(
            selection_observer->GetSelectedFrame(), *bounds);
        const bool has_region =
            selection_overlay_controller->GetSelectedRegionCount() > 0;
        if (has_region) {
          // Don't call `SyncRegions()`: `GlicSelectionObserver` already sent
          // the selected text to the panel, and `InitializeOverlay()` forwards
          // `selected_regions` to the overlay WebUI.
          // TODO(b:579078690): Clean this up once text and region selections
          // use one path.
          // Same as the other text pre-selection paths: the text selection is
          // fixed and opens directly in the post-selection state.
          selection_overlay_controller->session_->interaction_options =
              selection::InteractionOptions::New(
                  /*hide_handles=*/true, /*skip_intro_animation=*/true);
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
  auto* identity_manager =
      IdentityManagerFactory::GetForProfile(tab_->GetProfile());
  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    LOG(ERROR) << "Not signed in for " << web_contents->GetURL();
    return false;
  }
  if (!IsPageContextEligible()) {
    LOG(ERROR) << "Page context is not eligible for " << web_contents->GetURL();
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

void SelectionOverlayController::CloseLensOverlayIfNeeded() {
  if (auto* lens_search_controller = LensSearchController::From(tab_);
      lens_search_controller && !lens_search_controller->IsOff()) {
    lens_search_controller->CloseLensSync(
        lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void SelectionOverlayController::Show(mojom::TabContextOptionsPtr options,
                                      ::selection::EntryPoint entry_point) {
  CloseLensOverlayIfNeeded();
  if (!CanStartSession()) {
    return;
  }
  session_->entry_point = entry_point;
  ShowImpl(std::move(options));
}

void SelectionOverlayController::ShowImpl(mojom::TabContextOptionsPtr options) {
  if (session_->entry_point == ::selection::EntryPoint::kInlineCue) {
    // The chip click reaches here synchronously, after `CanStartSession()`, so
    // this is the click time for accepted clicks only.
    session_->show_requested_time = base::TimeTicks::Now();
  }
  session_->options = std::move(options);
  session_->active_endpoint_id = ResolveInitialPromptEndpointId();
  if (IsSelectionOverlayPromptEnabled() && session_->entry_point.has_value()) {
    session_->metrics_logger.emplace(*session_->entry_point,
                                     tab_->GetContents()->GetContentsMimeType(),
                                     !session_->selected_regions.empty());
  }
  for (const auto& endpoint : prompt_endpoints_) {
    if (IsEndpointAllowedForSession(endpoint->GetId()) &&
        endpoint->IsAvailable(*tab_)) {
      endpoint->Prepare(*tab_);
    }
  }
  if (::selection::SuggestionService* const suggestion_service =
          ::selection::SuggestionService::From(tab_)) {
    suggestion_service->MaybePrewarmServerRequest();
  }
  if (base::FeatureList::IsEnabled(
          features::kQuickAnswersSelectionSuggestions) &&
      !base::FeatureList::IsEnabled(
          ::selection::kSmartSelectionServerSuggestions)) {
    if (auto* opt_guide = OptimizationGuideKeyedServiceFactory::GetForProfile(
            tab_->GetProfile())) {
      opt_guide->PrewarmModelExecution(
          optimization_guide::ModelBasedCapabilityKey::kQuickAnswers);
    }
  }
  ObserveActiveTabChanges();
  ObserveSidePanelState();
  ObservePageContextEligibility();
  if (GlicSidePanelCoordinator::IsShowing(tab_)) {
    // The panel is already open (e.g. the context menu doesn't check), so it
    // won't open mid-session. Hand the session to it now.
    PostHandSessionToPanel();
  }
  ShowModalUI();
  MaybeAdoptPrewarmedOverlay();
  if (state() == State::kScreenshot &&
      session_->entry_point == ::selection::EntryPoint::kInlineCue) {
    if (auto* host_view = GetHostView()) {
      if (auto* focus_manager = host_view->GetFocusManager();
          focus_manager && !focus_manager->GetFocusedView() &&
          focus_manager->GetStoredFocusView()) {
        // Clear stored focus so dismissing the inline-cue chip does not
        // refocus the page during screenshot capture.
        focus_manager->SetStoredFocusView(nullptr);
        session_->cleared_stored_focus = true;
      }
    }
  }
}

// TODO(b/556786015): After requirement is finalized, reconsider if we should
// pass full interaction_options or just the selected media, and let this
// function decide which options to send to web.
void SelectionOverlayController::ShowWithSelection(
    content::RenderFrameHost* selected_frame,
    const gfx::Rect& selection_bounds,
    selection::InteractionOptionsPtr interaction_options,
    ::selection::EntryPoint entry_point) {
  CloseLensOverlayIfNeeded();
  if (!CanStartSession()) {
    return;
  }
  session_->entry_point = entry_point;
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
    // The bounds are an element selection from the image or page context menu;
    // every other entry point pre-selects text.
    bool is_text_selection =
        session_->entry_point != ::selection::EntryPoint::kContextMenuImage &&
        session_->entry_point != ::selection::EntryPoint::kContextMenuPage;
    if (auto region = CreateRegionFromBounds(
            selection_bounds, tab_->GetContents()->GetViewBounds(),
            /*apply_padding=*/is_text_selection)) {
      base::UnguessableToken id = region->id;
      session_->active_region_id = id;
      auto [it, _] = session_->selected_regions.emplace(
          id, SelectedRegionData(std::move(region)));
      it->second.is_text_selection = is_text_selection;
      if (is_text_selection && selected_frame &&
          selected_frame->IsRenderFrameLive()) {
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
  prewarmer_.Cancel();
  if (state() == State::kOff) {
    return;
  }
  overlay_page_bound_ = false;
  auto* focus_manager =
      GetHostView() ? GetHostView()->GetFocusManager() : nullptr;
  const bool had_focus =
      (overlay_web_view_ && overlay_web_view_->HasFocus()) ||
      (preselection_widget_ && preselection_widget_->IsActive()) ||
      (session_->cleared_stored_focus && state() == State::kScreenshot &&
       tab_->IsActivated() && tab_->GetBrowserWindowInterface()->IsActive() &&
       focus_manager && !focus_manager->GetFocusedView());
  CloseReason close_reason =
      session_->close_reason.value_or(CloseReason::kOther);
  Reset();
  OverlayBaseController::CloseUI();
  if (had_focus && tab_->GetContents()) {
    tab_->GetContents()->Focus();
  }
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
  SendPageContextReceivedIfReady();
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
  session_->cleared_stored_focus = false;
}

void SelectionOverlayController::OnScreenshotRedacted(const SkBitmap& bitmap) {
  session_->redacted_screenshot = bitmap;
  SyncRegionsToPanelIfPending();
  SendPageContextReceivedIfReady();
  MaybeRequestSuggestions();
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
  SendPageContextReceivedIfReady();
  MaybeRequestSuggestions();
}

void SelectionOverlayController::SetScreenshot(const SkBitmap& screenshot,
                                               SkBitmap rgb_screenshot) {
  session_->initial_rgb_screenshot = std::move(rgb_screenshot);
  session_->screenshot_available = true;
  InitializeOverlay();
}

bool SelectionOverlayController::IsPageContextEligible() const {
  auto* helper = tabs::PageContextEligibilityHelper::From(tab_);
  return helper &&
         helper->IsPageContextEligible() ==
             optimization_guide::PageContextEligibilityStatus::kEligible;
}

void SelectionOverlayController::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus status) {
  // `kUnknown` comes from tab switches and navigations, which
  // `TabDeactivated()` and `NotifyPageNavigated()` already handle.
  if (status ==
      optimization_guide::PageContextEligibilityStatus::kNotEligible) {
    Close();
  }
}

bool SelectionOverlayController::IsResultsSidePanelShowing() {
  return GlicSidePanelCoordinator::IsShowing(tab_);
}

GURL SelectionOverlayController::GetInitialURL() {
  return GURL(chrome::kChromeUIGlicSelectionOverlayURL);
}

void SelectionOverlayController::UpdateToolbarButtonHighlight(bool is_showing) {
  // The tab has no window while it is being detached.
  BrowserWindowInterface* const bwi = tab_->GetBrowserWindowInterface();
  if (!bwi) {
    return;
  }
  // The overlay can open (e.g. via hotkey) while the toolbar action is not
  // registered, and the pinned container CHECKs that the action exists.
  BrowserActions* const browser_actions = BrowserActions::From(bwi);
  if (!browser_actions || !actions::ActionManager::Get().FindAction(
                              kActionGlicSmartSelectionToolbar,
                              browser_actions->root_action_item())) {
    return;
  }
  BrowserWindow* const browser_window = BrowserWindow::FromBrowser(bwi);
  if (PinnedToolbarActions* const pinned_actions =
          browser_window ? browser_window->GetPinnedToolbarActions()
                         : nullptr) {
    pinned_actions->UpdateActionState(kActionGlicSmartSelectionToolbar,
                                      is_showing);
  }
}

void SelectionOverlayController::NotifyIsOverlayShowing(bool is_showing) {
  // Only the active tab owns the window-level toolbar button state; an
  // inactive split-view pane must not overwrite it.
  if (!tab_->IsActivated()) {
    return;
  }
  UpdateToolbarButtonHighlight(is_showing);
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

void SelectionOverlayController::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  CancelPendingEndpointRequests();
  OverlayBaseController::PrimaryMainFrameRenderProcessGone(status);
}

void SelectionOverlayController::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Cancel pending async endpoint submissions even when the overlay is `kOff`
  // (`InvokeWithPrompt()` calls `Close()` before `endpoint->Submit()`, and
  // `OverlayBaseController::DidFinishNavigation()` returns early when `kOff`).
  // Only cancel on cross-document navigations (which also covers reloads);
  // same-document URL changes (`pushState` / fragment updates) keep the
  // document alive and should not tear down an in-flight submission.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    CancelPendingEndpointRequests();
  }
  OverlayBaseController::DidFinishNavigation(navigation_handle);
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
      Close(CloseReason::kBackgroundClick);
      break;
  }
}

void SelectionOverlayController::AdjustRegion(
    selection::SelectedRegionPtr target,
    bool is_using_keyboard) {
  session_->active_region_id = target->id;
  session_->feedback_zip_builder = ::selection::FeedbackZipBuilder();
  if (SelectedRegionData* region_data =
          base::FindOrNull(session_->selected_regions, target->id)) {
    region_data->region = std::move(target);
    region_data->is_text_selection = false;
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

  if (GlicSidePanelCoordinator::IsShowing(tab_)) {
    // Retry in case the panel hasn't taken the session yet.
    PostHandSessionToPanel();
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

void SelectionOverlayController::InvokeFromSuggestion(
    std::string prompt,
    ::selection::mojom::PromptEndpointId endpoint_id) {
  InvokeWithPrompt(std::move(prompt), endpoint_id);
}

void SelectionOverlayController::SetActivePromptEndpoint(
    ::selection::mojom::PromptEndpointId endpoint_id) {
  // Endpoint availability can change asynchronously on the browser side while a
  // `SetActivePromptEndpoint()` call from the WebUI is in flight (e.g., if the
  // Gemini side panel adopts the session via `AdoptSession()`, the default
  // search engine changes, or Glic enabling changes). Ignore unavailable
  // endpoints instead of treating the race as a bad message.
  if (!FindAvailablePromptEndpoint(endpoint_id)) {
    return;
  }
  if (session_->active_endpoint_id == endpoint_id) {
    return;
  }
  session_->active_endpoint_id = endpoint_id;
  for (auto& [id, region_data] : session_->selected_regions) {
    if (region_data.suggestions_requested) {
      // Active prompt endpoint changed: invalidate cached or in-flight
      // suggestions so stale results are discarded and the next
      // `GetSuggestedActions()` issues a fresh query for the new endpoint.
      region_data.suggestions.clear();
      region_data.suggestions_requested = false;
      region_data.suggestions_complete = false;
      region_data.generation++;
    }
  }
}

void SelectionOverlayController::SubmitPrompt(const std::string& prompt) {
  if (!base::FeatureList::IsEnabled(features::kGlicSelectionOverlayPromptBox)) {
    return;
  }
  if (!session_->active_endpoint_id.has_value()) {
    return;
  }
  ::selection::PromptEndpoint* endpoint =
      FindAvailablePromptEndpoint(*session_->active_endpoint_id);
  if (!endpoint) {
    return;
  }
  if (!IsPageContextReady()) {
    // The overlay doesn't allow submitting until `PageContextReceived()`.
    session_->receiver.ReportBadMessage(
        "Prompt submitted before the page context.");
    return;
  }
  // Panel-owned sessions are pinned to Gemini, so only persist the last-used
  // endpoint for direct sessions where the user can choose.
  if (session_->owner == Session::Owner::kDirect) {
    pref_service_->SetInteger(prefs::kGlicSelectionOverlayLastEndpoint,
                              static_cast<int>(*session_->active_endpoint_id));
  }
  if (session_->metrics_logger) {
    session_->metrics_logger->RecordPromptSubmitted(endpoint->GetId());
  }
  InvokeWithPrompt(prompt, endpoint->GetId());
}

void SelectionOverlayController::RegisterPromptEndpoint(
    std::unique_ptr<::selection::PromptEndpoint> endpoint) {
  CHECK(endpoint);
  prompt_endpoints_.push_back(std::move(endpoint));
}

void SelectionOverlayController::SetPromptEndpointsForTesting(
    std::vector<std::unique_ptr<::selection::PromptEndpoint>> endpoints) {
  prompt_endpoints_ = std::move(endpoints);
}

bool SelectionOverlayController::ShouldCloseOnSubmit(
    ::selection::mojom::PromptEndpointId endpoint_id) const {
  // Only a Glic-owned session keeps the overlay open when submitting back to
  // Gemini; direct sessions and submissions to another endpoint (which replace
  // the Glic side panel) always close the overlay.
  return session_->owner != Session::Owner::kPanel ||
         endpoint_id != ::selection::mojom::PromptEndpointId::kGemini;
}

bool SelectionOverlayController::IsEndpointAllowedForSession(
    ::selection::mojom::PromptEndpointId endpoint_id) const {
  // Panel-owned sessions (initiated from or adopted by the open Gemini side
  // panel) stream regions directly to Gemini and stay pinned to `kGemini`.
  return session_->owner != Session::Owner::kPanel ||
         endpoint_id == ::selection::mojom::PromptEndpointId::kGemini;
}

::selection::PromptEndpoint*
SelectionOverlayController::FindAvailablePromptEndpoint(
    ::selection::mojom::PromptEndpointId endpoint_id) {
  if (!IsEndpointAllowedForSession(endpoint_id)) {
    return nullptr;
  }
  for (const auto& endpoint : prompt_endpoints_) {
    if (endpoint->GetId() == endpoint_id && endpoint->IsAvailable(*tab_)) {
      return endpoint.get();
    }
  }
  return nullptr;
}

std::optional<::selection::mojom::PromptEndpointId>
SelectionOverlayController::ResolveInitialPromptEndpointId() {
  const auto last_used = static_cast<::selection::mojom::PromptEndpointId>(
      pref_service_->GetInteger(prefs::kGlicSelectionOverlayLastEndpoint));
  if (::selection::mojom::IsKnownEnumValue(last_used) &&
      FindAvailablePromptEndpoint(last_used)) {
    return last_used;
  }
  if (FindAvailablePromptEndpoint(
          ::selection::mojom::PromptEndpointId::kGemini)) {
    return ::selection::mojom::PromptEndpointId::kGemini;
  }
  for (const auto& endpoint : prompt_endpoints_) {
    if (IsEndpointAllowedForSession(endpoint->GetId()) &&
        endpoint->IsAvailable(*tab_)) {
      return endpoint->GetId();
    }
  }
  return std::nullopt;
}

std::vector<selection::PromptEndpointInfoPtr>
SelectionOverlayController::GetAvailablePromptEndpointInfos() const {
  std::vector<selection::PromptEndpointInfoPtr> infos;
  for (const auto& endpoint : prompt_endpoints_) {
    if (IsEndpointAllowedForSession(endpoint->GetId()) &&
        endpoint->IsAvailable(*tab_)) {
      infos.push_back(selection::PromptEndpointInfo::New(
          endpoint->GetId(), base::UTF16ToUTF8(endpoint->GetDisplayName()),
          base::UTF16ToUTF8(endpoint->GetPlaceholder())));
    }
  }
  return infos;
}

::selection::PageContext SelectionOverlayController::BuildPageContext(
    bool include_submission_data) const {
  ::selection::PageContext page_context;
  page_context.screenshot = session_->redacted_screenshot;
  if (!session_->tab_context) {
    return page_context;
  }
  if (const auto& annotated_page_data =
          session_->tab_context->annotated_page_data) {
    if (annotated_page_data->annotated_page_content.has_value()) {
      if (auto apc =
              annotated_page_data->annotated_page_content
                  ->As<optimization_guide::proto::AnnotatedPageContent>()) {
        page_context.apc = std::move(*apc);
      }
    }
    page_context.metadata = annotated_page_data->metadata.Clone();
  }
  if (!include_submission_data) {
    return page_context;
  }
  if (const auto& viewport_screenshot =
          session_->tab_context->viewport_screenshot;
      viewport_screenshot && !viewport_screenshot->data.empty() &&
      viewport_screenshot->width_pixels > 0 &&
      viewport_screenshot->height_pixels > 0) {
    ::selection::PageContext::EncodedScreenshot encoded;
    encoded.data = viewport_screenshot->data;
    encoded.mime_type = viewport_screenshot->mime_type;
    encoded.size = gfx::Size(viewport_screenshot->width_pixels,
                             viewport_screenshot->height_pixels);
    page_context.encoded_screenshot = std::move(encoded);
  }
  if (session_->tab_context->pdf_document_data) {
    ::selection::PageContext::PdfDocumentData pdf;
    pdf.origin = session_->tab_context->pdf_document_data->origin;
    pdf.bytes = session_->tab_context->pdf_document_data->pdf_data;
    pdf.size_limit_exceeded =
        session_->tab_context->pdf_document_data->pdf_size_limit_exceeded;
    page_context.pdf_data = std::move(pdf);
  }
  return page_context;
}

::selection::AreaOfInterest SelectionOverlayController::BuildAreaOfInterest(
    const SelectedRegionData* region_data,
    bool include_region_bounds) const {
  ::selection::AreaOfInterest aoi;
  if (!region_data) {
    return aoi;
  }
  aoi.selected_text = region_data->selected_text;
  aoi.text_surrounding_selection = region_data->text_surrounding_selection;
  aoi.is_text_selection = region_data->is_text_selection;
  if (!include_region_bounds || session_->redacted_screenshot.empty()) {
    return aoi;
  }
  const auto& shape = region_data->region->shape;
  if (shape->is_rect()) {
    if (std::optional<gfx::Rect> rect = ConvertRectToPixels(
            shape->get_rect(), session_->redacted_screenshot)) {
      aoi.bounds = *rect;
    }
  } else if (shape->is_polyline()) {
    if (std::optional<std::vector<gfx::Point>> line_points =
            ConvertPolylineToPixels(shape->get_polyline(),
                                    session_->redacted_screenshot)) {
      aoi.bounds = std::move(*line_points);
    }
  }
  return aoi;
}

void SelectionOverlayController::CancelPendingEndpointRequests() {
  for (const auto& endpoint : prompt_endpoints_) {
    endpoint->CancelPendingRequests();
  }
}

bool SelectionOverlayController::HasOnlyTextSelection() const {
  return !session_->selected_regions.empty() &&
         std::ranges::all_of(session_->selected_regions, [](const auto& entry) {
           return entry.second.is_text_selection;
         });
}

bool SelectionOverlayController::ShouldSendRegionsWithPrompt() const {
  // Panel-owned sessions send their regions through region capture. This
  // includes sessions the panel started and sessions handed off to it.
  // TODO(liuwilliam): There are still two code paths to deliver attachments.
  // Reconsider once the feature is stable.
  // A text selection goes without its region: `GlicSelectionObserver` sends
  // the text when the panel opens, and the region would be a second chip.
  // TODO(b:579078690): Remove once text and region selections take one path.
  return session_->owner == Session::Owner::kDirect &&
         !HasOnlyTextSelection() &&
         base::FeatureList::IsEnabled(
             kGlicSelectionOverlaySendRegionWithPrompt);
}

bool SelectionOverlayController::IsPageContextReady() const {
  return session_->tab_context && !session_->redacted_screenshot.empty();
}

void SelectionOverlayController::SendPageContextReceivedIfReady() {
  if (session_->page && session_->screenshot_available &&
      IsPageContextReady()) {
    session_->page->PageContextReceived();
  }
}

void SelectionOverlayController::InvokeWithPrompt(
    std::string prompt,
    ::selection::mojom::PromptEndpointId endpoint_id) {
  ::selection::PromptEndpoint* endpoint =
      FindAvailablePromptEndpoint(endpoint_id);
  if (!endpoint) {
    return;
  }
  const SelectedRegionData* active_region = nullptr;
  if (session_->active_region_id.has_value()) {
    active_region = base::FindOrNull(session_->selected_regions,
                                     *session_->active_region_id);
  }
  if (!active_region && !session_->selected_regions.empty()) {
    active_region = &session_->selected_regions.begin()->second;
  }
  if (endpoint_id == ::selection::mojom::PromptEndpointId::kGemini &&
      active_region && active_region->is_text_selection) {
    for (const auto& [id, data] : session_->selected_regions) {
      if (!data.is_text_selection) {
        active_region = &data;
        break;
      }
    }
  }
  // Overlay-specific policy: Glic receives regions directly when the panel
  // owns the session, or omits them when
  // `kGlicSelectionOverlaySendRegionWithPrompt` is disabled or only text is
  // selected (`GlicSelectionObserver` handles text). Other endpoints (e.g.
  // Google Search) always receive the active region bounds so pre-selected
  // text bounding boxes can be cropped visually.
  const bool include_region_bounds =
      IsPageContextReady() &&
      (endpoint_id != ::selection::mojom::PromptEndpointId::kGemini ||
       ShouldSendRegionsWithPrompt());
  // Skip deserializing/copying page context data (APC, metadata, screenshot,
  // PDF bytes) when submitting to Gemini without region bounds, since
  // `GeminiPromptEndpoint::Submit()` discards `PageContext` when there are no
  // captured regions to attach.
  ::selection::PageContext page_context;
  if (endpoint_id != ::selection::mojom::PromptEndpointId::kGemini ||
      include_region_bounds) {
    page_context = BuildPageContext(/*include_submission_data=*/true);
  }
  ::selection::PageContextWithSelection context(
      std::move(page_context),
      BuildAreaOfInterest(active_region, include_region_bounds));
  const bool should_close = ShouldCloseOnSubmit(endpoint->GetId());
  base::WeakPtr<SelectionOverlayController> weak_this =
      weak_factory_.GetWeakPtr();
  if (should_close && state() != State::kOff) {
    // Close the overlay before invoking the endpoint so opening a non-Glic
    // destination side panel (e.g. Google Search) does not trigger
    // `OverlayBaseController::OnSidePanelDidOpen()` as an unexpected close.
    Close();
  }
  if (weak_this) {
    endpoint->Submit(*tab_, std::move(prompt), std::move(context));
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

  if (!::selection::SuggestionService::From(tab_)) {
    callback.Run({});
    return;
  }

  // Requests them now if every input is here, otherwise once the last arrives.
  MaybeRequestSuggestions();
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
  if (session_->active_region_id == region_id) {
    MaybeRequestSuggestions();
  }
}

bool SelectionOverlayController::AreSuggestionInputsReady() const {
  if (session_->redacted_screenshot.empty()) {
    return false;
  }
  // The page can ask for suggestions before the page content arrives.
  const bool needs_page_content =
      base::FeatureList::IsEnabled(
          ::selection::kSmartSelectionServerSuggestions) &&
      ::selection::kSmartSelectionServerAppendApc.Get();
  return !needs_page_content || session_->tab_context;
}

void SelectionOverlayController::MaybeRequestSuggestions() {
  if (!IsSelectionOverlayPromptEnabled() ||
      !session_->active_region_id.has_value() || !AreSuggestionInputsReady() ||
      !(session_->suggested_actions_listener.is_bound() ||
        session_->suggested_actions_callback_for_testing) ||
      !::selection::SuggestionService::From(tab_)) {
    return;
  }
  SelectedRegionData* region_data =
      base::FindOrNull(session_->selected_regions, *session_->active_region_id);
  if (!region_data || region_data->suggestions_requested ||
      region_data->waiting_for_surrounding_text) {
    return;
  }
  RequestNewSuggestions(*region_data);
}

void SelectionOverlayController::RequestNewSuggestions(
    SelectedRegionData& region_data) {
  ::selection::SuggestionService* suggestion_service =
      ::selection::SuggestionService::From(tab_);
  CHECK(suggestion_service);
  CHECK(AreSuggestionInputsReady());
  CHECK(!region_data.waiting_for_surrounding_text);

  region_data.suggestions_requested = true;
  uint64_t generation = ++region_data.generation;
  base::UnguessableToken region_id = region_data.region->id;

  ::selection::PageContext page_context =
      BuildPageContext(/*include_submission_data=*/false);
  ::selection::AreaOfInterest aoi =
      BuildAreaOfInterest(&region_data, /*include_region_bounds=*/true);

  if (base::FeatureList::IsEnabled(kStaticSelectionSuggestions)) {
    std::vector<std::unique_ptr<::selection::Suggestion>> static_suggestions;
    if (!base::FeatureList::IsEnabled(
            features::kGlicSelectionOverlayPromptBox)) {
      static_suggestions.emplace_back(std::make_unique<SelectionSuggestion>(
          CHECK_DEREF(tab_), u"Ask Gemini"));
    }
    if (!base::FeatureList::IsEnabled(
            ::selection::kSmartSelectionServerSuggestions)) {
      if (auto* quick_answers_tool = QuickAnswersTool::From(tab_)) {
        if (auto suggestion = quick_answers_tool->CreateSuggestion(aoi)) {
          static_suggestions.push_back(std::move(suggestion));
        }
      }
    }
    if (!static_suggestions.empty()) {
      OnSuggestionsReceived(region_id, generation, /*complete=*/false,
                            std::move(static_suggestions),
                            ::selection::FeedbackZipBuilder());
    }
  }

  suggestion_service->RequestSuggestions(
      ::selection::PageContextWithSelection(std::move(page_context),
                                            std::move(aoi)),
      session_->active_endpoint_id,
      base::BindOnce(&SelectionOverlayController::OnSuggestionsReceived,
                     session_weak_factory_.GetWeakPtr(), region_id, generation,
                     /*complete=*/true));
}

void SelectionOverlayController::OnSuggestionsReceived(
    const base::UnguessableToken& region_id,
    uint64_t generation,
    bool complete,
    std::vector<std::unique_ptr<::selection::Suggestion>> suggestions,
    ::selection::FeedbackZipBuilder feedback_zip_builder) {
  SelectedRegionData* region_data =
      base::FindOrNull(session_->selected_regions, region_id);
  if (!region_data || region_data->generation != generation) {
    return;
  }

  if (complete) {
    region_data->suggestions_complete = true;
    if (session_->active_region_id == region_id) {
      if (base::FeatureList::IsEnabled(
              features::kGlicSelectionOverlayDogfoodFeedback)) {
        session_->feedback_zip_builder = std::move(feedback_zip_builder);
      }
      if (session_->metrics_logger &&
          (session_->suggested_actions_listener.is_bound() ||
           session_->suggested_actions_callback_for_testing)) {
        session_->metrics_logger->RecordSuggestionsShown(suggestions);
      }
    }
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

  const auto tag = matched_suggestion->GetAction()->which();
  switch (tag) {
    case ::selection::mojom::Action::Tag::kHandoff:
      // kHandoff should not have a channel since the surface is going away.
      if (channel) {
        session_->receiver.ReportBadMessage(
            "Channel supplied for a handoff action.");
        return;
      }
      break;
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
      break;
  }

  if (session_->metrics_logger) {
    session_->metrics_logger->RecordSuggestionAccepted(*matched_suggestion);
  }
  // Panel-owned sessions are pinned to Gemini, so only persist the last-used
  // endpoint for direct sessions where the user can choose.
  if (session_->owner == Session::Owner::kDirect &&
      session_->active_endpoint_id.has_value()) {
    pref_service_->SetInteger(prefs::kGlicSelectionOverlayLastEndpoint,
                              static_cast<int>(*session_->active_endpoint_id));
  }

  switch (tag) {
    case ::selection::mojom::Action::Tag::kHandoff: {
      base::WeakPtr<SelectionOverlayController> weak_this =
          weak_factory_.GetWeakPtr();
      matched_suggestion->Execute(mojo::GenericPendingAssociatedReceiver());
      if (weak_this && state() != State::kOff &&
          session_->owner == Session::Owner::kDirect) {
        Close();
      }
      return;
    }
    case ::selection::mojom::Action::Tag::kInlineFulfillment:
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
  base::FilePath target_zip_path =
      DownloadPrefs::FromBrowserContext(tab_->GetProfile())
          ->DownloadPath()
          .AppendASCII("smart_selection_feedback.zip");
  session_->feedback_zip_builder.CreateZip(
      std::move(target_zip_path),
      base::BindOnce(
          [](base::WeakPtr<SelectionOverlayController> self, const GURL& url,
             std::optional<base::FilePath> zip_path) {
            if (!self || !zip_path.has_value()) {
              return;
            }
            CHECK_DEREF(self->tab_->GetBrowserWindowInterface())
                .OpenGURL(url,
                          ui::mojom::WindowOpenDisposition::kNewForegroundTab);
          },
          weak_factory_.GetWeakPtr(), url));
  Close();
}

void SelectionOverlayController::NotifyOverlayRendered() {
  if (session_->show_requested_time.is_null()) {
    return;
  }
  CHECK(session_->entry_point == ::selection::EntryPoint::kInlineCue);
  // Recorded once per session; later calls from the renderer are ignored.
  // Justification: Measures local overlay startup before any Glic provider is
  // involved, and stays with the other SmartSelection.* metrics.
  base::UmaHistogramTimes(
      base::StrCat({"SmartSelection.InlineCue.ClickToOverlayRenderedTime.",
                    session_->prewarm_ready ? "Ready" : "NotReady"}),
      base::TimeTicks::Now() - session_->show_requested_time);
  session_->show_requested_time = base::TimeTicks();
}

void SelectionOverlayController::Reset() {
  // Drop the callbacks still pending for the old session (e.g. the page context
  // fetch), so they can't land in the next one.
  session_weak_factory_.InvalidateWeakPtrs();
  if (session_->metrics_logger) {
    const bool cancelled_by_user =
        session_->close_reason == CloseReason::kCloseButton ||
        session_->close_reason == CloseReason::kEscapeKeyPress ||
        session_->close_reason == CloseReason::kBackgroundClick ||
        session_->close_reason == CloseReason::kToolbarButton;
    session_->metrics_logger->RecordSessionEnd(cancelled_by_user);
  }
  session_ = std::make_unique<Session>(this);
}

CapturedRegions SelectionOverlayController::GetCapturedRegions() const {
  CHECK(!session_->redacted_screenshot.empty());
  CapturedRegions captured_regions;
  // TODO(http://b/452032491): Reconsider what happens if the regions overlap.
  // TODO(http://b/452032491): Currently this class is only used once per
  // selection and only one region is supported, so it is fine to always loop
  // through all the regions. Revisit once we expand the selections.
  for (const auto& [id, region_data] : session_->selected_regions) {
    // `GlicSelectionObserver` already sends the text selection to the panel,
    // so don't also send its bounding box as a captured region.
    // TODO(b:579078690): Clean this up once text and region selections use
    // one path.
    if (region_data.is_text_selection) {
      continue;
    }
    const auto& region = region_data.region;
    if (region->shape->is_rect()) {
      if (std::optional<gfx::Rect> rect = ConvertRectToPixels(
              region->shape->get_rect(), session_->redacted_screenshot)) {
        captured_regions.emplace_back(
            id, glic::mojom::CapturedRegion::NewRect(*rect));
      } else {
        // TODO(b/485358530): Record proper histograms for the error case.
        LOG(ERROR) << "Invalid region selected "
                   << region->shape->get_rect().ToString();
      }
    } else if (region->shape->is_polyline()) {
      if (base::FeatureList::IsEnabled(features::kGlicRegionSelectionLine)) {
        if (std::optional<std::vector<gfx::Point>> line_points =
                ConvertPolylineToPixels(region->shape->get_polyline(),
                                        session_->redacted_screenshot)) {
          captured_regions.emplace_back(
              id, glic::mojom::CapturedRegion::NewPolyline(
                      std::move(*line_points)));
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
  if (!captured_regions.empty() && session_->metrics_logger) {
    session_->metrics_logger->RecordSpatialSelectionCompleted();
  }

  if (session_->page) {
    std::vector<selection::SelectedRegionPtr> regions;
    regions.reserve(session_->selected_regions.size());
    for (const auto& [id, region_data] : session_->selected_regions) {
      if (region_data.is_text_selection ||
          std::ranges::contains(captured_regions, id,
                                &CapturedRegions::value_type::first)) {
        regions.push_back(region_data.region.Clone());
      }
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

void SelectionOverlayController::SyncRegionsToPanel(bool should_focus_panel) {
  if (!session_->tab_context || session_->redacted_screenshot.empty()) {
    session_->panel_sync_pending = true;
    return;
  }
  SendRegionsToPanel(GetCapturedRegions(), should_focus_panel);
}

void SelectionOverlayController::SyncRegionsToPanelIfPending() {
  if (session_->panel_sync_pending) {
    // Keep focus on the overlay so that the user can adjust the region.
    SyncRegionsToPanel(/*should_focus_panel=*/false);
  }
}

void SelectionOverlayController::SendRegionsToPanel(
    CapturedRegions captured_regions,
    bool should_focus_panel) {
  if (session_->owner != Session::Owner::kPanel) {
    return;
  }
  session_->panel_sync_pending = false;
  // Without a valid region, glic would show the screenshot with no chips.
  // An empty list when only the text selection remains clears the region
  // chips after the user deletes the last drawn region.
  if (captured_regions.empty() && !HasOnlyTextSelection()) {
    return;
  }
  GlicKeyedService* service = GlicKeyedService::Get(tab_->GetProfile());
  GlicInstance* instance = service->GetInstanceForTab(tab_);
  if (!instance) {
    return;
  }
  instance->SendAdditionalContext(CreateRegionSelectionContext(
      *tab_, session_->tab_context.Clone(), std::move(captured_regions)));
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

}  // namespace glic
