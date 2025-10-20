// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_tab_underline_view.h"

#include "base/debug/crash_logging.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/favicon_size.h"
#include "ui/views/view_class_properties.h"

namespace glic {
namespace {

// The total duration of the underline's animation cycle.
constexpr static base::TimeDelta kCycleDuration = base::Milliseconds(3000);

// The width to use for the underline when tabs reach a small size.
constexpr static int kSmallUnderlineWidth = gfx::kFaviconSize;

// The width to use for the underline at the smallest tab sizes when tab
// contents begin to be clipped.
constexpr static int kMinUnderlineWidth = kSmallUnderlineWidth - 4;

// The threshold for tab width at which `kMinUnderlineWidth` should be used.
constexpr static int kMinimumTabWidthThreshold = 42;

// The height of the underline effect.
constexpr static int kEffectHeight = 2;

// The radius to use for rounded corners of the underline effect.
constexpr static float kCornerRadius = kEffectHeight / 2.0f;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(GlicTabUnderlineView,
                                      kGlicTabUnderlineElementId);

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

    if (!base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
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

      // Observe changes in the floaty state.
      glic_service->GetSingleInstanceWindowController().AddStateObserver(this);
    }

    // Subscribe to changes in the set of pinned tabs.
    pinned_tabs_change_subscription_ =
        sharing_manager.AddPinnedTabsChangedCallback(base::BindRepeating(
            &GlicTabUnderlineView::UnderlineViewUpdater::OnPinnedTabsChanged,
            base::Unretained(this)));

    // Subscribe to when new requests are made by glic.
    user_input_submitted_subscription_ =
        glic_service->AddUserInputSubmittedCallback(base::BindRepeating(
            &GlicTabUnderlineView::UnderlineViewUpdater::OnUserInputSubmitted,
            base::Unretained(this)));
  }
  UnderlineViewUpdater(const UnderlineViewUpdater&) = delete;
  UnderlineViewUpdater& operator=(const UnderlineViewUpdater&) = delete;
  ~UnderlineViewUpdater() override {
    if (!base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      GetGlicKeyedService()
          ->GetSingleInstanceWindowController()
          .RemoveStateObserver(this);
    }
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
                         const PanelStateContext& context) override {
    UpdateUnderlineView(
        panel_state.kind == mojom::PanelStateKind::kHidden
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
        if (IsUnderlineTabPinned() &&
            (base::FeatureList::IsEnabled(features::kGlicMultiInstance) ||
             IsGlicWindowShowing())) {
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
        if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
          ShowAndAnimateUnderline();
        } else {
          // If `underline_view_` is not visible, then this tab was just added
          // to the set of pinned tabs.
          if (!underline_view_->IsShowing()) {
            // Pinned tab underlines should only be visible while the glic panel
            // is open. For multi-instance this is controlled via the pinned
            // tabs api.
            if (IsGlicWindowShowing()) {
              ShowAndAnimateUnderline();
            }
          } else {
            // This tab was already pinned - re-animate to reflect the change in
            // the set of pinned tabs.
            AnimateUnderline();
          }
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
  void AnimateUnderline() { underline_view_->ResetAnimationCycle(); }

  void ShowOrAnimatePinnedUnderline() {
    if (!IsUnderlineTabPinned()) {
      return;
    }
    // For multi-instance, we rely on the umbrella sharing manager behavior to
    // determine when to show or not show underlines via the pinned tabs api.
    if (!base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
      // Pinned underlines should never be visible if the glic window is closed.
      if (!IsGlicWindowShowing()) {
        return;
      }
    }
    if (underline_view_->IsShowing()) {
      AnimateUnderline();
    } else {
      ShowAndAnimateUnderline();
    }
  }

  bool IsGlicWindowShowing() const {
    return underline_view_->GetGlicService()->IsWindowShowing();
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
    : GlicAnimatedEffectView(browser, std::move(tester)),
      updater_(std::make_unique<UnderlineViewUpdater>(browser, this)),
      tab_(tab) {
  SetProperty(views::kElementIdentifierKey, kGlicTabUnderlineElementId);
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

bool GlicTabUnderlineView::IsCycleDone(base::TimeTicks timestamp) {
  return progress_ == 1.f;
}

base::TimeDelta GlicTabUnderlineView::GetTotalDuration() const {
  return kCycleDuration;
}

void GlicTabUnderlineView::PopulateShaderUniforms(
    std::vector<cc::PaintShader::FloatUniform>& float_uniforms,
    std::vector<cc::PaintShader::Float2Uniform>& float2_uniforms,
    std::vector<cc::PaintShader::Float4Uniform>& float4_uniforms,
    std::vector<cc::PaintShader::IntUniform>& int_uniforms) const {
  const auto u_resolution = GetLocalBounds();
  // Insets aren't relevant to the tab underline effect, but are defined in the
  // uniforms of the GlicBorderView shader.
  gfx::Insets uniform_insets = gfx::Insets();

  float_uniforms.push_back(
      {.name = SkString("u_time"), .value = GetEffectTime()});
  float_uniforms.push_back(
      {.name = SkString("u_emphasis"), .value = emphasis_});
  float_uniforms.push_back(
      {.name = SkString("u_insets"),
       .value = static_cast<float>(uniform_insets.left())});
  float_uniforms.push_back(
      {.name = SkString("u_progress"), .value = progress_});

  float2_uniforms.push_back(
      // TODO(https://crbug.com/406026829): Ideally `u_resolution` should be a
      // vec4(x, y, w, h) and does not assume the origin is (0, 0). This way we
      // can eliminate `u_insets` and void the shader-internal origin-padding.
      {.name = SkString("u_resolution"),
       .value = SkV2{static_cast<float>(u_resolution.width()),
                     static_cast<float>(u_resolution.height())}});

  int_uniforms.push_back(
      {.name = SkString("u_dark"),
       .value = theme_service_->BrowserUsesDarkColors() ? 1 : 0});

  float4_uniforms.push_back({.name = SkString("u_corner_radius"),
                             .value = SkV4{kCornerRadius, kCornerRadius,
                                           kCornerRadius, kCornerRadius}});
}

int GlicTabUnderlineView::ComputeWidth() {
  // At the smallest tab sizes, favicons can be clipped and so a shorter
  // underline is required.
  if (size().width() < kMinimumTabWidthThreshold) {
    return kMinUnderlineWidth;
  }

  // Underline should use either the width of the tab's contents bounds or the
  // width of the favicon, whichever is greater.
  int underline_width = size().width() - tab_->GetInsets().width();
  if (underline_width < gfx::kFaviconSize) {
    return kSmallUnderlineWidth;
  }

  return underline_width;
}

void GlicTabUnderlineView::DrawEffect(gfx::Canvas* canvas,
                                      const cc::PaintFlags& flags) {
  int underline_width = ComputeWidth();
  int underline_x = (size().width() - underline_width + 1) / 2;

  // Draw the underline in the bottom `kEffectHeight` area of the given bounds
  // below the tab contents.
  gfx::Point origin(underline_x, size().height() - kEffectHeight);
  gfx::Size size(underline_width, kEffectHeight);
  gfx::Rect effect_bounds(origin, size);

  cc::PaintFlags new_flags(flags);
  // At small sizes, paint the underline as a solid color instead of a gradient.
  if (underline_width < gfx::kFaviconSize) {
    new_flags.setShader(nullptr);
    // `colors_` is not populated if the kGlicParameterizedShader feature is not
    // enabled.
    if (!colors_.empty()) {
      new_flags.setColor(colors_[0]);  // -gem-sys-color--brand-blue #3186FF
    } else {
      // Use -gem-sys-color--brand-blue as fallback color.
      const SkColor fallback_color = SkColorSetARGB(255, 49, 134, 255);
      new_flags.setColor(fallback_color);
    }
  }

  canvas->DrawRoundRect(gfx::RectF(effect_bounds), kCornerRadius, new_flags);
}

BEGIN_METADATA(GlicTabUnderlineView)
END_METADATA

}  // namespace glic
