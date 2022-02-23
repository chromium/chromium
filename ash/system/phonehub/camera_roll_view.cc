// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_view.h"

#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/animated_loading_card.h"
#include "ash/system/phonehub/camera_roll_thumbnail.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/phonehub/ui_constants.h"
#include "ash/system/tray/tray_constants.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance constants in dip.
constexpr int kCameraRollItemsInRow = 4;
constexpr int kCameraRollItemHorizontalSpacing = 8;
constexpr int kCameraRollItemVerticalSpacing = 8;
constexpr int kCameraRollItemHorizontalPadding = 4;
constexpr int kCameraRollItemVerticalPadding = 4;
constexpr int kHeaderLabelLineHeight = 48;

// Animation constants for loading card
constexpr float kAnimationLoadingCardCorner = 12.0f;
constexpr float kAnimationLoadingCardOpacity = 0.15f;
constexpr int kAnimationLoadingCardDelayInMs = 83;
constexpr int kAnimationLoadingCardTransitDurationInMs = 200;
constexpr int kAnimationLoadingCardFreezeDurationInMs = 150;

// Typography.
constexpr int kHeaderTextFontSizeDip = 15;

gfx::Size GetCameraRollItemSize() {
  int dimension =
      (kTrayMenuWidth - kBubbleHorizontalSidePaddingDip * 2 -
       kCameraRollItemHorizontalPadding * 2 -
       kCameraRollItemHorizontalSpacing * (kCameraRollItemsInRow - 1)) /
      kCameraRollItemsInRow;
  return gfx::Size(dimension, dimension);
}

class HeaderView : public views::Label {
 public:
  HeaderView() {
    SetText(l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_CAMERA_ROLL_TITLE));
    SetLineHeight(kHeaderLabelLineHeight);
    SetFontList(font_list()
                    .DeriveWithSizeDelta(kHeaderTextFontSizeDip -
                                         font_list().GetFontSize())
                    .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    SetVerticalAlignment(gfx::VerticalAlignment::ALIGN_MIDDLE);
    SetAutoColorReadabilityEnabled(false);
    SetSubpixelRenderingEnabled(false);
    SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }

  ~HeaderView() override = default;
  HeaderView(HeaderView&) = delete;
  HeaderView operator=(HeaderView&) = delete;

  // views::View:
  const char* GetClassName() const override { return "HeaderView"; }
};

}  // namespace

CameraRollView::CameraRollView(
    phonehub::CameraRollManager* camera_roll_manager,
    phonehub::UserActionRecorder* user_action_recorder)
    : camera_roll_manager_(camera_roll_manager),
      user_action_recorder_(user_action_recorder) {
  SetID(PhoneHubViewID::kCameraRollView);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  AddChildView(std::make_unique<HeaderView>());
  items_view_ = AddChildView(std::make_unique<CameraRollItemsView>());

  opt_in_view_ =
      AddChildView(std::make_unique<CameraRollOptInView>(camera_roll_manager_));

  Update();
  camera_roll_manager_->AddObserver(this);
}

CameraRollView::~CameraRollView() {
  camera_roll_manager_->RemoveObserver(this);
}

void CameraRollView::OnCameraRollViewUiStateUpdated() {
  Update();
}

const char* CameraRollView::GetClassName() const {
  return "CameraRollView";
}

CameraRollView::CameraRollItemsView::CameraRollItemsView() = default;

CameraRollView::CameraRollItemsView::~CameraRollItemsView() = default;

void CameraRollView::CameraRollItemsView::AddCameraRollItem(
    views::View* camera_roll_item) {
  int view_size = camera_roll_items_.view_size();
  camera_roll_items_.Add(camera_roll_item, view_size);
  AddChildView(camera_roll_item);
}

void CameraRollView::CameraRollItemsView::AddLoadingAnimatedItem(
    bool disable_repeated_animation_for_test) {
  gfx::RoundedCornersF rounded_corners(
      kAnimationLoadingCardCorner, kAnimationLoadingCardCorner,
      kAnimationLoadingCardCorner, kAnimationLoadingCardCorner);
  views::AnimationBuilder animation_builder;
  // create 4 annimated loading cards
  for (size_t index = 0; index < kCameraRollItemsInRow; index++) {
    AnimatedLoadingCard* loading_card = new AnimatedLoadingCard();
    camera_roll_items_.Add(loading_card, index);
    animation_builder.Once()
        .SetRoundedCorners(loading_card, rounded_corners)
        .SetOpacity(loading_card,
                    kAnimationLoadingCardOpacity * (index % 4 + 1));
    if (!disable_repeated_animation_for_test) {
      // In order to test a view with repeated animation, we need to do
      // loading_card->layer()->GetAnimator()->set_disable_timer_for_test(true)
      // before animation_builder tourches the view and manually step through
      // the timer, otherwise test will time out. Cosidering view unitest is
      // focusing on verify view state,disable repeated animation for tests.
      animation_builder.Repeatedly()
          .Offset(base::Milliseconds(kAnimationLoadingCardDelayInMs * index))
          .SetDuration(
              base::Milliseconds(kAnimationLoadingCardTransitDurationInMs))
          .SetOpacity(loading_card, 1.0f, gfx::Tween::LINEAR)
          .Then()
          .Offset(base::Milliseconds(kAnimationLoadingCardFreezeDurationInMs))
          .Then()
          .SetDuration(
              base::Milliseconds(kAnimationLoadingCardTransitDurationInMs))
          .SetOpacity(loading_card, kAnimationLoadingCardOpacity,
                      gfx::Tween::LINEAR);
    }
    AddChildView(loading_card);
  }
}

void CameraRollView::CameraRollItemsView::Reset() {
  camera_roll_items_.Clear();
  RemoveAllChildViews();
}

// views::View:
gfx::Size CameraRollView::CameraRollItemsView::CalculatePreferredSize() const {
  auto item_size = GetCameraRollItemSize();
  int width = item_size.width() * kCameraRollItemsInRow +
              kCameraRollItemHorizontalSpacing * (kCameraRollItemsInRow - 1) +
              kCameraRollItemHorizontalPadding * 2;
  int rows_num =
      std::ceil((double)camera_roll_items_.view_size() / kCameraRollItemsInRow);
  int height = (item_size.height() + kCameraRollItemVerticalSpacing) *
                   std::max(0, rows_num - 1) +
               item_size.height() + kCameraRollItemVerticalPadding * 2;
  return gfx::Size(width, height);
}

void CameraRollView::CameraRollItemsView::Layout() {
  views::View::Layout();
  CalculateIdealBounds();
  for (int i = 0; i < camera_roll_items_.view_size(); ++i) {
    auto* thumbnail = camera_roll_items_.view_at(i);
    thumbnail->SetBoundsRect(camera_roll_items_.ideal_bounds(i));
  }
}

const char* CameraRollView::CameraRollItemsView::GetClassName() const {
  return "CameraRollItemsView";
}

gfx::Point CameraRollView::CameraRollItemsView::GetCameraRollItemPosition(
    int index) {
  auto item_size = GetCameraRollItemSize();
  int row = index / kCameraRollItemsInRow;
  int column = index % kCameraRollItemsInRow;
  int x = (item_size.width() + kCameraRollItemHorizontalSpacing) * column +
          kCameraRollItemHorizontalPadding;
  int y = (item_size.height() + kCameraRollItemVerticalSpacing) * row +
          kCameraRollItemVerticalPadding;
  return gfx::Point(x, y);
}

void CameraRollView::CameraRollItemsView::CalculateIdealBounds() {
  for (int i = 0; i < camera_roll_items_.view_size(); ++i) {
    gfx::Rect camera_roll_item_bounds =
        gfx::Rect(GetCameraRollItemPosition(i), GetCameraRollItemSize());
    camera_roll_items_.set_ideal_bounds(i, camera_roll_item_bounds);
  }
}

void CameraRollView::Update() {
  items_view_->Reset();
  phonehub::CameraRollManager::CameraRollUiState current_ui_state =
      camera_roll_manager_->ui_state();

  switch (current_ui_state) {
    case phonehub::CameraRollManager::CameraRollUiState::SHOULD_HIDE:
    case phonehub::CameraRollManager::CameraRollUiState::NO_STORAGE_PERMISSION:
      SetVisible(false);
      break;
    case phonehub::CameraRollManager::CameraRollUiState::CAN_OPT_IN:
      opt_in_view_->SetVisible(true);
      items_view_->SetVisible(false);
      SetVisible(true);
      LogCameraRollOptInEvent(
          phone_hub_metrics::InterstitialScreenEvent::kShown);
      break;
    case phonehub::CameraRollManager::CameraRollUiState::LOADING_VIEW:
      opt_in_view_->SetVisible(false);
      items_view_->SetVisible(true);
      SetVisible(true);
      items_view_->AddLoadingAnimatedItem(
          should_disable_annimator_timer_for_test_);
      break;
    case phonehub::CameraRollManager::CameraRollUiState::ITEMS_VISIBLE:
      opt_in_view_->SetVisible(false);
      items_view_->SetVisible(true);
      SetVisible(true);
      const std::vector<phonehub::CameraRollItem> camera_roll_items =
          camera_roll_manager_->current_items();
      for (size_t index = 0; index < camera_roll_items.size(); index++) {
        CameraRollThumbnail* item_thumbnail = new CameraRollThumbnail(
            index, camera_roll_items.at(index), camera_roll_manager_,
            user_action_recorder_);

        const std::u16string accessible_name = l10n_util::GetStringFUTF16(
            IDS_ASH_PHONE_HUB_CAMERA_ROLL_THUMBNAIL_ACCESSIBLE_NAME,
            base::NumberToString16(index + 1),
            base::NumberToString16(camera_roll_items.size()));
        item_thumbnail->SetAccessibleName(accessible_name);
        item_thumbnail->SetTooltipText(accessible_name);
        items_view_->AddCameraRollItem(item_thumbnail);
      }
      break;
  }

  PreferredSizeChanged();
}

}  // namespace ash
