// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/home_button.h"

#include <math.h>  // std::ceil

#include <memory>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/quick_app_access_model.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/ash_typography.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_control_button.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/typography.h"
#include "ash/user_education/user_education_class_properties.h"
#include "base/check_op.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// The space between the home button and quick app.
constexpr int kQuickAppStartMargin = 8;

constexpr uint8_t kAssistantVisibleAlpha = 255;    // 100% alpha
constexpr uint8_t kAssistantInvisibleAlpha = 138;  // 54% alpha

// Nudge animation constants

// The offsets that the home button moves up/down from the original home button
// position at each stage of nudge animation.
constexpr int kAnimationBounceUpOffset = 12;
constexpr int kAnimationBounceDownOffset = 3;

// Constants used on `nudge_ripple_layer_` animation.
constexpr base::TimeDelta kHomeButtonAnimationDuration =
    base::Milliseconds(250);
constexpr base::TimeDelta kRippleAnimationDuration = base::Milliseconds(2000);

// Constants used on `nudge_label_` animation.
//
// The duration of the showing/hiding animation for nudge label.
constexpr base::TimeDelta kNudgeLabelTransitionOnDuration =
    base::Milliseconds(300);
constexpr base::TimeDelta kNudgeLabelTransitionOffDuration =
    base::Milliseconds(500);

// The duration of the fade out animation that animates `nudge_label_` when
// users click on the home button while `nudge_label_` is showing.
constexpr base::TimeDelta kNudgeLabelFadeOutDuration = base::Milliseconds(100);

// The duration that the nudge label is shown.
constexpr base::TimeDelta kNudgeLabelShowingDuration = base::Seconds(6);

// The minimum space we want to keep between the `nudge_label_` and the first
// app in hotseat. Used to determine if `nudge_label_` should be shown.
constexpr int kMinSpaceBetweenNudgeLabelAndHotseat = 24;

// The durations and delay used for animating in the `quick_app_button_` and
// `expandable_container_`.
constexpr base::TimeDelta kQuickAppSlideSlideInDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kQuickAppButtonFadeInDelay = base::Milliseconds(50);
constexpr base::TimeDelta kQuickAppButtonFadeInDuration =
    base::Milliseconds(50);
constexpr base::TimeDelta kQuickAppContainerFadeInDuration =
    base::Milliseconds(100);

// The durations used for animating out the `quick_app_button_` and
// `expandable_container_`.
constexpr base::TimeDelta kQuickAppSlideOutDuration = base::Milliseconds(200);
constexpr base::TimeDelta kQuickAppFadeOutDuration = base::Milliseconds(100);

}  // namespace

class HomeButton::ButtonImageView : public views::View {
  METADATA_HEADER(ButtonImageView, views::View)

 public:
  explicit ButtonImageView(HomeButtonController* button_controller)
      : button_controller_(button_controller) {
    SetCanProcessEventsWithinSubtree(false);
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    UpdateBackground();
    UpdateIconImageModel();
  }

  ButtonImageView(const ButtonImageView&) = delete;
  ButtonImageView& operator=(const ButtonImageView&) = delete;

  ~ButtonImageView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

    if (!image_.isNull()) {
      canvas->DrawImageInt(image_, (width() - image_.width()) / 2,
                           (height() - image_.height()) / 2, cc::PaintFlags());
      return;
    }

    gfx::PointF circle_center(gfx::Rect(size()).CenterPoint());

    const bool is_assistant_available =
        button_controller_->IsAssistantAvailable();
    // Paint a white ring as the foreground for the app list circle. The
    // ceil/dsf math assures that the ring draws sharply and is centered at all
    // scale factors.
    const float ring_outer_radius_dp = is_assistant_available ? 8.0f : 7.0f;
    const float ring_thickness_dp = is_assistant_available ? 1.0f : 1.5f;
    {
      gfx::ScopedCanvas scoped_canvas(canvas);
      const float dsf = canvas->UndoDeviceScaleFactor();
      circle_center.Scale(dsf);
      cc::PaintFlags fg_flags;
      fg_flags.setAntiAlias(true);
      fg_flags.setStyle(cc::PaintFlags::kStroke_Style);
      fg_flags.setColor(GetColorProvider()->GetColor(GetIconColorId()));

      if (is_assistant_available) {
        // active: 100% alpha, inactive: 54% alpha
        fg_flags.setAlphaf(button_controller_->IsAssistantVisible()
                               ? kAssistantVisibleAlpha / 255.0f
                               : kAssistantInvisibleAlpha / 255.0f);
      }

      const float thickness = std::ceil(ring_thickness_dp * dsf);
      const float radius =
          std::ceil(ring_outer_radius_dp * dsf) - thickness / 2;
      fg_flags.setStrokeWidth(thickness);
      // Make sure the center of the circle lands on pixel centers.
      canvas->DrawCircle(circle_center, radius, fg_flags);

      if (is_assistant_available) {
        fg_flags.setAlphaf(1.0f);
        const float kCircleRadiusDp = 5.f;
        fg_flags.setStyle(cc::PaintFlags::kFill_Style);
        canvas->DrawCircle(circle_center, std::ceil(kCircleRadiusDp * dsf),
                           fg_flags);
      }
    }
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    if (image_model_) {
      image_ = image_model_->Rasterize(GetColorProvider());
    }
    SchedulePaint();
  }

  // Updates the button image view for the new shelf config.
  void UpdateForShelfConfigChange() {
    layer()->SetBackgroundBlur(
        ShelfConfig::Get()->GetShelfControlButtonBlurRadius());
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    UpdateBackground();
    UpdateIconImageModel();
  }

  void SetToggled(bool toggled) {
    if (toggled_ == toggled) {
      return;
    }

    toggled_ = toggled;
    UpdateBackground();
    UpdateIconImageModel();
    SchedulePaint();
  }

 private:
  // Updates the view background to match the current shelf config.
  void UpdateBackground() {
    auto* const shelf_config = ShelfConfig::Get();

    if (shelf_config->in_tablet_mode() && shelf_config->is_in_app()) {
      SetBackground(nullptr);
      SetBorder(nullptr);
      return;
    }

    SetBackground(views::CreateThemedRoundedRectBackground(
        GetBackgroundColorId(), shelf_config->control_border_radius()));

    if (shelf_config->in_tablet_mode() && !shelf_config->is_in_app()) {
      SetBorder(std::make_unique<views::HighlightBorder>(
          shelf_config->control_border_radius(),
          views::HighlightBorder::Type::kHighlightBorderOnShadow));
    } else {
      SetBorder(nullptr);
    }
  }

  ui::ColorId GetIconColorId() {
    return toggled_ && !ShelfConfig::Get()->in_tablet_mode()
               ? cros_tokens::kCrosSysSystemOnPrimaryContainer
               : cros_tokens::kCrosSysOnSurface;
  }

  ui::ColorId GetBackgroundColorId() {
    if (ShelfConfig::Get()->in_tablet_mode()) {
      return cros_tokens::kCrosSysSystemBaseElevated;
    }

    return toggled_ ? cros_tokens::kCrosSysSystemPrimaryContainer
                    : cros_tokens::kCrosSysSystemOnBase;
  }

  void UpdateIconImageModel() {
    const std::string campbell_config = base::GetFieldTrialParamValueByFeature(
        features::kCampbellGlyph, "icon");

    if (!campbell_config.empty() && switches::IsCampbellSecretKeyMatched()) {
      if (campbell_config == "hero") {
        image_model_ =
            ui::ImageModel::FromVectorIcon(kCampbellHeroIcon, GetIconColorId());
      } else if (campbell_config == "action") {
        image_model_ = ui::ImageModel::FromVectorIcon(kCampbellActionIcon,
                                                      GetIconColorId());
      } else if (campbell_config == "text") {
        image_model_ =
            ui::ImageModel::FromVectorIcon(kCampbellTextIcon, GetIconColorId());
      } else if (campbell_config == "9dot") {
        image_model_ =
            ui::ImageModel::FromVectorIcon(kCampbell9dotIcon, GetIconColorId());
      }
    } else if (Shell::Get()->keyboard_capability()->GetMetaKeyToDisplay() ==
               ui::mojom::MetaKey::kLauncherRefresh) {
      image_model_ =
          ui::ImageModel::FromVectorIcon(kCampbellHeroIcon, GetIconColorId());
    } else {
      image_model_ = std::nullopt;
      image_ = gfx::ImageSkia();
      return;
    }

    if (image_model_ && GetColorProvider()) {
      image_ = image_model_->Rasterize(GetColorProvider());
    } else {
      image_ = gfx::ImageSkia();
    }
  }

  const raw_ptr<HomeButtonController> button_controller_;

  gfx::ImageSkia image_;
  std::optional<ui::ImageModel> image_model_;

  bool toggled_ = false;
};

BEGIN_METADATA(HomeButton, ButtonImageView)
END_METADATA

// HomeButton::ScopedNoClipRect ------------------------------------------------

HomeButton::ScopedNoClipRect::ScopedNoClipRect(
    ShelfNavigationWidget* shelf_navigation_widget)
    : shelf_navigation_widget_(shelf_navigation_widget),
      clip_rect_(shelf_navigation_widget_->GetLayer()->clip_rect()) {
  shelf_navigation_widget_->GetLayer()->SetClipRect(gfx::Rect());
}

HomeButton::ScopedNoClipRect::~ScopedNoClipRect() {
  // The shelf_navigation_widget_ may be destructed before this dtor is
  // called.
  if (shelf_navigation_widget_->GetLayer())
    shelf_navigation_widget_->GetLayer()->SetClipRect(clip_rect_);
}

// HomeButton::ScopedNoClipRect ------------------------------------------------

HomeButton::HomeButton(Shelf* shelf)
    : ShelfControlButton(shelf, this),
      shelf_(shelf),
      controller_(this) {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  // When Jelly is disabled, the toggled state is achieved by activating ink
  // drop from the home button controller. Given that the controller manages ink
  // drop on gesture events itself, disable the default on-gesture ink drop
  // behavior.
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);

  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
  layer()->SetName("shelf/Homebutton");

  // Added at 0 index to ensure it's painted below focus ring view.
  button_image_view_ =
      AddChildViewAt(std::make_unique<ButtonImageView>(&controller_), 0);

  if (features::IsHomeButtonWithTextEnabled()) {
    // Directly shows the nudge label if the text-in-shelf feature is enabled.
    CreateNudgeLabel();
    expandable_container_->SetVisible(true);
    shelf_->shelf_layout_manager()->LayoutShelf(false);
  }

  if (features::IsHomeButtonQuickAppAccessEnabled() &&
      !features::IsHomeButtonWithTextEnabled()) {
    shell_observation_.Observe(Shell::Get());
    app_list_model_observation_.Observe(AppListModelProvider::Get());
    quick_app_model_observation_.Observe(
        AppListModelProvider::Get()->quick_app_access_model());
  }

  if (features::IsUserEducationEnabled()) {
    // NOTE: Set `kHelpBubbleContextKey` before `views::kElementIdentifierKey`
    // in case registration causes a help bubble to be created synchronously.
    SetProperty(kHelpBubbleContextKey, HelpBubbleContext::kAsh);
  }
  SetProperty(views::kElementIdentifierKey, kHomeButtonElementId);

  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  ShelfConfig::Get()->AddObserver(this);
}

HomeButton::~HomeButton() {
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  ShelfConfig::Get()->RemoveObserver(this);
}

gfx::Size HomeButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const gfx::Size control_button_size =
      ShelfControlButton::CalculatePreferredSize(available_size);

  // Take the preferred size of the expandable container into consideration when
  // it is visible. Note that the button width is already included in the label
  // width.
  if (expandable_container_ && expandable_container_->GetVisible()) {
    const gfx::Size container_size = expandable_container_->GetPreferredSize();
    return gfx::Size(
        std::max(control_button_size.width(), container_size.width()),
        std::max(control_button_size.height(), container_size.height()));
  }

  return control_button_size;
}

void HomeButton::Layout(PassKey) {
  LayoutSuperclass<ShelfControlButton>(this);

  button_image_view_->SetBoundsRect(
      gfx::Rect(ShelfControlButton::CalculatePreferredSize({})));

  if (expandable_container_) {
    if (shelf_->IsHorizontalAlignment()) {
      expandable_container_->SetSize(gfx::Size(
          expandable_container_->GetPreferredSize().width(), height()));
    } else {
      expandable_container_->SetSize(gfx::Size(
          width(), expandable_container_->GetPreferredSize().height()));
    }

    if (quick_app_button_) {
      if (shelf_->IsHorizontalAlignment()) {
        expandable_container_->SetBorder(
            views::CreateEmptyBorder(gfx::Insets::TLBR(
                0,
                ShelfControlButton::CalculatePreferredSize({}).width() +
                    kQuickAppStartMargin,
                0, 0)));
      } else {
        expandable_container_->SetBorder(
            views::CreateEmptyBorder(gfx::Insets::TLBR(
                ShelfControlButton::CalculatePreferredSize({}).height() +
                    kQuickAppStartMargin,
                0, 0, 0)));
      }
      expandable_container_->layer()->SetClipRect(
          gfx::Rect(expandable_container_->size()));
    }
  }
}

void HomeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (!controller_.MaybeHandleGestureEvent(event))
    Button::OnGestureEvent(event);
}

std::u16string HomeButton::GetTooltipText(const gfx::Point& p) const {
  // Don't show a tooltip if we're already showing the app list.
  return IsShowingAppList() ? std::u16string()
                            : GetViewAccessibility().GetCachedName();
}

void HomeButton::OnShelfButtonAboutToRequestFocusFromTabTraversal(
    ShelfButton* button,
    bool reverse) {
  DCHECK_EQ(button, this);
  const bool quick_app_focused =
      quick_app_button_ &&
      (GetFocusManager()->GetFocusedView() == quick_app_button_);

  // Focus out if:
  // *   The currently focused view is already this button, so focus out to
  //     ensure traversal to a different button.
  // *   Going forward with the quick app button currently focused, implies that
  //     the widget is trying to traverse forward to the next widget.
  // *   Going in reverse when the shelf has a back button, which implies that
  //     the widget is trying to loop back from the back button.
  if (GetFocusManager()->GetFocusedView() == this ||
      (quick_app_focused && !reverse) ||
      (reverse && shelf()->navigation_widget()->GetBackButton())) {
    shelf()->shelf_focus_cycler()->FocusOut(reverse,
                                            SourceView::kShelfNavigationView);
  }
}

void HomeButton::ButtonPressed(views::Button* sender,
                               const ui::Event& event,
                               views::InkDrop* ink_drop) {
  if (display::Screen::GetScreen()->InTabletMode()) {
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeButtonPressedTablet"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("AppList_HomeButtonPressedClamshell"));
  }

  Shell::Get()->app_list_controller()->ToggleAppList(
      GetDisplayId(), AppListShowSource::kShelfButton, event.time_stamp());

  // If the home button is pressed, fade out the nudge label if it is showing.
  if (expandable_container_ && !quick_app_button_) {
    // The label shouldn't be removed if the text-in-shelf feature is enabled.
    if (features::IsHomeButtonWithTextEnabled())
      return;

    if (!expandable_container_->GetVisible()) {
      // If the nudge label is not visible and will not be animating, directly
      // remove them as the nudge won't be showing anymore.
      RemoveNudgeLabel();
      return;
    }

    if (label_nudge_timer_.IsRunning())
      label_nudge_timer_.AbandonAndStop();
    AnimateNudgeLabelFadeOut();
  }
}

void HomeButton::OnShelfConfigUpdated() {
  button_image_view_->UpdateForShelfConfigChange();
}

void HomeButton::OnAssistantAvailabilityChanged() {
  // `button_image_view_` may not be set during `HomeButton` construction -
  // `button_image_view_` is created after `controller_`, which can end up
  // calling this method in response to registering assistant state observer.
  if (button_image_view_) {
    button_image_view_->SchedulePaint();
  }
}

bool HomeButton::IsShowingAppList() const {
  auto* controller = Shell::Get()->app_list_controller();
  return controller && controller->GetTargetVisibility(GetDisplayId());
}

void HomeButton::HandleLocaleChange() {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_SHELF_APP_LIST_LAUNCHER_TITLE));
  TooltipTextChanged();
  // Reset the bounds rect so the child layer bounds get updated on next shelf
  // layout if the RTL changed.
  SetBoundsRect(gfx::Rect());
}

int64_t HomeButton::GetDisplayId() const {
  aura::Window* window = GetWidget()->GetNativeWindow();
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window).id();
}

std::unique_ptr<HomeButton::ScopedNoClipRect>
HomeButton::CreateScopedNoClipRect() {
  return std::make_unique<HomeButton::ScopedNoClipRect>(
      shelf()->navigation_widget());
}

bool HomeButton::CanShowNudgeLabel() const {
  if (!shelf_->IsHorizontalAlignment())
    return false;

  // Avoid showing the text nudge label when a quick app button is shown.
  if (quick_app_button_) {
    return false;
  }

  // If there's no pinned app in shelf, shows the nudge label for the launcher
  // nudge.
  ShelfView* shelf_view = shelf_->hotseat_widget()->GetShelfView();
  int view_size = shelf_view->view_model()->view_size();
  if (view_size == 0)
    return true;

  // Need to have nudge_label_ existing to calculate the space for itself.
  DCHECK(nudge_label_);

  // For the calculation below, convert all points and rects to the root window
  // coordinate to make sure they are under the same coordinate.
  gfx::Rect first_app_bounds =
      shelf_view->view_model()->view_at(0)->GetMirroredBounds();
  first_app_bounds = shelf_view->ConvertRectToWidget(first_app_bounds);
  aura::Window* shelf_native_window =
      shelf_view->GetWidget()->GetNativeWindow();
  aura::Window::ConvertRectToTarget(shelf_native_window,
                                    shelf_native_window->GetRootWindow(),
                                    &first_app_bounds);

  gfx::Rect label_rect =
      ConvertRectToWidget(expandable_container_->GetMirroredBounds());
  aura::Window* native_window = GetWidget()->GetNativeWindow();
  DCHECK_EQ(shelf_native_window->GetRootWindow(),
            native_window->GetRootWindow());
  aura::Window::ConvertRectToTarget(
      native_window, native_window->GetRootWindow(), &label_rect);

  // Horizontal space between the `label_rect` and the first app in shelf, which
  // is also the app that is closest to the home button, is calculated here to
  // check if there's enough space to show the `nudge_label_`.
  int space = label_rect.ManhattanInternalDistance(first_app_bounds);
  return space >= kMinSpaceBetweenNudgeLabelAndHotseat;
}

void HomeButton::StartNudgeAnimation() {
  // Don't animate the label as it is already visible when text-in-shelf is
  // enabled.
  if (features::IsHomeButtonWithTextEnabled())
    return;

  // Ensure any in-progress nudge animations are completed before initializing
  // a new nudge animation, and creating a rippler layer. Nudge animation
  // callbacks may otherwise delete ripple layer mid new animation set up (and
  // delete the newly created ripple layer just before the layer animation is
  // set up by animation builder).
  nudge_ripple_layer_.ReleaseLayer();
  if (nudge_label_)
    nudge_label_->layer()->GetAnimator()->AbortAllAnimations();
  if (expandable_container_) {
    expandable_container_->layer()->GetAnimator()->AbortAllAnimations();
  }

  // Create the nudge label first to check if there is enough space to show it.
  if (!nudge_label_)
    CreateNudgeLabel();

  const bool can_show_nudge_label = CanShowNudgeLabel();

  views::AnimationBuilder builder;
  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnStarted(base::BindOnce(&HomeButton::OnNudgeAnimationStarted,
                                weak_ptr_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(can_show_nudge_label
                                  ? &HomeButton::OnLabelSlideInAnimationEnded
                                  : &HomeButton::OnNudgeAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&HomeButton::OnNudgeAnimationEnded,
                                weak_ptr_factory_.GetWeakPtr()))
      .Once();

  if (can_show_nudge_label) {
    AnimateNudgeLabelSlideIn(builder);
  } else {
    AnimateNudgeBounce(builder);
  }

  // Remove clip_rect from the home button and its ancestors as the animation
  // goes beyond its bounds. The object is deleted once the animation ends.
  scoped_no_clip_rect_ = CreateScopedNoClipRect();
  AnimateNudgeRipple(builder);
}

void HomeButton::SetToggled(bool toggled) {
  button_image_view_->SetToggled(toggled);
}

void HomeButton::AddNudgeAnimationObserverForTest(
    NudgeAnimationObserver* observer) {
  observers_.AddObserver(observer);
}
void HomeButton::RemoveNudgeAnimationObserverForTest(
    NudgeAnimationObserver* observer) {
  observers_.RemoveObserver(observer);
}

void HomeButton::OnThemeChanged() {
  ShelfControlButton::OnThemeChanged();

  if (ripple_layer_delegate_) {
    ripple_layer_delegate_->set_color(GetColorProvider()->GetColor(
        cros_tokens::kCrosSysRippleNeutralOnSubtle));
  }
  if (expandable_container_) {
    expandable_container_->layer()->SetColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase));
  }
}

void HomeButton::CreateExpandableContainer() {
  const int home_button_width =
      ShelfControlButton::CalculatePreferredSize({}).width();

  // Add container at 0 index so it's stacked under other views (e.g.
  // `button_image_view_`, and focus ring).
  expandable_container_ = AddChildViewAt(std::make_unique<views::View>(), 0);
  expandable_container_->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  expandable_container_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  expandable_container_->layer()->SetMasksToBounds(true);
  if (GetColorProvider()) {
    expandable_container_->layer()->SetColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSystemOnBase));
  }
  expandable_container_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(home_button_width / 2.f));
  expandable_container_->layer()->SetName("NudgeLabelContainer");
}

void HomeButton::CreateNudgeLabel() {
  DCHECK(!expandable_container_);

  CreateExpandableContainer();
  expandable_container_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, ShelfControlButton::CalculatePreferredSize({}).width(), 0, 16)));

  // Create a view to clip the `nudge_label_` to the area right of the home
  // button during nudge label animation.
  auto* label_mask =
      expandable_container_->AddChildView(std::make_unique<views::View>());
  label_mask->SetLayoutManager(std::make_unique<views::FillLayout>());
  label_mask->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::TLBR(0, 12, 0, 0)));
  label_mask->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  label_mask->layer()->SetMasksToBounds(true);
  label_mask->layer()->SetName("NudgeLabelMask");

  nudge_label_ = label_mask->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_SHELF_LAUNCHER_NUDGE_TEXT)));
  nudge_label_->SetAutoColorReadabilityEnabled(false);
  nudge_label_->SetPaintToLayer();
  nudge_label_->layer()->SetFillsBoundsOpaquely(false);
  nudge_label_->SetTextContext(CONTEXT_LAUNCHER_NUDGE_LABEL);
  nudge_label_->SetTextStyle(views::style::STYLE_EMPHASIZED);
  nudge_label_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                        *nudge_label_);
  expandable_container_->SetVisible(false);
  DeprecatedLayoutImmediately();
}

void HomeButton::CreateQuickAppButton() {
  CreateExpandableContainer();
  if (shelf_->IsHorizontalAlignment()) {
    expandable_container_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        0,
        ShelfControlButton::CalculatePreferredSize({}).width() +
            kQuickAppStartMargin,
        0, 0)));
  } else {
    expandable_container_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        ShelfControlButton::CalculatePreferredSize({}).height() +
            kQuickAppStartMargin,
        0, 0, 0)));
  }

  quick_app_button_ = expandable_container_->AddChildView(
      std::make_unique<views::ImageButton>(base::BindRepeating(
          &HomeButton::QuickAppButtonPressed, base::Unretained(this))));
  quick_app_button_->GetViewAccessibility().SetName(
      AppListModelProvider::Get()->quick_app_access_model()->GetAppName());

  const int control_size =
      ShelfControlButton::CalculatePreferredSize({}).width();

  const gfx::Size preferred_size = gfx::Size(control_size, control_size);

  quick_app_button_->SetPaintToLayer();
  quick_app_button_->layer()->SetFillsBoundsOpaquely(false);
  quick_app_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(
          AppListModelProvider::Get()->quick_app_access_model()->GetAppIcon(
              preferred_size)));
  views::HighlightPathGenerator::Install(
      quick_app_button_,
      std::make_unique<views::RoundRectHighlightPathGenerator>(
          gfx::Insets(views::FocusRing::kDefaultHaloThickness / 2),
          ShelfConfig::Get()->control_border_radius()));
  views::FocusRing::Get(quick_app_button_)
      ->SetColorId(cros_tokens::kCrosSysFocusRing);
  quick_app_button_->SetSize(preferred_size);

  shelf_->shelf_layout_manager()->LayoutShelf(false);
}

void HomeButton::QuickAppButtonPressed() {
  ash::Shell::Get()->app_list_controller()->ActivateItem(
      AppListModelProvider::Get()->quick_app_access_model()->quick_app_id(),
      /*event_flags=*/0, ash::AppListLaunchedFrom::kLaunchedFromQuickAppAccess,
      /*is_above_the_fold=*/false);
  AppListModelProvider::Get()->quick_app_access_model()->SetQuickAppActivated();
}

void HomeButton::AnimateNudgeRipple(views::AnimationBuilder& builder) {
  // Create the ripple layer and its delegate for the nudge animation.
  nudge_ripple_layer_.Reset(std::make_unique<ui::Layer>());
  ui::Layer* ripple_layer = nudge_ripple_layer_.layer();

  float ripple_diameter =
      ShelfControlButton::CalculatePreferredSize({}).width();
  auto* color_provider = GetColorProvider();
  DCHECK(color_provider);
  ripple_layer_delegate_ = std::make_unique<views::CircleLayerDelegate>(
      color_provider->GetColor(cros_tokens::kCrosSysRippleNeutralOnSubtle),
      /*radius=*/ripple_diameter / 2);

  // The bounds are set with respect to |shelf_container_layer| stated below.
  ripple_layer->SetBounds(
      gfx::Rect(layer()->parent()->bounds().x() + layer()->bounds().x(),
                layer()->parent()->bounds().y() + layer()->bounds().y(),
                ripple_diameter, ripple_diameter));

  ripple_layer->set_delegate(ripple_layer_delegate_.get());
  ripple_layer->SetMasksToBounds(true);
  ripple_layer->SetFillsBoundsOpaquely(false);

  // The position of the ripple layer is independent to the home button and its
  // parent shelf navigation widget. Therefore the ripple layer is added to the
  // shelf container layer, which is the parent layer of the shelf navigation
  // widget.
  ui::Layer* shelf_container_layer = GetWidget()->GetLayer()->parent();
  shelf_container_layer->Add(ripple_layer);
  shelf_container_layer->StackBelow(ripple_layer, layer()->parent());

  // The point of the center of the round button.
  const gfx::PointF ripple_center =
      gfx::RectF(gfx::SizeF(ripple_layer->size())).CenterPoint();

  gfx::Transform initial_disc_scale;
  initial_disc_scale.Scale(0.1f, 0.1f);
  gfx::Transform initial_state =
      gfx::TransformAboutPivot(ripple_center, initial_disc_scale);

  gfx::Transform final_disc_scale;
  final_disc_scale.Scale(3.0f, 3.0f);
  gfx::Transform scale_about_pivot =
      gfx::TransformAboutPivot(ripple_center, final_disc_scale);

  builder.GetCurrentSequence()
      .At(base::TimeDelta())
      // Set up the animation of the `nudge_ripple_layer_`
      .SetDuration(base::TimeDelta())
      .SetTransform(ripple_layer, initial_state)
      .SetOpacity(ripple_layer, 0.5f)
      .Then()
      .SetDuration(kRippleAnimationDuration)
      .SetTransform(ripple_layer, scale_about_pivot,
                    gfx::Tween::ACCEL_0_40_DECEL_100)
      .SetOpacity(ripple_layer, 0.0f, gfx::Tween::ACCEL_0_80_DECEL_80);
}

void HomeButton::AnimateNudgeBounce(views::AnimationBuilder& builder) {
  gfx::PointF bounce_up_point = shelf()->SelectValueForShelfAlignment(
      gfx::PointF(0, -kAnimationBounceUpOffset),
      gfx::PointF(kAnimationBounceUpOffset, 0),
      gfx::PointF(-kAnimationBounceUpOffset, 0));
  gfx::PointF bounce_down_point = shelf()->SelectValueForShelfAlignment(
      gfx::PointF(0, kAnimationBounceDownOffset),
      gfx::PointF(-kAnimationBounceDownOffset, 0),
      gfx::PointF(kAnimationBounceDownOffset, 0));

  gfx::Transform move_up;
  move_up.Translate(bounce_up_point.x(), bounce_up_point.y());
  gfx::Transform move_down;
  move_down.Translate(bounce_down_point.x(), bounce_down_point.y());

  // Home button movement settings. Note that the navigation widget layer
  // contains the non-opaque part of the home button and is also animated along
  // with the home button.
  ui::Layer* widget_layer = GetWidget()->GetLayer();

  // Set up the animation of the `widget_layer`, which bounce up and down during
  // the animation.
  builder.GetCurrentSequence()
      .At(base::TimeDelta())
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, move_up, gfx::Tween::FAST_OUT_SLOW_IN_3)
      .Then()
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, move_down, gfx::Tween::ACCEL_80_DECEL_20)
      .Then()
      .SetDuration(kHomeButtonAnimationDuration)
      .SetTransform(widget_layer, gfx::Transform(),
                    gfx::Tween::FAST_OUT_SLOW_IN_3);
}

void HomeButton::AnimateNudgeLabelSlideIn(views::AnimationBuilder& builder) {
  // Make sure the label is created.
  DCHECK(expandable_container_ && nudge_label_);

  // Update the shelf layout to provide space for the navigation widget.
  expandable_container_->SetVisible(true);
  shelf_->shelf_layout_manager()->LayoutShelf(false);

  const gfx::Rect initial_container_clip_rect =
      GetExpandableContainerClipRectToHomeButton();
  const gfx::Transform initial_transform =
      GetTransformForContainerChildBehindHomeButton();

  // Calculate the target clip rect on `expandable_container_`.
  const gfx::Rect container_target_clip_rect =
      gfx::Rect(expandable_container_->size());

  // Set up the animation of the `nudge_label_`
  builder.GetCurrentSequence()
      .At(base::TimeDelta())
      .SetDuration(base::TimeDelta())
      .SetTransform(nudge_label_->layer(), initial_transform)
      .SetClipRect(expandable_container_->layer(), initial_container_clip_rect)
      .SetOpacity(expandable_container_->layer(), 0)
      .Then()
      .SetDuration(kNudgeLabelTransitionOnDuration)
      .SetTransform(nudge_label_->layer(), gfx::Transform(),
                    gfx::Tween::ACCEL_5_70_DECEL_90)
      .SetClipRect(expandable_container_->layer(), container_target_clip_rect,
                   gfx::Tween::ACCEL_5_70_DECEL_90)
      .SetOpacity(expandable_container_->layer(), 1,
                  gfx::Tween::ACCEL_5_70_DECEL_90);
}

void HomeButton::AnimateNudgeLabelSlideOut() {
  const gfx::Transform target_transform =
      GetTransformForContainerChildBehindHomeButton();
  const gfx::Rect container_target_clip_rect =
      GetExpandableContainerClipRectToHomeButton();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&HomeButton::OnNudgeAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&HomeButton::OnNudgeAnimationEnded,
                                weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kNudgeLabelTransitionOffDuration)
      .SetTransform(nudge_label_->layer(), target_transform,
                    gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetClipRect(expandable_container_->layer(), container_target_clip_rect,
                   gfx::Tween::ACCEL_40_DECEL_100_3)
      .SetOpacity(expandable_container_->layer(), 0,
                  gfx::Tween::ACCEL_40_DECEL_100_3);
}

void HomeButton::AnimateNudgeLabelFadeOut() {
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&HomeButton::OnLabelFadeOutAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&HomeButton::OnLabelFadeOutAnimationEnded,
                                weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kNudgeLabelFadeOutDuration)
      .SetOpacity(expandable_container_->layer(), 0, gfx::Tween::LINEAR);
}

void HomeButton::OnNudgeAnimationStarted() {
  for (auto& observer : observers_)
    observer.NudgeAnimationStarted(this);
}

void HomeButton::OnNudgeAnimationEnded() {
  // Delete the ripple layer and its delegate after the launcher nudge animation
  // is completed.
  nudge_ripple_layer_.ReleaseLayer();
  ripple_layer_delegate_.reset();

  if (expandable_container_) {
    expandable_container_->SetVisible(false);
    shelf_->shelf_layout_manager()->LayoutShelf(false);
  }

  // Reset the clip rect after the animation is completed.
  scoped_no_clip_rect_.reset();

  for (auto& observer : observers_)
    observer.NudgeAnimationEnded(this);
}

void HomeButton::OnLabelSlideInAnimationEnded() {
  for (auto& observer : observers_)
    observer.NudgeLabelShown(this);

  // After the label is shown for `kNudgeLabelShowingDuration` amount of time,
  // move the label back to its original position.
  label_nudge_timer_.Start(
      FROM_HERE, kNudgeLabelShowingDuration,
      base::BindOnce(&HomeButton::AnimateNudgeLabelSlideOut,
                     base::Unretained(this)));
}

void HomeButton::OnLabelFadeOutAnimationEnded() {
  OnNudgeAnimationEnded();

  // If the label is faded out by clicking on it, remove the label as it is
  // assumed that the nudge won't be shown again.
  RemoveNudgeLabel();
}

void HomeButton::RemoveNudgeLabel() {
  nudge_label_ = nullptr;
  RemoveChildViewT(expandable_container_.ExtractAsDangling());
}

void HomeButton::RemoveQuickAppButton() {
  quick_app_button_ = nullptr;
  RemoveChildViewT(expandable_container_.ExtractAsDangling());
}

bool HomeButton::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect button_bounds = target->GetLocalBounds();

  // If the `expandable_container_` is visible, set all the area within the
  // label bounds clickable.
  if (expandable_container_ && expandable_container_->GetVisible()) {
    button_bounds = expandable_container_->layer()->bounds();
  }

  // Increase clickable area for the button to account for clicks around the
  // spacing. This will not intercept events outside of the parent widget.
  button_bounds.Inset(
      gfx::Insets::VH(-ShelfConfig::Get()->control_button_edge_spacing(
                          !shelf()->IsHorizontalAlignment()),
                      -ShelfConfig::Get()->control_button_edge_spacing(
                          shelf()->IsHorizontalAlignment())));
  return button_bounds.Intersects(rect);
}

void HomeButton::OnInputDeviceConfigurationChanged(uint8_t input_device_types) {
  if (input_device_types & InputDeviceEventObserver::kKeyboard) {
    button_image_view_->UpdateForShelfConfigChange();
  }
}

void HomeButton::OnDeviceListsComplete() {
  button_image_view_->UpdateForShelfConfigChange();
}

void HomeButton::OnShellDestroying() {
  shell_observation_.Reset();
  app_list_model_observation_.Reset();
  quick_app_model_observation_.Reset();
}

void HomeButton::OnActiveAppListModelsChanged(AppListModel* model,
                                              SearchModel* search_model) {
  QuickAppAccessModel* quick_model =
      AppListModelProvider::Get()->quick_app_access_model();
  quick_app_model_observation_.Reset();
  quick_app_model_observation_.Observe(quick_model);

  OnQuickAppShouldShowChanged(quick_model->quick_app_should_show_state());
}

void HomeButton::OnQuickAppShouldShowChanged(bool show_quick_app) {
  if (!show_quick_app && quick_app_button_) {
    AnimateQuickAppButtonOut();
  } else if (show_quick_app && !quick_app_button_) {
    if (nudge_label_) {
      RemoveNudgeLabel();
    }
    AnimateQuickAppButtonIn();
  }
}

void HomeButton::OnQuickAppIconChanged() {
  if (!quick_app_button_) {
    return;
  }

  const int control_size =
      ShelfControlButton::CalculatePreferredSize({}).width();
  quick_app_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(
          AppListModelProvider::Get()->quick_app_access_model()->GetAppIcon(
              gfx::Size(control_size, control_size))));
}

void HomeButton::AnimateQuickAppButtonIn() {
  CreateQuickAppButton();

  CHECK(quick_app_button_ && expandable_container_ && !nudge_label_);

  const gfx::Rect initial_container_clip_rect =
      GetExpandableContainerClipRectToHomeButton();
  const gfx::Transform initial_transform =
      GetTransformForContainerChildBehindHomeButton();

  // Calculate the target clip rect on `expandable_container_`.
  const gfx::Rect container_target_clip_rect =
      gfx::Rect(expandable_container_->size());

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(quick_app_button_->layer(), initial_transform)
      .SetClipRect(expandable_container_->layer(), initial_container_clip_rect)
      .SetOpacity(quick_app_button_->layer(), 0)
      .SetOpacity(expandable_container_->layer(), 0)
      .Then()
      .SetDuration(kQuickAppSlideSlideInDuration)
      .SetClipRect(expandable_container_->layer(), container_target_clip_rect,
                   gfx::Tween::ACCEL_20_DECEL_100)
      .SetTransform(quick_app_button_->layer(), gfx::Transform(),
                    gfx::Tween::ACCEL_20_DECEL_100)
      .At(kQuickAppButtonFadeInDelay)
      .SetDuration(kQuickAppButtonFadeInDuration)
      .SetOpacity(quick_app_button_->layer(), 1)
      .At(base::TimeDelta())
      .SetDuration(kQuickAppContainerFadeInDuration)
      .SetOpacity(expandable_container_->layer(), 1);
}

void HomeButton::AnimateQuickAppButtonOut() {
  const gfx::Transform target_transform =
      GetTransformForContainerChildBehindHomeButton();
  const gfx::Rect container_target_clip_rect =
      GetExpandableContainerClipRectToHomeButton();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&HomeButton::OnQuickAppButtonSlideOutDone,
                              weak_ptr_factory_.GetWeakPtr()))
      .OnAborted(base::BindOnce(&HomeButton::OnQuickAppButtonSlideOutDone,
                                weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kQuickAppSlideOutDuration)
      .SetTransform(quick_app_button_->layer(), target_transform,
                    gfx::Tween::ACCEL_20_DECEL_100)
      .SetClipRect(expandable_container_->layer(), container_target_clip_rect,
                   gfx::Tween::ACCEL_20_DECEL_100)
      .At(base::TimeDelta())
      .SetDuration(kQuickAppFadeOutDuration)
      .SetOpacity(expandable_container_->layer(), 0)
      .SetOpacity(quick_app_button_->layer(), 0);
}

void HomeButton::OnQuickAppButtonSlideOutDone() {
  RemoveQuickAppButton();
  shelf_->shelf_layout_manager()->LayoutShelf(true);
}

gfx::Transform HomeButton::GetTransformForContainerChildBehindHomeButton() {
  const int home_button_width =
      ShelfControlButton::CalculatePreferredSize({}).width();

  const int container_visible_width =
      expandable_container_->width() - home_button_width;

  gfx::Transform target_transform;
  if (shelf_->IsHorizontalAlignment()) {
    target_transform.Translate(base::i18n::IsRTL() ? container_visible_width
                                                   : -container_visible_width,
                               0);
  } else {
    target_transform.Translate(
        0, home_button_width - expandable_container_->height());
  }
  return target_transform;
}

gfx::Rect HomeButton::GetExpandableContainerClipRectToHomeButton() {
  const int home_button_width =
      ShelfControlButton::CalculatePreferredSize({}).width();
  const int container_visible_width =
      expandable_container_->width() - home_button_width;

  gfx::Rect clip_rect =
      gfx::Rect(base::i18n::IsRTL() ? container_visible_width : 0, 0,
                home_button_width, home_button_width);

  return clip_rect;
}

BEGIN_METADATA(HomeButton)
END_METADATA

}  // namespace ash
