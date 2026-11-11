// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include <cmath>

#include "ash/bubble/bubble_utils.h"
#include "ash/login/ui/views_utils.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_decoration.h"
#include "ash/style/typography.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_textured.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_observer.h"

namespace arc::input_overlay {

namespace {

constexpr SkScalar kTriangleLength = 20.0f;
constexpr SkScalar kTriangleHeight = 14.0f;
// The straight distance from triangle rounded corner start to end.
constexpr SkScalar kTriangleRoundDistance = 4.0f;
constexpr SkScalar kCornerRadius = 16.0f;
constexpr SkScalar kHighlightBorderThickness = 2.0f;
constexpr SkScalar kHalfHighlightBorderThickness =
    kHighlightBorderThickness / 2.0f;

// Whole menu width with arrow.
constexpr int kMenuWidth = kButtonOptionsMenuWidth + kTriangleHeight;

constexpr float kDoneButtonCornerRadius = 16.0f;

// Gap from focus ring outer edge to the edge of the view.
constexpr float kDoneButtonHaloInset = -5.0f;
// Thickness of focus ring.
constexpr float kDoneButtonHaloThickness = 3.0f;

constexpr int kHeaderLeftMarginSpacing = 6;

// Draws the dialog shape path with round corner. It starts after the corner
// radius on line #0 and draws clockwise.
//
// `draw_triangle_on_left` draws the triangle wedge on the left side of the box
// instead of the right if set to true.
//
// `action_offset` draws the triangle wedge higher or lower if the position of
// the action is too close to the top or bottom of the window. An offset of
// zero draws the triangle wedge at the vertical center of the box.
//
// `origin_offset` of 0 means drawing the top and left edge on the x-axis and
// y-axis. Otherwise, draw the top and left edge on the y = origin_offset and x
// = origin_offset.
//  _0>__________
// |             |
// |             |
// |             |
// |              >
// |             |
// |             |
// |_____________|
//
SkPath BackgroundPath(SkScalar height,
                      SkScalar action_offset,
                      bool draw_triangle_on_left,
                      SkScalar origin_offset = 0,
                      SkScalar corner_radius = kCornerRadius) {
  SkPathBuilder path;
  const SkScalar short_length =
      SkIntToScalar(kMenuWidth) - kTriangleHeight - 2 * corner_radius;
  const SkScalar short_height = height - 2 * corner_radius;

  // Calculate values for drawing triangle rounded corner. Check b/324940844 for
  // calculation details.
  const SkScalar triangle_radius =
      kTriangleRoundDistance / 4 *
      std::sqrt(4 +
                std::pow(kTriangleLength, 2) / std::pow(kTriangleHeight, 2));
  const SkScalar dx =
      kTriangleHeight * kTriangleRoundDistance / kTriangleLength;
  const SkScalar dy = kTriangleRoundDistance / 2;

  // If the offset is greater than the limit or less than the negative
  // limit, set it respectively.
  const SkScalar limit = short_height / 2 - kTriangleLength / 2;
  if (action_offset > limit) {
    action_offset = limit;
  } else if (action_offset < -limit) {
    action_offset = -limit;
  }
  if (draw_triangle_on_left) {
    path.moveTo(corner_radius + kTriangleHeight + origin_offset, origin_offset);
  } else {
    path.moveTo(corner_radius + origin_offset, origin_offset);
  }
  // Top left after corner radius to top right corner radius.
  path.rLineTo(short_length, 0);
  path.rArcTo({corner_radius, corner_radius}, 0, SkPathBuilder::kSmall_ArcSize,
              SkPathDirection::kCW, {corner_radius, corner_radius});
  if (draw_triangle_on_left) {
    // Top right after corner radius to bottom right corner radius.
    path.rLineTo(0, short_height);
  } else {
    // Top right after corner radius to midway point.
    path.rLineTo(0, limit + action_offset);
    // Triangle shape.
    path.rLineTo(kTriangleHeight - dx, kTriangleLength / 2 - dy);
    // Draw triangle rounded corner.
    path.rArcTo({triangle_radius, triangle_radius}, 0,
                SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                {0, kTriangleRoundDistance});
    path.rLineTo(-kTriangleHeight + dx, kTriangleLength / 2 - dy);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, limit - action_offset);
  }
  path.rArcTo({corner_radius, corner_radius}, 0, SkPathBuilder::kSmall_ArcSize,
              SkPathDirection::kCW, {-corner_radius, corner_radius});
  // Bottom right after corner radius to bottom left corner radius.
  path.rLineTo(-short_length, 0);
  path.rArcTo({corner_radius, corner_radius}, 0, SkPathBuilder::kSmall_ArcSize,
              SkPathDirection::kCW, {-corner_radius, -corner_radius});
  if (draw_triangle_on_left) {
    // bottom left after corner radius to midway point.
    path.rLineTo(0, -limit + action_offset);
    // Triangle shape.
    path.rLineTo(-kTriangleHeight + dx, -kTriangleLength / 2 + dy);
    // Draw triangle rounded corner.
    path.rArcTo({triangle_radius, triangle_radius}, 0,
                SkPathBuilder::kSmall_ArcSize, SkPathDirection::kCW,
                {0, -kTriangleRoundDistance});
    path.rLineTo(kTriangleHeight - dx, -kTriangleLength / 2 + dy);
    // After midway point to bottom right corner radius.
    path.rLineTo(0, -limit - action_offset);
  } else {
    // Bottom left after corner radius to top left corner radius.
    path.rLineTo(0, -short_height);
  }
  path.rArcTo({corner_radius, corner_radius}, 0, SkPathBuilder::kSmall_ArcSize,
              SkPathDirection::kCW, {corner_radius, -corner_radius});
  // Path finish.
  path.close();
  return path.detach();
}

gfx::ShadowValues GetShadowValues() {
  return gfx::ShadowValue::MakeChromeOSSystemUIShadowValues(
      ash::SystemDecoration::GetElevationFromType(
          ash::SystemDecoration::ShadowType::kElevation12));
}

// Returns negative insets to expand the shadow layer bigger than the container.
gfx::Insets GetShadowInsets() {
  return gfx::ShadowValue::GetMargin(GetShadowValues());
}

}  // namespace

// ButtonOptionsMenu is not a regular shadow container, so it needs to draw the
// special shape of the shadow in the ShadowLayer.
class ButtonOptionsMenu::ShadowLayer : public ui::LayerTextured,
                                       public ui::LayerDelegate,
                                       public views::ViewObserver {
 public:
  explicit ShadowLayer(ButtonOptionsMenu* owner) : owner_(owner) {
    // TODO(b/331837116): Check the shadow distance and blur again after the
    // system shadow is adjusted to keep them consistent.
    SetName("ButtonOptionsMenu:ShadowLayer");
    SetFillsBoundsOpaquely(false);
    set_delegate(this);
  }

  ShadowLayer(const ShadowLayer&) = delete;
  ShadowLayer& operator=(const ShadowLayer&) = delete;

  ~ShadowLayer() override = default;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override {
    if (bounds().IsEmpty()) {
      return;
    }
    ui::PaintRecorder recorder(context, size());
    auto* canvas = recorder.canvas();

    const auto shadow_values = GetShadowValues();
    gfx::Insets shadow_insets = -gfx::ShadowValue::GetMargin(shadow_values);

    cc::PaintFlags flags;
    flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values));
    flags.setColor(SK_ColorTRANSPARENT);
    flags.setAntiAlias(true);

    canvas->DrawPath(
        BackgroundPath(SkIntToScalar(owner_->size().height()),
                       SkIntToScalar(owner_->arrow_vertical_offset_),
                       owner_->arrow_on_left_, shadow_insets.top()),
        flags);
  }

  // ui::LayerDelegate:
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    auto shadow_layer_bounds = observed_view->layer()->bounds();
    shadow_layer_bounds.Inset(GetShadowInsets());
    SetBounds(shadow_layer_bounds);
  }

  void OnViewLayoutInvalidated(views::View* observed_view) override {
    // When the `observed_view` is relayout without bounds change, it also needs
    // to redraw the shadow because the triangle arrow position may change.
    SchedulePaint(gfx::Rect(size()));
  }

  void OnViewRemovedFromWidget(views::View* observed_view) override {
    observed_view->RemoveObserver(this);
  }

 private:
  raw_ptr<ButtonOptionsMenu> owner_;
};

// ButtonOptionsActionEdit shows in ButtonOptions and is associated with each
// of Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------
class ButtonOptionsActionEdit : public ActionEditView {
  METADATA_HEADER(ButtonOptionsActionEdit, ActionEditView)

 public:
  ButtonOptionsActionEdit(DisplayOverlayController* controller, Action* action)
      : ActionEditView(controller,
                       action,
                       /*is_editing_list=*/false) {
    name_tag_->SetTitle(l10n_util::GetStringUTF16(
        action_->is_new() ? IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGN_NEW_KEY
                          : IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGNED_KEY));
  }
  ButtonOptionsActionEdit(const ButtonOptionsActionEdit&) = delete;
  ButtonOptionsActionEdit& operator=(const ButtonOptionsActionEdit&) = delete;
  ~ButtonOptionsActionEdit() override = default;

  // ActionEditView:
  void OnActionInputBindingUpdated() override {
    ActionEditView::OnActionInputBindingUpdated();
    name_tag_->SetTitle(l10n_util::GetStringUTF16(
        IDS_INPUT_OVERLAY_BUTTON_OPTIONS_ASSIGNED_KEY));
  }

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override {
    if (is_visible && action_->is_new() &&
        labels_view_->IsFirstLabelUnassigned() &&
        controller_->HasSingleUserAddedAction()) {
      PerformPulseAnimation();
    }
  }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  // ActionEditView:
  void ClickCallback() override { labels_view_->FocusLabel(); }
};

BEGIN_METADATA(ButtonOptionsActionEdit)
END_METADATA

// DoneButton shows in ButtonOptions and allows the user to finish editing the
// action.
// ------------------------------
// ||        Done button       ||
// ------------------------------
class DoneButton : public views::LabelButton {
  METADATA_HEADER(DoneButton, views::LabelButton)

 public:
  explicit DoneButton(PressedCallback pressed_callback)
      : LabelButton(std::move(pressed_callback),
                    l10n_util::GetStringUTF16(
                        IDS_INPUT_OVERLAY_EDITING_DONE_BUTTON_LABEL)) {
    GetViewAccessibility().SetName(
        l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDITING_DONE_BUTTON_LABEL));

    SetBackground(
        views::CreateRoundedRectBackground(cros_tokens::kCrosSysSystemOnBase,
                                           /*radius=*/kDoneButtonCornerRadius));
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(14, 0)));
    SetProperty(views::kMarginsKey, gfx::Insets::TLBR(12, 0, 0, 0));

    ash::TypographyProvider::Get()->StyleLabel(
        ash::TypographyToken::kCrosButton2, *label());
    SetEnabledTextColors(cros_tokens::kCrosSysOnSurface);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);

    // Set highlight path.
    views::HighlightPathGenerator::Install(
        this, std::make_unique<views::RoundRectHighlightPathGenerator>(
                  gfx::Insets(), /*corner_radius=*/kDoneButtonCornerRadius));

    // Set up highlight and focus ring for `DoneButton`.
    ash::StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                          /*highlight_on_hover=*/false,
                                          /*highlight_on_focus=*/false);
    auto* focus_ring = views::FocusRing::Get(this);
    focus_ring->SetHaloInset(kDoneButtonHaloInset);
    focus_ring->SetHaloThickness(kDoneButtonHaloThickness);
  }

  DoneButton(const DoneButton&) = delete;
  DoneButton& operator=(const DoneButton&) = delete;

  ~DoneButton() override = default;
};

BEGIN_METADATA(DoneButton)
END_METADATA

ButtonOptionsMenu::ButtonOptionsMenu(DisplayOverlayController* controller,
                                     Action* action)
    : TouchInjectorObserver(), controller_(controller), action_(action) {
  UpdateBorder();

  // Add shadow.
  shadow_layer_ = std::make_unique<ShadowLayer>(this);
  AddLayerToRegion(shadow_layer_.get(), views::LayerRegion::kBelow);
  AddObserver(shadow_layer_.get());

  controller_->AddTouchInjectorObserver(this);
  Init();
}

ButtonOptionsMenu::~ButtonOptionsMenu() {
  controller_->RemoveTouchInjectorObserver(this);
}

void ButtonOptionsMenu::SetArrowVerticalOffset(int offset) {
  if (arrow_vertical_offset_ != offset) {
    arrow_vertical_offset_ = offset;
    SchedulePaint();
  }
}

void ButtonOptionsMenu::SetArrowOnLeft(bool arrow_on_left) {
  if (arrow_on_left_ != arrow_on_left) {
    arrow_on_left_ = arrow_on_left;
    UpdateBorder();
    SchedulePaint();
  }
}

void ButtonOptionsMenu::UpdateWidget() {
  auto* widget = GetWidget();
  CHECK(widget, base::NotFatalUntil::M160);

  controller_->UpdateWidgetBoundsInRootWindow(
      widget,
      gfx::Rect(action_->action_view()->CalculateAttachViewPositionInRootWindow(
                    /*available_bounds=*/CalculateAvailableBounds(
                        /*root_window=*/controller_->touch_injector()
                            ->window()
                            ->GetRootWindow()),
                    /*window_content_origin=*/
                    controller_->touch_injector()->content_bounds().origin(),
                    /*attached_view=*/this),
                GetPreferredSize()));
}

void ButtonOptionsMenu::Init() {
  SetUseDefaultFillLayout(true);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddHeader();
  AddEditTitle();
  AddActionSelection();
  AddActionEdit();
  AddDoneButton();
}

void ButtonOptionsMenu::UpdateBorder() {
  SetBorder(views::CreateEmptyBorder(
      arrow_on_left_
          ? gfx::Insets::TLBR(
                kButtonOptionsMenuHorizontalBorderInset,
                kButtonOptionsMenuHorizontalBorderInset + kTriangleHeight,
                kButtonOptionsMenuHorizontalBorderInset,
                kButtonOptionsMenuHorizontalBorderInset)
          : gfx::Insets::TLBR(
                kButtonOptionsMenuHorizontalBorderInset,
                kButtonOptionsMenuHorizontalBorderInset,
                kButtonOptionsMenuHorizontalBorderInset,
                kButtonOptionsMenuHorizontalBorderInset + kTriangleHeight)));
}

void ButtonOptionsMenu::AddHeader() {
  // ------------------------------------
  // ||"Button options"|          |icon||
  // ------------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  auto* layout = container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  container->SetProperty(views::kMarginsKey,
                         gfx::Insets::TLBR(0, kHeaderLeftMarginSpacing, 12, 0));

  action_name_label_ = container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosTitle1, u"", cros_tokens::kCrosSysOnSurface));
  action_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  action_name_label_->SetMultiLine(true);
  // Flex `action_name_label_` to fill empty space.
  layout->SetFlexForView(action_name_label_, /*flex=*/1);

  trash_button_ = container->AddChildView(std::make_unique<ash::IconButton>(
      base::BindRepeating(&ButtonOptionsMenu::OnTrashButtonPressed,
                          base::Unretained(this)),
      ash::IconButton::Type::kMedium, &ash::kGameControlsDeleteIcon,
      IDS_INPUT_OVERLAY_BUTTON_OPTIONS_DELETE_TOOLTIP_TEXT));
}

void ButtonOptionsMenu::AddEditTitle() {
  // ------------------------------
  // ||"Buttons let..."|          |
  // ------------------------------
  auto* label = AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosAnnotation2,
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_EDIT_INFO),
      cros_tokens::kCrosSysOnSurface));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMaximumWidth(kButtonOptionsMenuWidth -
                         2 * kButtonOptionsMenuHorizontalBorderInset -
                         kHeaderLeftMarginSpacing);
  label->SetProperty(views::kMarginsKey,
                     gfx::Insets::TLBR(0, kHeaderLeftMarginSpacing, 16, 0));
}

void ButtonOptionsMenu::AddActionSelection() {
  // ----------------------------------
  // | |"Choose your button type:"  | |
  // | |feature_tile| |feature_title| |
  // ----------------------------------
  auto* container = AddChildView(std::make_unique<views::View>());
  container->SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, /*top_radius=*/16.0f,
      /*bottom_radius=*/0.0f));
  container->SetUseDefaultFillLayout(true);
  container->SetProperty(views::kMarginsKey, gfx::Insets::TLBR(0, 0, 2, 0));
  container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      /*inside_border_insets=*/gfx::Insets::TLBR(12, 16, 16, 16),
      /*between_child_spacing=*/12));

  auto* label = container->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosButton2,
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_BUTTON_TYPE),
      cros_tokens::kCrosSysOnSurface));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetMaximumWidth(kButtonOptionsMenuWidth -
                         2 * kButtonOptionsMenuHorizontalBorderInset);

  button_group_ = container->AddChildView(
      ActionTypeButtonGroup::CreateButtonGroup(controller_, action_));
}

void ButtonOptionsMenu::AddActionEdit() {
  // ------------------------------
  // ||"Selected key" |key labels||
  // ||"key"                      |
  // ------------------------------
  action_edit_ = AddChildView(
      std::make_unique<ButtonOptionsActionEdit>(controller_, action_));
  action_name_label_->SetText(action_edit_->CalculateActionName());
}

void ButtonOptionsMenu::AddDoneButton() {
  // ------------------------------
  // ||        Done button       ||
  // ------------------------------
  AddChildView(std::make_unique<DoneButton>(base::BindRepeating(
      &ButtonOptionsMenu::OnDoneButtonPressed, base::Unretained(this))));
}

void ButtonOptionsMenu::OnTrashButtonPressed() {
  RecordButtonOptionsMenuFunctionTriggered(controller_->GetPackageName(),
                                           ButtonOptionsMenuFunction::kDelete);
  controller_->RemoveAction(action_);
}

void ButtonOptionsMenu::OnDoneButtonPressed() {
  controller_->SaveToProtoFile();
  RecordButtonOptionsMenuFunctionTriggered(controller_->GetPackageName(),
                                           ButtonOptionsMenuFunction::kDone);

  controller_->SetEditingListVisibility(/*visible=*/true);

  // Remove this view at last.
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  // Draw the shape.
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  ui::ColorProvider* color_provider = GetColorProvider();
  flags.setColor(
      color_provider->GetColor(cros_tokens::kCrosSysSystemBaseElevatedOpaque));

  const int height = GetHeightForWidth(kMenuWidth);
  canvas->DrawPath(
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);

  // Start to draw the highlight border.
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(kHalfHighlightBorderThickness);
  // Draw outside border.
  flags.setColor(views::HighlightBorder::GetBorderColor(
      *this, views::HighlightBorder::Type::kHighlightBorderOnShadow));
  canvas->DrawPath(
      BackgroundPath(SkIntToScalar(height),
                     SkIntToScalar(arrow_vertical_offset_), arrow_on_left_),
      flags);
  // Draw inside highlight.
  flags.setColor(views::HighlightBorder::GetHighlightColor(
      *this, views::HighlightBorder::Type::kHighlightBorderOnShadow));
  canvas->DrawPath(
      BackgroundPath(
          SkIntToScalar(height - kHighlightBorderThickness),
          SkIntToScalar(arrow_vertical_offset_), arrow_on_left_,
          /*origin_offset=*/kHalfHighlightBorderThickness,
          /*corner_radius=*/kCornerRadius - kHalfHighlightBorderThickness),
      flags);
}

gfx::Size ButtonOptionsMenu::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMenuWidth, GetLayoutManager()->GetPreferredHeightForWidth(
                                   this, kMenuWidth));
}

void ButtonOptionsMenu::OnActionRemoved(const Action& action) {
  if (action_ != &action) {
    return;
  }

  controller_->SetEditingListVisibility(/*visible=*/true);

  // Remove this view at last.
  controller_->RemoveButtonOptionsMenuWidget();
}

void ButtonOptionsMenu::OnActionTypeChanged(Action* action,
                                            Action* new_action) {
  CHECK_EQ(action_, action, base::NotFatalUntil::M160);
  action_ = new_action;
  button_group_->set_action(new_action);
  auto index = GetIndexOf(action_edit_);
  RemoveChildViewT(action_edit_);
  action_edit_ = AddChildViewAt(
      std::make_unique<ButtonOptionsActionEdit>(controller_, action_), *index);
  action_name_label_->SetText(action_edit_->CalculateActionName());
  UpdateWidget();
}

void ButtonOptionsMenu::OnActionInputBindingUpdated(const Action& action) {
  if (action_ == &action) {
    action_edit_->OnActionInputBindingUpdated();
    action_name_label_->SetText(action_edit_->CalculateActionName());
  }
}

void ButtonOptionsMenu::OnActionNewStateRemoved(const Action& action) {
  if (action_ == &action) {
    action_edit_->RemoveNewState();
  }
}

BEGIN_METADATA(ButtonOptionsMenu)
END_METADATA

}  // namespace arc::input_overlay
