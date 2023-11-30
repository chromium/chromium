// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/glanceables/glanceables_bar_view.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/icon_button.h"
#include "ash/wm/work_area_insets.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kMaxChipsNum = 4;
constexpr gfx::Insets kHideButtonMargin = gfx::Insets::VH(12, 5);
constexpr int kChipSpacing = 8;
constexpr int kBarHeight = 60;
constexpr int kBarBottomPadding = 10;
constexpr base::TimeDelta kShowHideAnimationDuration = base::Milliseconds(500);

// ----- For test use -----------
std::unique_ptr<views::Widget> g_widget_for_testing;

}  // namespace

//------------------------------------------------------------------------------
// GlanceablesBarView::GlanceablesChipsContainer
// The chips container with glanceables chips and hiding chips button.
class GlanceablesBarView::GlanceablesChipsContainer
    : public views::BoxLayoutView {
  METADATA_HEADER(GlanceablesChipsContainer, views::BoxLayoutView)

 public:
  explicit GlanceablesChipsContainer(GlanceablesBarView* glanceable_bar)
      : glanceable_bar_(glanceable_bar) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);
    SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetBetweenChildSpacing(kChipSpacing);
    hide_chips_button_ = AddChildView(std::make_unique<IconButton>(
        base::BindRepeating(&GlanceablesBarView::OnShowHideChipsButtonPressed,
                            base::Unretained(glanceable_bar_), /*show=*/false),
        IconButton::Type::kMedium, &kChevronDownIcon, u"Hide", false, false));
    hide_chips_button_->SetProperty(views::kMarginsKey, kHideButtonMargin);
    hide_chips_button_->SetEnableBlurredBackgroundShield(true);
  }

  GlanceablesChipsContainer(const GlanceablesChipsContainer&) = delete;
  GlanceablesChipsContainer& operator=(const GlanceablesChipsContainer&) =
      delete;
  ~GlanceablesChipsContainer() override = default;

  void AddChip(std::unique_ptr<GlanceablesChipButton> chip) {
    if (static_cast<int>(chips_.size()) == kMaxChipsNum) {
      NOTREACHED() << "The number of glanceable chips reaches the limit of 4";
      return;
    }
    const size_t child_num = children().size();
    CHECK_GE(child_num, 1u);
    // Insert the chip before `hide_chips_button_`.
    chips_.push_back(AddChildViewAt(std::move(chip), child_num - 1));
  }

  void RemoveChip(GlanceablesChipButton* chip) {
    auto iter = std::find(chips_.begin(), chips_.end(), chip);
    if (iter != chips_.end()) {
      RemoveChildViewT(chip);
      chips_.erase(iter);
      Layout();
    }
  }

 private:
  raw_ptr<GlanceablesBarView> glanceable_bar_;
  std::vector<raw_ptr<GlanceablesChipButton>> chips_;
  raw_ptr<IconButton> hide_chips_button_;
};

BEGIN_METADATA(GlanceablesBarView,
               GlanceablesChipsContainer,
               views::BoxLayoutView)
END_METADATA

//------------------------------------------------------------------------------
// GlanceablesBarView
GlanceablesBarView::GlanceablesBarView() {
  chips_container_ =
      AddChildView(std::make_unique<GlanceablesChipsContainer>(this));

  show_chips_button_container_ = AddChildView(std::make_unique<views::View>());
  show_chips_button_container_->SetPaintToLayer();
  show_chips_button_container_->layer()->SetFillsBoundsOpaquely(false);
  show_chips_button_container_->SetUseDefaultFillLayout(true);

  auto* show_chips_button =
      show_chips_button_container_->AddChildView(std::make_unique<IconButton>(
          base::BindRepeating(&GlanceablesBarView::OnShowHideChipsButtonPressed,
                              base::Unretained(this), /*show=*/true),
          IconButton::Type::kMedium, &kChevronUpIcon, u"Show", false, false));
  show_chips_button->SetEnableBlurredBackgroundShield(true);

  chips_container_->SetVisible(false);
}

GlanceablesBarView::~GlanceablesBarView() = default;

void GlanceablesBarView::ShowWidgetForTesting(
    std::unique_ptr<GlanceablesBarView> bar_view) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_POPUP;
  params.layer_type = ui::LAYER_NOT_DRAWN;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  auto* root_window = Shell::Get()->GetPrimaryRootWindow();
  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(root_window)->user_work_area_bounds();
  const gfx::Size bar_size = bar_view->GetPreferredSize();
  params.bounds =
      gfx::Rect(gfx::Point(work_area.bottom_center().x() - bar_size.width() / 2,
                           work_area.bottom_center().y() - bar_size.height() -
                               kBarBottomPadding),
                bar_size);
  params.parent = Shell::Get()->GetContainer(
      root_window, kShellWindowId_AlwaysOnTopContainer);

  g_widget_for_testing = std::make_unique<views::Widget>(std::move(params));
  g_widget_for_testing->SetContentsView(std::move(bar_view));
  g_widget_for_testing->Show();
}

void GlanceablesBarView::HideWidgetForTesting() {
  if (g_widget_for_testing) {
    g_widget_for_testing->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    g_widget_for_testing.reset();
  }
}

void GlanceablesBarView::AddChip(
    const ui::ImageModel& icon,
    const std::u16string& title,
    const std::u16string& sub_title,
    views::Button::PressedCallback callback,
    std::optional<std::u16string> button_title,
    std::optional<views::Button::PressedCallback> button_callback) {
  auto chip = views::Builder<GlanceablesChipButton>()
                  .SetIconImage(icon)
                  .SetTitleText(title)
                  .SetSubtitleText(sub_title)
                  .SetCallback(std::move(callback))
                  .SetDelegate(this)
                  .Build();
  if (button_title.has_value() && button_callback.has_value()) {
    chip->SetActionButton(button_title.value(),
                          std::move(button_callback.value()));
  }
  chips_container_->AddChip(std::move(chip));
}

gfx::Size GlanceablesBarView::CalculatePreferredSize() const {
  gfx::Size preferred_size;
  for (views::View* content : children()) {
    preferred_size.SetToMax(content->GetPreferredSize());
  }
  return gfx::Size(preferred_size.width(), kBarHeight);
}

int GlanceablesBarView::GetHeightForWidth(int width) const {
  return kBarHeight;
}

void GlanceablesBarView::Layout() {
  // Centralize the chips container/show button.
  const gfx::Point center_point = GetContentsBounds().CenterPoint();
  for (views::View* content : children()) {
    content->SizeToPreferredSize();
    content->SetPosition(gfx::Point(center_point.x() - content->width() / 2,
                                    center_point.y() - content->height() / 2));
  }
}

void GlanceablesBarView::RemoveChip(GlanceablesChipButton* chip) {
  chips_container_->RemoveChip(chip);
}

void GlanceablesBarView::OnAnimationsEnded(bool show) {
  // Update contents visibility and opacity on animation completed or aborted.
  animation_in_progress_ = false;
  if (show) {
    show_chips_button_container_->SetVisible(false);
  } else {
    chips_container_->SetVisible(false);
  }
}

void GlanceablesBarView::OnShowHideChipsButtonPressed(bool show) {
  if (animation_in_progress_) {
    return;
  }

  animation_in_progress_ = true;

  // Fade in/out and expand/shrink the showing button.
  show_chips_button_container_->SetVisible(true);
  auto* show_chips_button_layer = show_chips_button_container_->layer();
  show_chips_button_layer->SetOpacity(show ? 1.0f : 0.0f);
  gfx::Transform shrink = gfx::GetScaleTransform(
      show_chips_button_container_->GetContentsBounds().CenterPoint(), 0.3f);
  show_chips_button_layer->SetTransform(show ? gfx::Transform() : shrink);

  // Fade in/out and rise/fall the chips container.
  chips_container_->SetVisible(true);
  auto* chips_container_layer = chips_container_->layer();
  chips_container_layer->SetOpacity(show ? 0.0f : 1.0f);
  const gfx::Transform vertical_shift =
      gfx::Transform::MakeTranslation(0, kBarHeight);
  chips_container_->SetTransform(show ? vertical_shift : gfx::Transform());

  // Setup animations.
  auto animation_complete_callback = base::BindRepeating(
      &GlanceablesBarView::OnAnimationsEnded, base::Unretained(this), show);
  views::AnimationBuilder animation_builder;
  animation_builder.OnEnded(base::OnceClosure(animation_complete_callback))
      .OnAborted(base::OnceClosure(animation_complete_callback))
      .Once()
      .SetDuration(kShowHideAnimationDuration)
      .SetOpacity(show_chips_button_layer, show ? 0.0f : 1.0f)
      .SetTransform(show_chips_button_layer, show ? shrink : gfx::Transform())
      .SetOpacity(chips_container_layer, show ? 1.0f : 0.0f)
      .SetTransform(chips_container_layer,
                    show ? gfx::Transform() : vertical_shift);
}

BEGIN_METADATA(GlanceablesBarView)
END_METADATA

}  // namespace ash
