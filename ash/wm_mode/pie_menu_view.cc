// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/pie_menu_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/color_util.h"
#include "ash/style/style_util.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kBackButtonRadius = 45;

constexpr int kButtonBorderStrokeWidth = 2;

constexpr int kIconTextVerticalSpacing = 5;

// Returns the size needed to paint the given `text` with the given `font_list`.
gfx::Size GetTextSize(const std::u16string& text,
                      const gfx::FontList& font_list) {
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, font_list, &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  return gfx::Size(width, height);
}

}  // namespace

// -----------------------------------------------------------------------------
// PieMenuButton:

// Defines a button that paints as a slice of a circle in pie menu. This button
// can have an associated sub menu container which it opens when pressed.
class PieMenuButton : public views::Button,
                      public views::MaskedTargeterDelegate {
  METADATA_HEADER(PieMenuButton, views::Button)

 public:
  PieMenuButton(int button_id,
                const std::u16string& button_label_text,
                const gfx::VectorIcon* icon)
      : button_id_(button_id),
        button_label_text_(button_label_text),
        icon_(icon) {
    SetTooltipText(button_label_text_);
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    StyleUtil::SetUpInkDropForButton(this);
    views::InkDropHost* const ink_drop = views::InkDrop::Get(this);
    ink_drop->SetCreateMaskCallback(base::BindRepeating(
        &PieMenuButton::CreateInkDropMask, base::Unretained(this)));
  }
  PieMenuButton(const PieMenuButton&) = delete;
  PieMenuButton& operator=(const PieMenuButton&) = delete;
  ~PieMenuButton() override = default;

  int button_id() const { return button_id_; }
  PieSubMenuContainerView* associated_sub_menu_container() {
    return associated_sub_menu_container_;
  }
  void set_associated_sub_menu_container(PieSubMenuContainerView* sub_menu) {
    associated_sub_menu_container_ = sub_menu;
  }

  void SetButtonIndexAndSweepAngle(int index, float sweep_angle) {
    button_index_ = index;
    sweep_angle_ = sweep_angle;
  }

  void SetButtonLabelText(const std::u16string& text) {
    button_label_text_ = text;
    // TODO(afakhry): Provide API to set the tooltip separately.
    SetTooltipText(button_label_text_);
    SchedulePaint();
  }

  // views::Button:
  void OnThemeChanged() override {
    Button::OnThemeChanged();
    RefreshIconImage();
  }

  void StateChanged(ButtonState old_state) override {
    RefreshIconImage();
    SchedulePaint();
  }

  void PaintButtonContents(gfx::Canvas* canvas) override {
    views::Button::PaintButtonContents(canvas);

    // Draw the base background color.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    auto* color_provider = GetColorProvider();
    flags.setColor(color_provider->GetColor(kColorAshShieldAndBase60));
    const SkPath path = ComputePieSlicePath(/*for_masking=*/false);
    canvas->DrawPath(path, flags);

    // Draw a highlight if the button is hovered.
    const ButtonState button_state = GetState();
    if (button_state == STATE_HOVERED) {
      flags.setColor(color_provider->GetColor(kColorAshInkDrop));
      canvas->DrawPath(path, flags);
    }

    const auto& font_list = views::Label::GetDefaultFontList();
    const gfx::Size text_size = GetTextSize(button_label_text_, font_list);
    int total_content_height = text_size.height();
    if (!icon_image_.isNull())
      total_content_height += (kIconTextVerticalSpacing + icon_image_.height());

    const auto contents_center = GetButtonContentsCenter();
    int y = contents_center.y() - (total_content_height / 2);

    // Draw the icon (if any).
    if (!icon_image_.isNull()) {
      const gfx::Point image_origin(
          contents_center.x() - (icon_image_.width() / 2), y);
      canvas->DrawImageInt(icon_image_, image_origin.x(), image_origin.y());
      y += (icon_image_.height() + kIconTextVerticalSpacing);
    }

    // Draw the button text.
    gfx::Rect text_bounds{
        gfx::Point{contents_center.x() - text_size.width() / 2, y}, text_size};

    // The label text might be very long to fit within the bounds of this
    // button's path. Therefore, we will use the `path` to have the canvas clip
    // anything outside it. We also need to make sure that the `text_bounds`
    // stays within the clip bounds.
    canvas->ClipPath(path, /*do_anti_alias=*/true);
    gfx::Rect clip_bounds;
    if (canvas->GetClipBounds(&clip_bounds))
      text_bounds.Intersect(clip_bounds);

    const auto text_color = color_provider->GetColor(
        button_state == STATE_DISABLED ? KColorAshTextDisabledColor
                                       : kColorAshTextColorPrimary);
    canvas->DrawStringRectWithFlags(button_label_text_, font_list, text_color,
                                    text_bounds,
                                    gfx::Canvas::TEXT_ALIGN_CENTER);

    // Draw the border stroke.
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(kButtonBorderStrokeWidth);
    flags.setColor(color_provider->GetColor(kColorAshSeparatorColor));
    canvas->DrawPath(path, flags);
  }

  // views::MaskedTargeterDelegate:
  bool GetHitTestMask(SkPath* mask) const override {
    mask->addPath(ComputePieSlicePath(/*for_masking=*/true));
    return true;
  }

  // Returns the point that will be the center of the contents painted on this
  // button e.g. the icon (if any) and text.
  gfx::Point GetButtonContentsCenter() const {
    const auto center = GetLocalBounds().CenterPoint();
    gfx::Transform transform;
    transform.Rotate(button_index_ * sweep_angle_);
    transform = gfx::TransformAboutPivot(gfx::PointF(center), transform);
    const gfx::Point image_center =
        center + gfx::Vector2d((width() + 2 * kBackButtonRadius) / 4, 0);
    return transform.MapPoint(image_center);
  }

 private:
  void RefreshIconImage() {
    if (!icon_)
      return;

    auto* color_provider = GetColorProvider();
    DCHECK(color_provider);

    icon_image_ = gfx::CreateVectorIcon(
        *icon_, color_provider->GetColor(GetState() == STATE_DISABLED
                                             ? kColorAshIconPrimaryDisabledColor
                                             : kColorAshIconColorPrimary));
  }

  // Creates and returns the mask that will be used to clip the ink drop to the
  // path of the slice of this button.
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const {
    return std::make_unique<views::PathInkDropMask>(
        size(), ComputePieSlicePath(/*for_masking=*/true));
  }

  // Returns the path representing a slice of circle that this button has the
  // shape of. `for_masking` is true when the returned path will be used for
  // hit testing and ink drop masking. Otherwise, it will be used for painting.
  SkPath ComputePieSlicePath(bool for_masking) const {
    const auto local_bounds = GetLocalBounds();
    gfx::Rect inner_circle_rect = local_bounds;
    inner_circle_rect.ClampToCenteredSize(
        gfx::Size(2 * kBackButtonRadius, 2 * kBackButtonRadius));

    SkPath path;
    // Clamp the `sweep_angle_` to a maximum of 359.5 since we can't paint an
    // arc from 360 back to -360.
    const float sweep_angle = std::clamp(sweep_angle_, 0.0f, 359.5f);
    path.arcTo(gfx::RectToSkRect(inner_circle_rect), sweep_angle, -sweep_angle,
               false);
    const auto right_center = local_bounds.right_center();
    path.lineTo(right_center.x(), right_center.y());
    path.arcTo(gfx::RectToSkRect(local_bounds), 0, sweep_angle, false);

    // Close the path if it's going to be used for masking (e.g. hit testing),
    // or if it's the single button in its container. When there are multiple
    // buttons next to each other, we only need to close one side of the slide
    // path, since the next button will paint its border on the unclosed side
    // of this button.
    if (for_masking || sweep_angle_ == 360.0f)
      path.close();

    // Rotate the slice to position it in its location inside the pie.
    gfx::Transform transform;
    transform.Rotate(-sweep_angle / 2.0f + button_index_ * sweep_angle);
    transform = gfx::TransformAboutPivot(
        gfx::PointF(local_bounds.CenterPoint()), transform);
    path.transform(gfx::TransformToFlattenedSkMatrix(transform));

    return path;
  }

  // The unique ID of this button among all buttons in the hosting pie menu.
  const int button_id_;

  // The text used as the label of this button.
  std::u16string button_label_text_;

  // The index of this button in the container that hosts it. The index will
  // determine the rotation of the button's slice path to position it in the
  // pie.
  int button_index_ = 0;

  // The angle that determines the size of the slice. The pie (the full 360
  // degrees) is divided equally into slices, making this value equal to
  // 360 / number of buttons in the pie sub menu container.
  float sweep_angle_ = 0.0f;

  // If not null, the icon that paints at the center of this button.
  const raw_ptr<const gfx::VectorIcon> icon_ = nullptr;

  // The cached image of the above `icon_` if any.
  gfx::ImageSkia icon_image_;

  // The sub menu container that this button opens when it gets pressed.
  raw_ptr<PieSubMenuContainerView> associated_sub_menu_container_ = nullptr;
};

BEGIN_METADATA(PieMenuButton)
END_METADATA

// -----------------------------------------------------------------------------
// PieSubMenuContainerView:

PieSubMenuContainerView::~PieSubMenuContainerView() = default;

views::View* PieSubMenuContainerView::AddMenuButton(
    int button_id,
    const std::u16string& button_label_text,
    const gfx::VectorIcon* icon) {
  PieMenuButton* button = AddChildView(
      std::make_unique<PieMenuButton>(button_id, button_label_text, icon));
  buttons_.push_back(button);

  const int buttons_count = buttons_.size();
  const float sweep_angle = 360.0f / buttons_count;
  for (int i = 0; i < buttons_count; ++i) {
    buttons_[i]->SetButtonIndexAndSweepAngle(i, sweep_angle);
  }

  owner_menu_view_->OnPieMenuButtonAdded(button);
  return button;
}

void PieSubMenuContainerView::RemoveAllButtons() {
  for (ash::PieMenuButton* button : buttons_) {
    owner_menu_view_->OnPieMenuButtonRemoved(button);
    RemoveChildViewT(button);
  }
  buttons_.clear();
}

PieSubMenuContainerView::PieSubMenuContainerView(PieMenuView* owner_menu_view)
    : owner_menu_view_(owner_menu_view) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
}

BEGIN_METADATA(PieSubMenuContainerView)
END_METADATA

// -----------------------------------------------------------------------------
// PieMenuView:

PieMenuView::PieMenuView(Delegate* delegate)
    : delegate_(delegate),
      main_menu_container_(
          AddChildView(base::WrapUnique(new PieSubMenuContainerView(this)))),
      back_button_(AddChildView(std::make_unique<views::ImageButton>(
          base::BindRepeating(&PieMenuView::MaybePopSubMenu,
                              base::Unretained(this))))) {
  DCHECK(delegate_);

  back_button_->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back_button_->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  // TODO(http://b/252558235): Localize all strings when ready.
  back_button_->SetTooltipText(u"Back");
  StyleUtil::SetUpInkDropForButton(back_button_, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/false);
  views::InstallCircleHighlightPathGenerator(back_button_);
  back_button_->SetVisible(false);
}

PieMenuView::~PieMenuView() = default;

PieSubMenuContainerView* PieMenuView::GetOrAddSubMenuForButton(int button_id) {
  auto* button = GetButtonById(button_id);
  CHECK(button);

  if (!button->associated_sub_menu_container()) {
    auto* sub_menu =
        AddChildView(base::WrapUnique(new PieSubMenuContainerView(this)));
    sub_menu->SetVisible(false);
    button->set_associated_sub_menu_container(sub_menu);
    // The back button should remain stacked on top, so it can get events before
    // the other views.
    ReorderChildView(back_button_, children().size() - 1);
  }
  return button->associated_sub_menu_container();
}

void PieMenuView::SetButtonLabelText(int button_id,
                                     const std::u16string& text) {
  auto* button = GetButtonById(button_id);
  CHECK(button);
  button->SetButtonLabelText(text);
}

void PieMenuView::ReturnToMainMenu() {
  while (!active_sub_menus_stack_.empty()) {
    MaybePopSubMenu();
  }
}

views::View* PieMenuView::GetButtonByIdAsView(int button_id) const {
  return GetButtonById(button_id);
}

gfx::Point PieMenuView::GetButtonContentsCenterInScreen(int button_id) const {
  if (auto* button = GetButtonById(button_id)) {
    return views::View::ConvertPointToScreen(button,
                                             button->GetButtonContentsCenter());
  }
  return gfx::Point();
}

void PieMenuView::Layout(PassKey) {
  // All child views except the back button (i.e. all
  // `PieSubMenuContainerView`s) should fill the entire bounds of this view. The
  // back button however should be centered.
  auto local_bounds = GetLocalBounds();
  for (views::View* child : children()) {
    if (child != back_button_)
      child->SetBoundsRect(local_bounds);
  }

  const int diameter = 2 * kBackButtonRadius;
  local_bounds.ClampToCenteredSize(gfx::Size(diameter, diameter));
  back_button_->SetBoundsRect(local_bounds);

  DCHECK_EQ(width(), height());
  auto* layer = GetWidget()->GetLayer();
  layer->SetRoundedCornerRadius(gfx::RoundedCornersF(height() / 2.f));
  layer->SetIsFastRoundedCorner(true);
}

void PieMenuView::AddedToWidget() {
  auto* layer = GetWidget()->GetLayer();
  layer->SetFillsBoundsOpaquely(false);
  layer->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
}

void PieMenuView::OnThemeChanged() {
  views::View::OnThemeChanged();

  auto* color_provider = GetColorProvider();
  back_button_->SetBackground(views::CreateRoundedRectBackground(
      color_provider->GetColor(kColorAshShieldAndBase60), kBackButtonRadius));
  const auto normal_color = color_provider->GetColor(kColorAshIconColorPrimary);
  back_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(kKsvBrowserBackIcon, normal_color));
  back_button_->SetImageModel(
      views::Button::STATE_DISABLED,
      ui::ImageModel::FromVectorIcon(
          kKsvBrowserBackIcon, ColorUtil::GetDisabledColor(normal_color)));
}

void PieMenuView::OnPieMenuButtonAdded(PieMenuButton* button) {
  button->SetCallback(base::BindRepeating(&PieMenuView::OnPieMenuButtonPressed,
                                          base::Unretained(this), button));
  const auto pair = buttons_by_id_.emplace(button->button_id(), button);
  DCHECK(pair.second) << "Cannot add a button with a duplicate ID";
}

void PieMenuView::OnPieMenuButtonRemoved(PieMenuButton* button) {
  const size_t removed = buttons_by_id_.erase(button->button_id());
  CHECK_EQ(removed, 1u);
}

void PieMenuView::OnPieMenuButtonPressed(PieMenuButton* button) {
  if (auto* sub_menu = button->associated_sub_menu_container())
    OpenSubMenu(sub_menu);

  delegate_->OnPieMenuButtonPressed(button->button_id());
}

void PieMenuView::OpenSubMenu(PieSubMenuContainerView* sub_menu) {
  DCHECK(sub_menu);
  main_menu_container_->SetVisible(false);
  if (!active_sub_menus_stack_.empty()) {
    auto* top_sub_menu = active_sub_menus_stack_.top().get();
    top_sub_menu->SetVisible(false);
  }
  active_sub_menus_stack_.push(sub_menu);
  sub_menu->SetVisible(true);
  back_button_->SetVisible(true);
}

void PieMenuView::MaybePopSubMenu() {
  if (!active_sub_menus_stack_.empty()) {
    auto* top_sub_menu = active_sub_menus_stack_.top().get();
    top_sub_menu->SetVisible(false);
    active_sub_menus_stack_.pop();
  }

  if (active_sub_menus_stack_.empty()) {
    main_menu_container_->SetVisible(true);
    back_button_->SetVisible(false);
  } else {
    active_sub_menus_stack_.top()->SetVisible(true);
  }
}

PieMenuButton* PieMenuView::GetButtonById(int button_id) const {
  auto iter = buttons_by_id_.find(button_id);
  return iter == buttons_by_id_.end() ? nullptr : iter->second;
}

BEGIN_METADATA(PieMenuView)
END_METADATA

}  // namespace ash
