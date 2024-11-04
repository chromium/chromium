// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button.h"

#include "ash/birch/birch_coral_provider.h"
#include "ash/birch/birch_item.h"
#include "ash/public/cpp/coral_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/wm/overview/birch/birch_animation_utils.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_util.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/birch/resources/grit/coral_resources.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// The color and layout parameters of the chip.
constexpr gfx::Insets kInteriorMarginsNoAddon = gfx::Insets::TLBR(12, 0, 8, 20);
constexpr gfx::Insets kInteriorMarginsWithAddon = gfx::Insets::VH(12, 0);

// The layout parameters of icon.
constexpr gfx::Insets kIconMargins = gfx::Insets::TLBR(0, 12, 0, 8);
constexpr int kParentIconViewSize = 44;
constexpr int kPrimaryIconViewSize = 40;
constexpr int kSecondaryIconViewSize = 20;
constexpr int kSecondaryIconImageSize = 12;
constexpr gfx::Point kSecondaryIconOffset(24, 24);
constexpr int kIconSize = 16;
constexpr int kIconCornerRadius = 20;
constexpr int kIllustrationSize = 40;
constexpr int kIllustrationCornerRadius = 8;
constexpr int kCoralGroupedImageSize = 40;
constexpr int kCoralIconBackgroundCornerRadius = 20;
constexpr int kWeatherImageSize = 32;

// The colors of icons.
constexpr ui::ColorId kIconBackgroundColorId =
    cros_tokens::kCrosSysSystemOnBase;
constexpr ui::ColorId kSecondaryIconBackgroundColorId =
    cros_tokens::kCrosSysSecondaryLight;
constexpr ui::ColorId kSecondaryIconColorId = cros_tokens::kCrosSysOnSecondary;

// The colors and fonts of title and subtitle.
constexpr int kTitleSpacing = 2;
constexpr TypographyToken kTitleFont = TypographyToken::kCrosButton1;
constexpr ui::ColorId kTitleColorId = cros_tokens::kCrosSysOnSurface;
constexpr TypographyToken kSubtitleFont = TypographyToken::kCrosAnnotation1;
constexpr ui::ColorId kSubtitleColorId = cros_tokens::kCrosSysOnSurfaceVariant;

constexpr gfx::Size kLoadingAnimationSize = gfx::Size(100, 20);

BirchSuggestionType GetSuggestionTypeFromItemType(BirchItemType item_type) {
  switch (item_type) {
    case BirchItemType::kWeather:
      return BirchSuggestionType::kWeather;
    case BirchItemType::kCalendar:
      return BirchSuggestionType::kCalendar;
    // Attachments are considered Drive suggestions in the UI.
    case BirchItemType::kAttachment:
    case BirchItemType::kFile:
      return BirchSuggestionType::kDrive;
    // All tab types are "Chrome browser" in the UI.
    case BirchItemType::kTab:
    case BirchItemType::kLastActive:
    case BirchItemType::kMostVisited:
    case BirchItemType::kSelfShare:
      return BirchSuggestionType::kChromeTab;
    case BirchItemType::kLostMedia:
      return BirchSuggestionType::kMedia;
    case BirchItemType::kReleaseNotes:
      return BirchSuggestionType::kExplore;
    case BirchItemType::kCoral:
      return BirchSuggestionType::kCoral;
    default:
      return BirchSuggestionType::kUndefined;
  }
}

std::unique_ptr<views::ImageView> CreatePrimaryImageView(
    const ui::ImageModel& icon,
    PrimaryIconType type) {
  int icon_size;
  std::optional<int> rounded_corners;
  switch (type) {
    case PrimaryIconType::kIcon:
      icon_size = kIconSize;
      rounded_corners = kIconCornerRadius;
      break;
    case PrimaryIconType::kIllustration:
      icon_size = kIllustrationSize;
      rounded_corners = kIllustrationCornerRadius;
      break;
    case PrimaryIconType::kWeatherImage:
      icon_size = kWeatherImageSize;
      break;
    case PrimaryIconType::kCoralGroupIcon:
      icon_size = kCoralGroupedImageSize;
      rounded_corners = kCoralIconBackgroundCornerRadius;
      break;
  }

  return views::Builder<views::ImageView>()
      .SetImage(icon)
      .SetImageSize(gfx::Size(icon_size, icon_size))
      .SetSize(gfx::Size(kPrimaryIconViewSize, kPrimaryIconViewSize))
      .SetBorder(views::CreateEmptyBorder(
          gfx::Insets((kPrimaryIconViewSize - icon_size) / 2)))
      .SetBackground(rounded_corners
                         ? views::CreateThemedRoundedRectBackground(
                               kIconBackgroundColorId, rounded_corners.value())
                         : nullptr)
      .Build();
}

std::unique_ptr<views::ImageView> CreateSecondaryImageView(
    SecondaryIconType type) {
  ui::ImageModel icon_image;
  switch (type) {
    case SecondaryIconType::kTabFromDesktop:
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconDesktopIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kTabFromPhone:
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconPortraitIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kTabFromTablet:
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconLandscapeIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kTabFromUnknown:
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconUnknownIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kLostMediaAudio:
      icon_image = ui::ImageModel::FromVectorIcon(kBirchSecondaryIconAudioIcon,
                                                  kSecondaryIconColorId);
      break;
    case SecondaryIconType::kLostMediaVideo:
      icon_image = ui::ImageModel::FromVectorIcon(kBirchSecondaryIconVideoIcon,
                                                  kSecondaryIconColorId);
      break;
    case SecondaryIconType::kLostMediaVideoConference:
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconVideoConferenceIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kSelfShareIcon:
      // TODO(https://b/364912772): Remove temporary fix by adding sender's
      // device form_factor to `SelfTabToSelfEntry`.
      icon_image = ui::ImageModel::FromVectorIcon(
          kBirchSecondaryIconGenericShareIcon, kSecondaryIconColorId);
      break;
    case SecondaryIconType::kNoIcon:
      NOTREACHED();
  }
  return views::Builder<views::ImageView>()
      .SetImage(icon_image)
      .SetImageSize(gfx::Size(kSecondaryIconImageSize, kSecondaryIconImageSize))
      .SetPosition(kSecondaryIconOffset)
      .SetSize(gfx::Size(kSecondaryIconViewSize, kSecondaryIconViewSize))
      .SetBackground(views::CreateThemedRoundedRectBackground(
          kSecondaryIconBackgroundColorId, kSecondaryIconViewSize / 2))
      .SetBorder(views::CreateThemedRoundedRectBorder(
          1, kSecondaryIconViewSize / 2,
          cros_tokens::kCrosSysSystemOnBaseOpaque))
      .Build();
}

}  // namespace

//------------------------------------------------------------------------------
// BirchChipButton::ChipMenuController:
class BirchChipButton::ChipMenuController
    : public views::ContextMenuController {
 public:
  explicit ChipMenuController(BirchChipButton* chip) : chip_(chip) {}
  ChipMenuController(const ChipMenuController&) = delete;
  ChipMenuController& operator=(const ChipMenuController&) = delete;
  ~ChipMenuController() override = default;

 private:
  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& point,
      ui::mojom::MenuSourceType source_type) override {
    if (auto* birch_bar_controller_ = BirchBarController::Get()) {
      birch_bar_controller_->ShowChipContextMenu(
          chip_, GetSuggestionTypeFromItemType(chip_->GetItem()->GetType()),
          point, source_type);
    }
  }

  const raw_ptr<BirchChipButton> chip_;
};

//------------------------------------------------------------------------------
// BirchChipButton:
BirchChipButton::BirchChipButton()
    : chip_menu_controller_(std::make_unique<ChipMenuController>(this)) {
  auto flex_layout = std::make_unique<views::FlexLayout>();
  flex_layout_ = flex_layout.get();
  flex_layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMarginsNoAddon);

  // Build up the chip's contents.
  views::Builder<BirchChipButtonBase>(this)
      .SetLayoutManager(std::move(flex_layout))
      .AddChildren(
          // Icon parent.
          views::Builder<views::View>()
              .CopyAddressTo(&icon_parent_view_)
              .SetPreferredSize(
                  gfx::Size(kParentIconViewSize, kParentIconViewSize))
              .SetProperty(views::kMarginsKey, kIconMargins),
          views::Builder<views::BoxLayoutView>()
              .CopyAddressTo(&titles_container_)
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
              .SetOrientation(views::BoxLayout::Orientation::kVertical)
              .SetBetweenChildSpacing(kTitleSpacing)
              .AddChildren(views::Builder<views::Label>()
                               .CopyAddressTo(&title_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kTitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT),
                           views::Builder<views::Label>()
                               .CopyAddressTo(&subtitle_)
                               .SetAutoColorReadabilityEnabled(false)
                               .SetEnabledColorId(kSubtitleColorId)
                               .SetHorizontalAlignment(gfx::ALIGN_LEFT)))
      .BuildChildren();

  // Stylize the titles.
  auto* typography_provider = TypographyProvider::Get();
  typography_provider->StyleLabel(kTitleFont, *title_);
  typography_provider->StyleLabel(kSubtitleFont, *subtitle_);

  // Add removal chip panel.
  set_context_menu_controller(chip_menu_controller_.get());
}

BirchChipButton::~BirchChipButton() = default;

void BirchChipButton::OnSelectionWidgetVisibilityChanged() {
  CHECK(tab_app_selection_widget_);
  UpdateRoundedCorners(tab_app_selection_widget_->IsVisible());

  CHECK(addon_view_);
  views::AsViewClass<IconButton>(addon_view_)
      ->SetTooltipText(l10n_util::GetStringUTF16(
          tab_app_selection_widget_->IsVisible()
              ? IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_SHOWN
              : IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_HIDDEN));
}

void BirchChipButton::ShutdownSelectionWidget() {
  tab_app_selection_widget_.reset();
}

void BirchChipButton::ReloadIcon() {
  item_->LoadIcon(base::BindOnce(&BirchChipButton::SetIconImage,
                                 weak_factory_.GetWeakPtr()));
}

void BirchChipButton::Init(BirchItem* item) {
  item_ = item;

  title_->SetText(item_->title());
  subtitle_->SetText(item_->subtitle());

  SetCallback(
      base::BindRepeating(&BirchItem::PerformAction, base::Unretained(item_)));

  const auto addon_type = item_->GetAddonType();
  // Add add-ons according to the add-on type.
  switch (addon_type) {
    case BirchAddonType::kButton: {
      base::RepeatingClosure callback = base::BindRepeating(
          &BirchItem::PerformAddonAction, base::Unretained(item_));
      auto button = birch_bar_util::CreateAddonButton(std::move(callback),
                                                      *item_->addon_label());
      button->SetTooltipText(item->GetAddonAccessibleName());
      SetAddon(std::move(button));
      break;
    }
    case BirchAddonType::kCoralButton: {
      // Coral item works different since it triggers a new overview view.
      base::RepeatingClosure callback = base::BindRepeating(
          &BirchChipButton::OnCoralAddonClicked, base::Unretained(this));
      // Coral chip's addon button contains no text.
      auto button = birch_bar_util::CreateCoralAddonButton(
          std::move(callback), vector_icons::kCaretUpIcon);
      button->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_BIRCH_CORAL_ADDON_SELECTOR_HIDDEN));
      SetAddon(std::move(button));
      // Coral chip gets the real title from the group.
      auto* coral_provider = BirchCoralProvider::Get();
      const std::optional<std::string>& group_title =
          coral_provider
              ? coral_provider
                    ->GetGroupById(
                        static_cast<BirchCoralItem*>(item_)->group_id())
                    ->title
              : "";
      if (group_title) {
        // If the title is not empty, reset the `title_` with the real title.
        if (!group_title->empty()) {
          title_->SetText(base::UTF8ToUTF16(*group_title));
        }
        // Show title and delete the loading animation.
        title_->SetVisible(true);
        if (!!title_loading_animated_image_) {
          title_loading_animated_image_->Stop();
          titles_container_->RemoveChildViewT(
              std::exchange(title_loading_animated_image_, nullptr));
        }
      } else {
        // If the title is null, show the animation to wait for title loading.
        title_->SetVisible(false);

        BuildTitleLoadingAnimation();
        title_loading_animated_image_->Play(
            birch_animation_utils::GetLottiePlaybackConfig(
                *title_loading_animated_image_->animated_image()->skottie(),
                IDR_CORAL_LOADING_TITLE_ANIMATION));
      }
      break;
    }
    case BirchAddonType::kWeatherTempLabelC:
    case BirchAddonType::kWeatherTempLabelF:
      SetAddon(birch_bar_util::CreateWeatherTemperatureView(
          *item_->addon_label(),
          addon_type == BirchAddonType::kWeatherTempLabelF));
      break;
    case BirchAddonType::kNone:
      break;
  }
  item_->LoadIcon(base::BindOnce(&BirchChipButton::SetIconImage,
                                 weak_factory_.GetWeakPtr()));

  SetAccessibleName(item_->GetAccessibleName());
}

const BirchItem* BirchChipButton::GetItem() const {
  return item_.get();
}

BirchItem* BirchChipButton::GetItem() {
  return item_.get();
}

void BirchChipButton::Shutdown() {
  item_ = nullptr;

  // Invalidate all weakptrs to avoid previously triggered callbacks from using
  // `item_`.
  weak_factory_.InvalidateWeakPtrs();
}

void BirchChipButton::ExecuteCommand(int command_id, int event_flags) {
  auto* birch_bar_controller = BirchBarController::Get();
  CHECK(birch_bar_controller);

  using CommandId = BirchChipContextMenuModel::CommandId;

  switch (command_id) {
    case base::to_underlying(CommandId::kHideSuggestion):
      birch_bar_controller->OnItemHiddenByUser(item_);
      break;
    case base::to_underlying(CommandId::kHideWeatherSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kWeather,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kToggleTemperatureUnits):
      birch_bar_controller->ToggleTemperatureUnits();
      break;
    case base::to_underlying(CommandId::kHideCalendarSuggestions):
      birch_bar_controller->SetShowSuggestionType(
          BirchSuggestionType::kCalendar,
          /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideDriveSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kDrive,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideChromeTabSuggestions):
      birch_bar_controller->SetShowSuggestionType(
          BirchSuggestionType::kChromeTab,
          /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideMediaSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kMedia,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kHideCoralSuggestions):
      birch_bar_controller->SetShowSuggestionType(BirchSuggestionType::kCoral,
                                                  /*show=*/false);
      break;
    case base::to_underlying(CommandId::kCoralNewDesk):
      item_->PerformAction();
      break;
    case base::to_underlying(CommandId::kCoralSaveForLater): {
      CHECK_EQ(BirchItemType::kCoral, item_->GetType());
      auto* coral_provider = BirchCoralProvider::Get();
      Shell::Get()->coral_delegate()->CreateSavedDeskFromGroup(
          coral_provider->ExtractGroupById(
              static_cast<BirchCoralItem*>(item_)->group_id()));
      break;
    }
    case base::to_underlying(CommandId::kProvideFeedback):
      birch_bar_controller->ProvideFeedbackForCoral();
      break;
    default:
      birch_bar_controller->ExecuteMenuCommand(command_id, /*from_chip=*/true);
  }
}

void BirchChipButton::SetAddon(std::unique_ptr<views::View> addon_view) {
  if (addon_view_) {
    RemoveChildViewT(std::exchange(addon_view_, nullptr));
  } else {
    flex_layout_->SetInteriorMargin(kInteriorMarginsWithAddon);
  }
  addon_view_ = AddChildView(std::move(addon_view));
}

void BirchChipButton::SetIconImage(PrimaryIconType primary_icon_type,
                                   SecondaryIconType secondary_icon_type,
                                   const ui::ImageModel& icon_image) {
  icon_parent_view_->RemoveAllChildViews();
  icon_parent_view_->AddChildView(
      CreatePrimaryImageView(icon_image, primary_icon_type));
  if (secondary_icon_type != SecondaryIconType::kNoIcon) {
    icon_parent_view_->AddChildView(
        CreateSecondaryImageView(secondary_icon_type));
  }
}

void BirchChipButton::OnCoralAddonClicked() {
  CHECK_EQ(BirchItemType::kCoral, item_->GetType());

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

void BirchChipButton::BuildTitleLoadingAnimation() {
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
  // Setup rounder corners for `title_loading_animated_image_`.
  title_loading_animated_image_ =
      titles_container_->AddChildViewAt(std::move(title_loading_animated_image),
                                        /*index=*/0);
}

BEGIN_METADATA(BirchChipButton)
END_METADATA

}  // namespace ash
