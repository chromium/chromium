// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_tab_underline_view.h"

#include <math.h>

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/browser_ui/theme_util.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/context_factory.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/color_parser.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

// The amount of time for the opacity to go from 0 to 1.
constexpr static base::TimeDelta kOpacityRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the opacity to go from 0 to 1 in a fast ramp up.
constexpr static base::TimeDelta kFastOpacityRampUpDuration =
    base::Milliseconds(200);
// The amount of time for the opacity to go from 1 to 0.
constexpr static base::TimeDelta kOpacityRampDownDuration =
    base::Milliseconds(200);
// The amount of time for the underline emphasis to go from 0 the max.
constexpr static base::TimeDelta kEmphasisRampUpDuration =
    base::Milliseconds(500);
// The amount of time for the underline emphasis to go from max to 0.
constexpr static base::TimeDelta kEmphasisRampDownDuration =
    base::Milliseconds(1000);
// The amount of time for the underline to stay emphasized.
constexpr static base::TimeDelta kEmphasisDuration = base::Milliseconds(1500);
// Time since creation will roll over after this time to prevent growing
// indefinitely.
constexpr static base::TimeDelta kMaxTime = base::Hours(1);

float ClampAndInterpolate(gfx::Tween::Type type,
                          float t,
                          float low,
                          float high) {
  float clamp_lo = std::min(low, high);
  float clamp_hi = std::max(low, high);
  float clamped = std::clamp(t, clamp_lo, clamp_hi);
  // Interpolate `clamped` within [low, high], using the function `type`.
  double calculated = gfx::Tween::CalculateValue(type, clamped);
  // Linear project `calculated` onto [low, high].
  return gfx::Tween::FloatValueBetween(calculated, low, high);
}

int64_t TimeTicksToMicroseconds(base::TimeTicks tick) {
  return (tick - base::TimeTicks()).InMicroseconds();
}

std::vector<SkColor> GetParameterizedColors() {
  std::vector<SkColor> colors;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    std::vector<std::string> unparsed_colors =
        base::SplitString(::features::kGlicParameterizedShaderColors.Get(), "#",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& unparsed : unparsed_colors) {
      SkColor result;
      if (!content::ParseHexColorString("#" + unparsed, &result)) {
        return std::vector<SkColor>();
      }
      colors.push_back(result);
    }
  }
  return colors;
}

std::vector<float> GetParameterizedFloats() {
  std::vector<float> floats;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    std::vector<std::string> unparsed_floats =
        base::SplitString(::features::kGlicParameterizedShaderFloats.Get(), "#",
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& unparsed : unparsed_floats) {
      double result;
      if (!base::StringToDouble(unparsed, &result)) {
        return std::vector<float>();
      }
      floats.push_back(static_cast<float>(result));
    }
  }
  return floats;
}

}  // namespace

GlicTabUnderlineView::Factory* GlicTabUnderlineView::Factory::factory_ =
    nullptr;

std::unique_ptr<GlicTabUnderlineView> GlicTabUnderlineView::Factory::Create(
    Browser* browser,
    Tab* tab) {
  if (factory_) [[unlikely]] {
    return factory_->CreateUnderlineView(browser, tab);
  }
  return base::WrapUnique(
      new GlicTabUnderlineView(browser, tab, /*tester=*/nullptr));
}

// The following logic makes many references to "pinned" tabs. All of these
// refer to tabs that are selected to be shared with Gemini under the glic
// multitab feature. This is different from the older existing notion of
// "pinned" tabs in the tabstrip, which is the UI treatment that fixes a Tab
// view to one side with a reduced visual. Separate terminology should be used
// for the glic multitab concept in order to disambiguate, but landed code
// already adopts the "pinning" term and so that continues to be used here.
// TODO(crbug.com/433131600): update glic multitab sharing code to use less
// conflicting terminology.
class GlicTabUnderlineView::UnderlineViewUpdater
    : public GlicWindowController::StateObserver {
 public:
  UnderlineViewUpdater(Browser* browser, GlicTabUnderlineView* underline_view)
      : underline_view_(underline_view), browser_(browser) {
    auto* glic_service = GetGlicKeyedService();
    GlicSharingManager& sharing_manager = glic_service->sharing_manager();

    // Subscribe to changes in the focused tab.
    focus_change_subscription_ =
        sharing_manager.AddFocusedTabChangedCallback(base::BindRepeating(
            &GlicTabUnderlineView::UnderlineViewUpdater::OnFocusedTabChanged,
            base::Unretained(this)));

    // Subscribe to changes in the context access indicator status.
    indicator_change_subscription_ =
        glic_service->AddContextAccessIndicatorStatusChangedCallback(
            base::BindRepeating(&GlicTabUnderlineView::UnderlineViewUpdater::
                                    OnIndicatorStatusChanged,
                                base::Unretained(this)));

    // Subscribe to changes in the set of pinned tabs.
    pinned_tabs_change_subscription_ =
        sharing_manager.AddPinnedTabsChangedCallback(base::BindRepeating(
            &GlicTabUnderlineView::UnderlineViewUpdater::OnPinnedTabsChanged,
            base::Unretained(this)));

    // Observe changes in the floaty state.
    glic_service->window_controller().AddStateObserver(this);

    // Subscribe to when new requests are made by glic.
    user_input_submitted_subscription_ =
        glic_service->AddUserInputSubmittedCallback(base::BindRepeating(
            &GlicTabUnderlineView::UnderlineViewUpdater::OnUserInputSubmitted,
            base::Unretained(this)));
  }
  UnderlineViewUpdater(const UnderlineViewUpdater&) = delete;
  UnderlineViewUpdater& operator=(const UnderlineViewUpdater&) = delete;
  ~UnderlineViewUpdater() override {
    GetGlicKeyedService()->window_controller().RemoveStateObserver(this);
  }

  // Called when the focused tab changes with the focused tab data object.
  // This code interprets the tab data to determine how underline_view_'s tab
  // was involved.
  void OnFocusedTabChanged(const FocusedTabData& focused_tab_data) {
    tabs::TabInterface* tab = focused_tab_data.focus();
    auto* previous_focus = glic_current_focused_contents_.get();

    if (tab) {
      glic_current_focused_contents_ = tab->GetContents()->GetWeakPtr();
    } else {
      glic_current_focused_contents_.reset();
    }
    auto* current_focus = glic_current_focused_contents_.get();

    base::WeakPtr<content::WebContents> underline_contents;
    if (auto tab_interface = GetTabInterface()) {
      underline_contents = tab_interface->GetContents()->GetWeakPtr();
    } else {
      return;
    }

    bool focus_changed = previous_focus != current_focus;
    bool tab_switch =
        previous_focus && glic_current_focused_contents_ && focus_changed;
    bool this_tab_gained_focus =
        (underline_contents.get() == current_focus) && focus_changed;
    bool this_tab_lost_focus =
        (underline_contents.get() == previous_focus) && focus_changed;

    bool window_gained_focus =
        !previous_focus && glic_current_focused_contents_;
    bool window_lost_focus = previous_focus && !glic_current_focused_contents_;

    if (tab_switch) {
      if (this_tab_gained_focus) {
        UpdateUnderlineView(
            UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus);
      } else if (this_tab_lost_focus) {
        UpdateUnderlineView(
            UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus);
      } else {
        UpdateUnderlineView(
            UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange);
      }
    } else {
      if (window_gained_focus) {
        UpdateUnderlineView(
            UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus);
      } else if (window_lost_focus) {
        UpdateUnderlineView(
            UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus);
      }
    }
  }

  // Called when the client changes the context access indicator status. This
  // happens when the sharing control in the floaty is toggled, and implicitly
  // when floaty is [back/fore]grounded while sharing is on.
  void OnIndicatorStatusChanged(bool enabled) {
    if (context_access_indicator_enabled_ == enabled) {
      return;
    }
    context_access_indicator_enabled_ = enabled;
    UpdateUnderlineView(
        context_access_indicator_enabled_
            ? UpdateUnderlineReason::kContextAccessIndicatorOn
            : UpdateUnderlineReason::kContextAccessIndicatorOff);
  }

  // Called when the glic set of pinned tabs changes.
  void OnPinnedTabsChanged(
      const std::vector<content::WebContents*>& pinned_contents) {
    if (!GetTabInterface()) {
      // If the TabInterface is invalid at this point, there is no relevant UI
      // to handle.
      return;
    }

    // Triggering is handled based on whether the tab is in the pinned set.
    if (IsUnderlineTabPinned()) {
      UpdateUnderlineView(
          UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet);
      return;
    }
    UpdateUnderlineView(
        UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet);
  }

  // The glic panel state must be separately observed because underlines of
  // pinned tabs uniquely respond to showing/hiding of the glic panel.
  void PanelStateChanged(const glic::mojom::PanelState& panel_state,
                         Browser*) override {
    UpdateUnderlineView(
        panel_state.kind == mojom::PanelState::Kind::kHidden
            ? UpdateUnderlineReason::kPanelStateChanged_PanelHidden
            : UpdateUnderlineReason::kPanelStateChanged_PanelShowing);
  }

  void OnUserInputSubmitted() {
    UpdateUnderlineView(UpdateUnderlineReason::kUserInputSubmitted);
  }

 private:
  // Types of updates to the tab underline UI effect given changes in relevant
  // triggering signals, including tab focus, glic sharing controls, pinned tabs
  // and the floaty panel.
  enum class UpdateUnderlineReason {
    kContextAccessIndicatorOn = 0,
    kContextAccessIndicatorOff,

    // Tab focus change not involving this underline.
    kFocusedTabChanged_NoFocusChange,
    // This underline's tab gained focus.
    kFocusedTabChanged_TabGainedFocus,
    // This underline's tab lost focus.
    kFocusedTabChanged_TabLostFocus,

    kFocusedTabChanged_ChromeGainedFocus,
    kFocusedTabChanged_ChromeLostFocus,

    // Chanes were made to the set of pinned of tabs.
    kPinnedTabsChanged_TabInPinnedSet,
    kPinnedTabsChanged_TabNotInPinnedSet,

    // Events related to the glic panel's state.
    kPanelStateChanged_PanelShowing,
    kPanelStateChanged_PanelHidden,

    kUserInputSubmitted,
  };

  GlicKeyedService* GetGlicKeyedService() {
    return GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
  }

  // Returns the TabInterface corresponding to `underline_view_`, if it is
  // valid.
  base::WeakPtr<tabs::TabInterface> GetTabInterface() {
    if (underline_view_ && underline_view_->tab_) {
      if (auto tab_interface = underline_view_->tab_->data().tab_interface) {
        return tab_interface;
      }
    }
    return nullptr;
  }

  bool IsUnderlineTabPinned() {
    if (auto tab_interface = GetTabInterface()) {
      if (auto* glic_service = GetGlicKeyedService()) {
        return glic_service->sharing_manager().IsTabPinned(
            tab_interface->GetHandle());
      }
    }
    return false;
  }

  bool IsUnderlineTabSharedThroughActiveFollow() {
    if (auto tab_interface = GetTabInterface()) {
      if (auto* glic_service = GetGlicKeyedService()) {
        return (glic_service->sharing_manager().GetFocusedTabData().focus() ==
                tab_interface.get()) &&
               context_access_indicator_enabled_;
      }
    }
    return false;
  }

  // Trigger the necessary UI effect, primarily based on the given
  // `UpdateUnderlineReason` and whether or not `underline_view_`'s tab is
  // being shared via pinning or active following.
  void UpdateUnderlineView(UpdateUnderlineReason reason) {
    AddReasonForDebugging(reason);
    auto reasons_string = UpdateReasonsToString();
    SCOPED_CRASH_KEY_STRING1024("crbug-398319435", "update_reasons",
                                reasons_string);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "access_indicator",
                          context_access_indicator_enabled_);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "glic_focused_contents",
                          !!glic_current_focused_contents_);
    SCOPED_CRASH_KEY_BOOL("crbug-398319435", "is_glic_window_showing",
                          IsGlicWindowShowing());

    switch (reason) {
      case UpdateUnderlineReason::kContextAccessIndicatorOn: {
        // Active follow tab underline should be newly shown, pinned tabs should
        // re-animate or be newly shown if not already visible.
        if (IsUnderlineTabSharedThroughActiveFollow()) {
          ShowAndAnimateUnderline();
        }
        ShowOrAnimatePinnedUnderline();
        break;
      }
      case UpdateUnderlineReason::kContextAccessIndicatorOff: {
        // Underline should be hidden, with exception to pinned tabs while the
        // glic panel remains open.
        if (IsUnderlineTabPinned() && IsGlicWindowShowing()) {
          break;
        }
        HideUnderline();
        break;
      }
      case UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange: {
        // Pinned tab underlines should re-animate if active follow sharing is
        // on.
        if (context_access_indicator_enabled_ && IsUnderlineTabPinned()) {
          AnimateUnderline();
        }
        break;
      }
      case UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus: {
        // Underline visibility corresponds to the focused tab during active
        // follow. Pinned tabs should not react as the set of shared tabs has
        // not changed.
        if (IsUnderlineTabSharedThroughActiveFollow()) {
          ShowAndAnimateUnderline();
        }
        break;
      }
      case UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus: {
        // Underline visibility corresponds to the focused tab during active
        // follow. Pinned tabs should re-animate if the set of shared tabs has
        // changed
        if (IsUnderlineTabPinned() && context_access_indicator_enabled_) {
          AnimateUnderline();
        } else if (!IsUnderlineTabPinned()) {
          HideUnderline();
        }
        break;
      }
      case UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus:
        // Active follow tab underline should be newly shown, pinned tabs should
        // re-animate or be newly shown if not already visible.
        if (IsUnderlineTabSharedThroughActiveFollow()) {
          ShowAndAnimateUnderline();
        }
        ShowOrAnimatePinnedUnderline();
        break;
      case UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus:
        // Underline should be hidden, with exception to pinned tabs.
        if (!IsUnderlineTabPinned()) {
          HideUnderline();
        }
        break;
      case UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet:
        // If `underline_view_` is not visible, then this tab was just added to
        // the set of pinned tabs.
        if (!underline_view_->IsShowing()) {
          // Pinned tab underlines should only be visible while the glic panel
          // is open.
          if (IsGlicWindowShowing()) {
            ShowAndAnimateUnderline();
          }
        } else {
          // This tab was already pinned - re-animate to reflect the change in
          // the set of pinned tabs.
          AnimateUnderline();
        }
        break;
      case UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet:
        // Re-animate to reflect the change in the set of pinned tabs.
        if (IsUnderlineTabSharedThroughActiveFollow()) {
          AnimateUnderline();
          return;
        }
        // This tab may have just been removed from the pinned set.
        HideUnderline();
        break;
      case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
        // Visibility of underlines of pinned tabs should follow visibility of
        // the glic panel.
        if (IsUnderlineTabPinned()) {
          ShowAndAnimateUnderline();
        }
        break;
      case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
        // Visibility of underlines of pinned tabs should follow visibility of
        // the glic panel.
        if (IsUnderlineTabPinned()) {
          HideUnderline();
        }
        break;
      case UpdateUnderlineReason::kUserInputSubmitted:
        if (underline_view_->IsShowing()) {
          AnimateUnderline();
        }
        break;
    }
  }

  // Off to On. Throw away everything we have and start the animation from
  // the beginning.
  void ShowAndAnimateUnderline() {
    underline_view_->StopShowing();
    underline_view_->Show();
  }

  void HideUnderline() {
    if (underline_view_->IsShowing()) {
      underline_view_->StartRampingDown();
    }
  }

  // Replay the animation without hiding and re-showing the view.
  void AnimateUnderline() { underline_view_->ResetEmphasisAndReplay(); }

  void ShowOrAnimatePinnedUnderline() {
    if (!IsUnderlineTabPinned()) {
      return;
    }
    if (underline_view_->IsShowing()) {
      AnimateUnderline();
    } else {
      ShowAndAnimateUnderline();
    }
  }

  bool IsGlicWindowShowing() const {
    return underline_view_->GetGlicService()->window_controller().IsShowing();
  }

  bool IsTabInCurrentWindow(const content::WebContents* tab) const {
    auto* model = browser_->GetTabStripModel();
    CHECK(model);
    int index = model->GetIndexOfWebContents(tab);
    return index != TabStripModel::kNoTab;
  }

  std::string UpdateReasonToString(UpdateUnderlineReason reason) {
    switch (reason) {
      case UpdateUnderlineReason::kContextAccessIndicatorOn:
        return "IndicatorOn";
      case UpdateUnderlineReason::kContextAccessIndicatorOff:
        return "IndicatorOff";
      case UpdateUnderlineReason::kFocusedTabChanged_NoFocusChange:
        return "TabFocusChange";
      case UpdateUnderlineReason::kFocusedTabChanged_TabGainedFocus:
        return "TabGainedFocus";
      case UpdateUnderlineReason::kFocusedTabChanged_TabLostFocus:
        return "TabLostFocus";
      case UpdateUnderlineReason::kFocusedTabChanged_ChromeGainedFocus:
        return "ChromeGainedFocus";
      case UpdateUnderlineReason::kFocusedTabChanged_ChromeLostFocus:
        return "ChromeLostFocus";
      case UpdateUnderlineReason::kPinnedTabsChanged_TabInPinnedSet:
        return "TabInPinnedSet";
      case UpdateUnderlineReason::kPinnedTabsChanged_TabNotInPinnedSet:
        return "TabNotInPinnedSet";
      case UpdateUnderlineReason::kPanelStateChanged_PanelShowing:
        return "PanelShowing";
      case UpdateUnderlineReason::kPanelStateChanged_PanelHidden:
        return "PanelHidden";
      case UpdateUnderlineReason::kUserInputSubmitted:
        return "UserInputSubmitted";
    }
  }

  void AddReasonForDebugging(UpdateUnderlineReason reason) {
    underline_update_reasons_.push_back(UpdateReasonToString(reason));
    if (underline_update_reasons_.size() > kNumReasonsToKeep) {
      underline_update_reasons_.pop_front();
    }
  }

  std::string UpdateReasonsToString() const {
    std::ostringstream oss;
    for (const auto& r : underline_update_reasons_) {
      oss << r << ",";
    }
    return oss.str();
  }

  // Back pointer to the owner. Guaranteed to outlive `this`.
  const raw_ptr<GlicTabUnderlineView> underline_view_;

  // Owned by `BrowserView`. Outlives all the children of the `BrowserView`.
  const raw_ptr<BrowserWindowInterface> browser_;

  // Tracked states and their subscriptions.
  base::WeakPtr<content::WebContents> glic_current_focused_contents_;
  base::CallbackListSubscription focus_change_subscription_;
  bool context_access_indicator_enabled_ = false;
  base::CallbackListSubscription indicator_change_subscription_;
  base::CallbackListSubscription pinned_tabs_change_subscription_;
  base::CallbackListSubscription user_input_submitted_subscription_;

  static constexpr size_t kNumReasonsToKeep = 10u;
  std::list<std::string> underline_update_reasons_;
};

GlicTabUnderlineView::GlicTabUnderlineView(Browser* browser,
                                           Tab* tab,
                                           std::unique_ptr<Tester> tester)
    : updater_(std::make_unique<UnderlineViewUpdater>(browser, this)),
      creation_time_(base::TimeTicks::Now()),
      tester_(std::move(tester)),
      colors_(GetParameterizedColors()),
      floats_(GetParameterizedFloats()),
      theme_service_(ThemeServiceFactory::GetForProfile(browser->GetProfile())),
      tab_(tab),
      browser_(browser) {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  has_hardware_acceleration_ =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  // Upon GPU crashing, the hardware acceleration status might change. This
  // will observe GPU changes to keep hardware acceleration status updated.
  gpu_data_manager_observer_.Observe(gpu_data_manager);

  UpdateShader();
  CHECK(!shader_.empty()) << "Shader not initialized.";

  auto* glic_service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser->GetProfile());
  // Post-initialization updates. Don't do the update in the updater's ctor
  // because at that time GlicTabUnderlineView isn't fully initialized, which
  // can lead to undefined behavior.
  //
  // Fetch the latest context access indicator status from service. We can't
  // assume the WebApp always updates the status on the service (thus the new
  // subscribers not getting the latest value).
  updater_->OnIndicatorStatusChanged(
      glic_service->is_context_access_indicator_enabled());
}

GlicTabUnderlineView::~GlicTabUnderlineView() = default;

void GlicTabUnderlineView::OnPaint(gfx::Canvas* canvas) {
  if (!compositor_) {
    return;
  }
  auto bounds = GetLocalBounds();
  const auto u_resolution = GetLocalBounds();
  // Insets aren't relevant to the tab underline effect, but are defined in the
  // uniforms of the GlicBorderView shader.
  gfx::Insets uniform_insets = gfx::Insets();

  float corner_radius = 0.0f;
#if BUILDFLAG(IS_MAC)
  if (!browser_->window()->IsFullscreen()) {
    corner_radius = 12.0f;
  }
#endif
  // TODO(crbug.com/433136181): shader logic is borrowed from GlicBorderView,
  // but emphasis can be fixed to 0 for the underline and related handling can
  // be removed entirely.
  std::vector<cc::PaintShader::FloatUniform> float_uniforms = {
      {.name = SkString("u_time"), .value = GetEffectTime()},
      {.name = SkString("u_emphasis"), .value = emphasis_},
      {.name = SkString("u_corner_radius"), .value = corner_radius},
      {.name = SkString("u_insets"),
       .value = static_cast<float>(uniform_insets.left())},
      {.name = SkString("u_progress"), .value = progress_}};
  std::vector<cc::PaintShader::Float2Uniform> float2_uniforms = {
      // TODO(https://crbug.com/406026829): Ideally `u_resolution` should be a
      // vec4(x, y, w, h) and does not assume the origin is (0, 0). This way we
      // can eliminate `u_insets` and void the shader-internal origin-padding.
      {.name = SkString("u_resolution"),
       .value = SkV2{static_cast<float>(u_resolution.width()),
                     static_cast<float>(u_resolution.height())}}};
  std::vector<cc::PaintShader::IntUniform> int_uniforms = {
      {.name = SkString("u_dark"),
       .value = UseDarkMode(theme_service_) ? 1 : 0}};

  std::vector<cc::PaintShader::Float4Uniform> float4_uniforms;
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader)) {
    for (int i = 0; i < static_cast<int>(colors_.size()); ++i) {
      float4_uniforms.push_back(
          {.name = SkString(absl::StrFormat("u_color%d", i + 1)),
           .value =
               SkV4{static_cast<float>(SkColorGetR(colors_[i]) / 255.0),
                    static_cast<float>(SkColorGetG(colors_[i]) / 255.0),
                    static_cast<float>(SkColorGetB(colors_[i]) / 255.0), 1.f}});
    }
    for (int i = 0; i < static_cast<int>(floats_.size()); ++i) {
      float_uniforms.push_back(
          {.name = SkString(absl::StrFormat("u_float%d", i + 1)),
           .value = floats_[i]});
    }
  }

  views::View::OnPaint(canvas);

  cc::PaintFlags flags;
  auto shader = cc::PaintShader::MakeSkSLCommand(
      shader_, std::move(float_uniforms), std::move(float2_uniforms),
      std::move(float4_uniforms), std::move(int_uniforms),
      cached_paint_shader_);

  flags.setShader(shader);

  if (base::FeatureList::IsEnabled(features::kGlicUseShaderCache)) {
    cached_paint_shader_ = shader;
  }

  constexpr static int kMaxEffectWidth = 2;
  gfx::Point origin =
      bounds.origin() +
      gfx::Vector2d(0, bounds.size().height() - kMaxEffectWidth);
  gfx::Size size(bounds.size().width(), kMaxEffectWidth);
  gfx::Rect effect_bounds(origin, size);
  canvas->DrawRect(gfx::RectF(effect_bounds), flags);
}

void GlicTabUnderlineView::OnAnimationStep(base::TimeTicks timestamp) {
  if (tester_) [[unlikely]] {
    timestamp = tester_->GetTestTimestamp();
  }
  last_animation_step_time_ = timestamp;
  if (first_frame_time_.is_null()) {
    first_frame_time_ = timestamp;
  }
  if (first_emphasis_frame_.is_null()) {
    first_emphasis_frame_ = timestamp;

    // The time gaps when the underline is in steady state cause discontinuous
    // underline states when switching tabs. By keeping track of the total
    // steady time, we can have a continuous effect time. Each steady time
    // interval is added to the total at the very beginning of an upcoming
    // emphasis animation. Note: the opacity ramp up / down is not part of the
    // shader animation.
    if (!last_emphasis_frame_.is_null()) {
      total_steady_time_ += timestamp - last_emphasis_frame_;
      last_emphasis_frame_ = base::TimeTicks{};
    }
  }
  if (record_first_ramp_down_frame_) {
    record_first_ramp_down_frame_ = false;
    first_ramp_down_frame_ = timestamp;
  }

  base::TimeDelta emphasis_since_first_frame =
      timestamp - first_emphasis_frame_;
  emphasis_ = GetEmphasis(emphasis_since_first_frame);
  base::TimeDelta opacity_since_first_frame = timestamp - first_frame_time_;
  opacity_ = GetOpacity(timestamp);
  progress_ = GetEffectProgress(timestamp);

  // TODO(liuwilliam): Ideally this should be done in paint-related methods.
  // Consider moving it to LayerDelegate::OnPaintLayer().
  CHECK(layer());
  layer()->SetOpacity(opacity_);

  // Don't animate if the animations have exhausted and we haven't started
  // ramping down. We shouldn't be an observer for more than 60 seconds
  // (CompositorAnimationObserver::NotifyFailure()).
  bool emphasis_done =
      emphasis_ == 0.f && !emphasis_since_first_frame.is_zero();
  bool opacity_ramp_up_done =
      opacity_ == 1.f && !opacity_since_first_frame.is_zero();
  bool show_steady_state =
      emphasis_done && opacity_ramp_up_done && first_ramp_down_frame_.is_null();

  if (show_steady_state) {
    // If skipping the animation the class does not need to be an animation
    // observer.
    compositor_->RemoveAnimationObserver(this);
    if (last_emphasis_frame_.is_null()) {
      last_emphasis_frame_ = timestamp;
    }
    return;
  }

  bool opacity_ramp_down_done =
      opacity_ == 0.f && !first_ramp_down_frame_.is_null();
  if (opacity_ramp_down_done) {
    StopShowing();
    return;
  }

  SchedulePaint();
}

void GlicTabUnderlineView::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  StopShowing();
}

void GlicTabUnderlineView::OnGpuInfoUpdate() {
  auto* gpu_data_manager = content::GpuDataManager::GetInstance();
  bool has_hardware_acceleration =
      gpu_data_manager->IsGpuRasterizationForUIEnabled();

  if (has_hardware_acceleration_ != has_hardware_acceleration) {
    has_hardware_acceleration_ = has_hardware_acceleration;
    UpdateShader();

    if (IsShowing()) {
      SchedulePaint();
    }
  }
}

bool GlicTabUnderlineView::IsShowing() const {
  // `compositor_` is set when the underline starts to show and unset when the
  // underline stops to show.
  return !!compositor_;
}

float GlicTabUnderlineView::GetEffectTimeForTesting() const {
  return GetEffectTime();
}

void GlicTabUnderlineView::Show() {
  if (compositor_) {
    // The user can click on the glic icon after the window is shown. The
    // animation is already playing at that time.
    return;
  }

  if (!parent()) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetVisible(true);

  skip_emphasis_animation_ =
      gfx::Animation::PrefersReducedMotion() || ForceSimplifiedShader();

  ui::Compositor* compositor = layer()->GetCompositor();
  if (!compositor) {
    base::debug::DumpWithoutCrashing();
    return;
  }

  compositor_ = compositor;
  compositor_animation_observation_.Observe(compositor_.get());
  compositor_observation_.Observe(compositor_.get());

  if (tester_) [[unlikely]] {
    tester_->AnimationStarted();
  }
}

void GlicTabUnderlineView::StopShowing() {
  if (!compositor_) {
    return;
  }

  compositor_observation_.Reset();
  compositor_animation_observation_.Reset();
  compositor_ = nullptr;
  first_frame_time_ = base::TimeTicks{};
  first_emphasis_frame_ = base::TimeTicks{};
  last_emphasis_frame_ = base::TimeTicks{};
  first_ramp_down_frame_ = base::TimeTicks{};
  record_first_ramp_down_frame_ = false;
  total_steady_time_ = base::Milliseconds(0);
  opacity_ = 0.f;
  emphasis_ = 0.f;

  // `DestroyLayer()` schedules another paint to repaint the affected area by
  // the destroyed layer.
  DestroyLayer();
  SetVisible(false);
}

float GlicTabUnderlineView::GetEmphasis(base::TimeDelta delta) const {
  if (skip_emphasis_animation_) {
    return 0.f;
  }
  static constexpr base::TimeDelta kRampUpAndSteady =
      kEmphasisRampUpDuration + kEmphasisDuration;
  if (delta < kRampUpAndSteady) {
    auto target = static_cast<float>(delta / kEmphasisRampUpDuration);
    return ClampAndInterpolate(gfx::Tween::Type::EASE_OUT, target, 0, 1);
  }
  auto target = static_cast<float>((delta - kRampUpAndSteady) /
                                   kEmphasisRampDownDuration);
  return ClampAndInterpolate(gfx::Tween::Type::EASE_IN_OUT_2, target, 1, 0);
}

void GlicTabUnderlineView::ResetEmphasisAndReplay() {
  // TOOD(crbug.com/398319435): Remove once we know why this is called before
  // `Show()`.
  if (!compositor_) {
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "opacity", opacity_);
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "emphasis", emphasis_);
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "creation",
                            TimeTicksToMicroseconds(creation_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_frame",
                            TimeTicksToMicroseconds(first_frame_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_emphasis",
                            TimeTicksToMicroseconds(first_emphasis_frame_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "last_step",
                            TimeTicksToMicroseconds(last_animation_step_time_));
    SCOPED_CRASH_KEY_NUMBER("crbug-398319435", "first_rampdown",
                            TimeTicksToMicroseconds(first_ramp_down_frame_));
    base::debug::DumpWithoutCrashing();

    // Gracefully handling the crash case in crbug.com/398319435 by
    // closing(minimizing) the glic window.
    // TODO(crbug.com/413442838): Add tests to reproduce the dump without crash
    // and validate the solution.
    GetGlicService()->window_controller().Close();
    return;
  }
  CHECK(compositor_->HasObserver(this));
  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }
  first_emphasis_frame_ = base::TimeTicks{};
  SchedulePaint();

  if (tester_) [[unlikely]] {
    tester_->EmphasisRestarted();
  }
}

float GlicTabUnderlineView::GetOpacity(base::TimeTicks timestamp) {
  auto ramp_up_duration = skip_emphasis_animation_ ? kFastOpacityRampUpDuration
                                                   : kOpacityRampUpDuration;
  if (!first_ramp_down_frame_.is_null()) {
    // The ramp up opacity could be any value between 0-1 during the ramp up
    // time. Thus, the ramping down opacity must be deducted from the value of
    // ramp up opacity at the time of `first_ramp_down_frame_`.
    base::TimeDelta delta = first_ramp_down_frame_ - first_frame_time_;
    float ramp_up_opacity =
        std::clamp(static_cast<float>(delta.InMillisecondsF() /
                                      ramp_up_duration.InMillisecondsF()),
                   0.0f, 1.0f);

    base::TimeDelta time_since_first_ramp_down_frame =
        timestamp - first_ramp_down_frame_;
    float ramp_down_opacity =
        static_cast<float>(time_since_first_ramp_down_frame.InMillisecondsF() /
                           kOpacityRampDownDuration.InMillisecondsF());
    ramp_down_opacity_ =
        std::clamp(ramp_up_opacity - ramp_down_opacity, 0.0f, 1.0f);
    return ramp_down_opacity_;
  } else {
    base::TimeDelta time_since_first_frame = timestamp - first_frame_time_;
    return std::clamp(
        static_cast<float>(ramp_down_opacity_ +
                           (time_since_first_frame.InMillisecondsF() /
                            ramp_up_duration.InMillisecondsF())),
        0.0f, 1.0f);
  }
}

void GlicTabUnderlineView::StartRampingDown() {
  CHECK(compositor_);

  // From now on the opacity will be decreased until it reaches 0.
  record_first_ramp_down_frame_ = true;

  if (!compositor_->HasAnimationObserver(this)) {
    compositor_->AddAnimationObserver(this);
  }

  if (tester_) [[unlikely]] {
    tester_->RampDownStarted();
  }
}

float GlicTabUnderlineView::GetEffectTime() const {
  if (last_animation_step_time_.is_null()) {
    return 0;
  }

  // Returns a constant duration so the underline states don't jump around when
  // switching tabs.
  if (skip_emphasis_animation_) {
    auto time_since_creation =
        (first_frame_time_ - GetCreationTime()) % kMaxTime;
    return time_since_creation.InSecondsF();
  }

  auto time_since_creation =
      ((last_animation_step_time_ - GetCreationTime()) - total_steady_time_) %
      kMaxTime;
  return time_since_creation.InSecondsF();
}

float GlicTabUnderlineView::GetEffectProgress(base::TimeTicks timestamp) const {
  if (skip_emphasis_animation_) {
    return 0.0;
  }
  base::TimeDelta time_since_first_frame = timestamp - first_emphasis_frame_;
  base::TimeDelta total_duration =
      kEmphasisRampUpDuration + kEmphasisRampDownDuration + kEmphasisDuration;
  return std::clamp(
      static_cast<float>(time_since_first_frame.InMillisecondsF() /
                         total_duration.InMillisecondsF()),
      0.0f, 1.0f);
}

base::TimeTicks GlicTabUnderlineView::GetCreationTime() const {
  if (tester_ && !tester_->GetTestCreationTime().is_null()) [[unlikely]] {
    return tester_->GetTestCreationTime();
  }
  return creation_time_;
}

bool GlicTabUnderlineView::ForceSimplifiedShader() const {
  return base::FeatureList::IsEnabled(features::kGlicForceSimplifiedBorder) ||
         !has_hardware_acceleration_;
}

GlicKeyedService* GlicTabUnderlineView::GetGlicService() const {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile());
  CHECK(service);
  return service;
}

void GlicTabUnderlineView::UpdateShader() {
  if (base::FeatureList::IsEnabled(features::kGlicParameterizedShader) &&
      !colors_.empty() && !floats_.empty()) {
    shader_ =
        ForceSimplifiedShader()
            ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_SIMPLIFIED_PARAMETERIZED_BORDER_SHADER)
            : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_PARAMETERIZED_BORDER_SHADER);
  } else {
    shader_ =
        ForceSimplifiedShader()
            ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_SIMPLIFIED_BORDER_SHADER)
            : ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                  IDR_GLIC_BORDER_SHADER);
  }
}

BEGIN_METADATA(GlicTabUnderlineView)
END_METADATA

}  // namespace glic
