// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/coral_chip_button.h"

#include "ash/birch/birch_coral_item.h"
#include "ash/birch/birch_coral_provider.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/wm/desks/templates/saved_desk_presenter.h"
#include "ash/wm/overview/birch/birch_animation_utils.h"
#include "ash/wm/overview/birch/birch_bar_util.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/birch/resources/grit/coral_resources.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "ash/wm/overview/overview_session.h"
#include "base/strings/utf_string_conversions.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

constexpr gfx::Size kLoadingAnimationSize = gfx::Size(100, 20);
constexpr char kMaxSavedGroupsToastId[] = "coral_max_saved_groups_toast";
constexpr base::TimeDelta kBorderAnimationDuration = base::Seconds(3);

}  // namespace

CoralChipButton::CoralChipButton() = default;

CoralChipButton::~CoralChipButton() = default;

void CoralChipButton::OnSelectionWidgetVisibilityChanged() {
  CHECK(tab_app_selection_widget_);
  UpdateRoundedCorners(tab_app_selection_widget_->IsVisible());
  UpdateAddonButtonTooltip();
}

void CoralChipButton::ShutdownSelectionWidget() {
  tab_app_selection_widget_.reset();
}

void CoralChipButton::ReloadIcon() {
  item_->LoadIcon(base::BindOnce(&CoralChipButton::SetIconImage,
                                 weak_factory_.GetWeakPtr()));
}

void CoralChipButton::UpdateTitle(
    const std::optional<std::string>& group_title) {
  views::Label* title_label = title();
  if (group_title) {
    // If the title is not empty, reset the `title_` with the real title.
    if (!group_title->empty()) {
      title_label->SetText(base::UTF8ToUTF16(*group_title));
    }
    // Show title and delete the loading animation.
    title_label->SetVisible(true);
    if (title_loading_animated_image_) {
      title_loading_animated_image_->Stop();
      title_loading_animated_image_->parent()->RemoveChildViewT(
          std::exchange(title_loading_animated_image_, nullptr));
    }
  } else {
    // If the title is null, show the animation to wait for title loading.
    title_label->SetVisible(false);

    BuildTitleLoadingAnimation();
  }

  SetAccessibleName(item_->GetAccessibleName());

  if (chevron_button_) {
    UpdateAddonButtonTooltip();
  }
}

void CoralChipButton::Init(BirchItem* item) {
  CHECK_EQ(item->GetType(), BirchItemType::kCoral);

  BirchChipButton::Init(item);

  BuildBorderAnimation();

  // Override the title, callback and addon. Gets the real title from the group.
  auto* coral_provider = BirchCoralProvider::Get();
  const std::optional<std::string>& group_title =
      coral_provider
          ? coral_provider
                ->GetGroupById(static_cast<BirchCoralItem*>(item_)->group_id())
                ->title
          : std::string();
  UpdateTitle(group_title);

  SetCallback(
      base::BindRepeating(&BirchCoralItem::LaunchGroup,
                          base::Unretained(static_cast<BirchCoralItem*>(item_)),
                          base::Unretained(this)));

  base::RepeatingClosure callback = base::BindRepeating(
      &CoralChipButton::OnCoralAddonClicked, weak_factory_.GetWeakPtr());

  auto button = birch_bar_util::CreateCoralAddonButton(
      std::move(callback), vector_icons::kCaretUpIcon);
  button->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_HIDDEN, item_->title()));
  chevron_button_ = button.get();
  SetAddon(std::move(button));
  UpdateAddonButtonTooltip();
}

void CoralChipButton::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case base::to_underlying(
        BirchChipContextMenuModel::CommandId::kCoralNewDesk):
      static_cast<BirchCoralItem*>(item_)->LaunchGroup(this);
      break;
    case base::to_underlying(
        BirchChipContextMenuModel::CommandId::kCoralSaveForLater): {
      // Show a toast if we already have the max amount of allowed coral saved
      // groups.
      auto* saved_desk_presenter =
          OverviewController::Get()->overview_session()->saved_desk_presenter();
      if (saved_desk_presenter->GetEntryCount(DeskTemplateType::kCoral) >=
          saved_desk_presenter->GetMaxEntryCount(DeskTemplateType::kCoral)) {
        ToastData toast(kMaxSavedGroupsToastId,
                        ToastCatalogName::kCoralSavedGroupLimitMax,
                        l10n_util::GetStringUTF16(
                            IDS_ASH_BIRCH_CORAL_SAVED_GROUPS_MAX_NUM_REACHED),
                        ToastData::kDefaultToastDuration,
                        /*visible_on_lock_screen=*/false);
        Shell::Get()->toast_manager()->Show(std::move(toast));
        return;
      }

      // `CreateSavedDeskFromGroup()` will delete `this`.
      aura::Window* root_window = GetWidget()->GetNativeWindow();

      auto* coral_provider = BirchCoralProvider::Get();
      Shell::Get()->coral_controller()->CreateSavedDeskFromGroup(
          base::UTF16ToUTF8(title()->GetText()),
          coral_provider->ExtractGroupById(
              static_cast<BirchCoralItem*>(item_)->group_id()),
          root_window);
      break;
    }
    default:
      BirchChipButton::ExecuteCommand(command_id, event_flags);
  }
}

void CoralChipButton::OnCoralAddonClicked() {
  if (!tab_app_selection_widget_) {
    tab_app_selection_widget_ = std::make_unique<TabAppSelectionHost>(this);
    tab_app_selection_widget_->Show();
    return;
  }

  if (!tab_app_selection_widget_->IsVisible()) {
    tab_app_selection_widget_->Show();
  } else {
    tab_app_selection_widget_->SlideOut();
  }
}

void CoralChipButton::BuildTitleLoadingAnimation() {
  // Build `title_loading_animated_image_` and insert into the
  // front of `titles_container_`.
  std::unique_ptr<views::AnimatedImageView> title_loading_animated_image =
      views::Builder<views::AnimatedImageView>()
          .SetAnimatedImage(birch_animation_utils::GetLottieAnimationData(
              IDR_CORAL_LOADING_TITLE_ANIMATION))
          .SetImageSize(kLoadingAnimationSize)
          .SetVisible(true)
          .SetHorizontalAlignment(views::ImageViewBase::Alignment::kLeading)
          .Build();
  title_loading_animated_image_ =
      title()->parent()->AddChildViewAt(std::move(title_loading_animated_image),
                                        /*index=*/0);
  title_loading_animated_image_->Play(
      birch_animation_utils::GetLottiePlaybackConfig(
          *title_loading_animated_image_->animated_image()->skottie()));
}

void CoralChipButton::BuildBorderAnimation() {
  // Build `rainbow_border_animated_image_` and cover it on the chip.
  std::unique_ptr<views::AnimatedImageView> rainbow_border_animated_image =
      views::Builder<views::AnimatedImageView>()
          .SetAnimatedImage(birch_animation_utils::GetLottieAnimationData(
              IDR_CORAL_BORDER_ANIMATION))
          .SetProperty(views::kViewIgnoredByLayoutKey, true)
          .SetCanProcessEventsWithinSubtree(false)
          .Build();
  rainbow_border_animated_image_ =
      AddChildView(std::move(rainbow_border_animated_image));

  // Transform the animated image to fit the chip.
  const gfx::SizeF chip_size(GetPreferredSize());
  const gfx::SizeF image_size(
      rainbow_border_animated_image_->GetPreferredSize());
  rainbow_border_animated_image_->SetTransform(
      gfx::Transform::MakeScale(chip_size.width() / image_size.width(),
                                chip_size.height() / image_size.height()));
  rainbow_border_animated_image_->layer()->SetFillsBoundsOpaquely(false);

  // Clipping the image outside chip region.
  rainbow_border_animated_image_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(GetRoundedCornerRadius()));

  rainbow_border_animated_image_->SizeToPreferredSize();
  rainbow_border_animated_image_->Play(
      birch_animation_utils::GetLottiePlaybackConfig(
          *rainbow_border_animated_image_->animated_image()->skottie()));

  stop_border_animation_timer_.Start(FROM_HERE, kBorderAnimationDuration, this,
                                     &CoralChipButton::DestroyBorderAnimation);
}

void CoralChipButton::DestroyBorderAnimation() {
  if (rainbow_border_animated_image_) {
    stop_border_animation_timer_.Stop();
    RemoveChildViewT(std::exchange(rainbow_border_animated_image_, nullptr));
  }
}

void CoralChipButton::UpdateAddonButtonTooltip() {
  CHECK(chevron_button_);
  chevron_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      tab_app_selection_widget_ && tab_app_selection_widget_->IsVisible()
          ? IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_SHOWN
          : IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_HIDDEN,
      item_->title()));
}

BEGIN_METADATA(CoralChipButton)
END_METADATA

}  // namespace ash
