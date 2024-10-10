// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_chip_button.h"

#include "ash/birch/birch_item.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/resources/grit/mahi_resources.h"
#include "ash/wm/overview/birch/birch_animation_utils.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_controller.h"
#include "ash/wm/overview/birch/birch_bar_util.h"
#include "ash/wm/overview/birch/birch_chip_context_menu_model.h"
#include "ash/wm/overview/birch/tab_app_selection_host.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
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

namespace ash {

namespace {

// The color and layout parameters of the chip.
constexpr gfx::Insets kInteriorMarginsNoAddon = gfx::Insets::TLBR(12, 0, 8, 20);
constexpr gfx::Insets kInteriorMarginsWithAddon = gfx::Insets::VH(12, 0);

// The layout parameters of icon.
constexpr gfx::Insets kIconMargins = gfx::Insets::TLBR(0, 12, 0, 8);
constexpr int kMainIconViewSize = 40;
constexpr int kParentIconViewSize = 44;
constexpr int kSecondaryIconViewSize = 20;
constexpr int kSecondaryIconImageSize = 12;
constexpr int kFaviconSize = 32;
constexpr int kFaviconCornerRadius = 8;
constexpr int kAppIconSize = 16;
constexpr int kAppCornerRadius = 20;
constexpr int kIllustrationSize = 40;
constexpr int kCoralGroupedImageSize = 40;
constexpr int kIllustrationCornerRadius = 8;
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

//
constexpr gfx::Size kLoadingAnimationSize = gfx::Size(100, 20);
constexpr int kLoadingAnimationRadius = 10;

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
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {
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
              .SetProperty(views::kMarginsKey, kIconMargins)
              .SetVisible(true)
              .AddChildren(
                  // Main icon.
                  views::Builder<views::ImageView>().CopyAddressTo(
                      &primary_icon_view_),
                  // Secondary icon.
                  views::Builder<views::ImageView>().CopyAddressTo(
                      &secondary_icon_view_)),
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

void BirchChipButton::Init(BirchItem* item) {
  item_ = item;

  title_->SetText(item_->title());
  subtitle_->SetText(item_->subtitle());

  SetCallback(base::BindRepeating(
      &BirchItem::PerformAction, base::Unretained(item_),
      /*is_post_login=*/BirchBarController::Get()->is_informed_restore()));

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
          std::move(callback), vector_icons::kCaretUpIcon,
          item->GetAddonAccessibleName());
      button->SetTooltipText(item->GetAddonAccessibleName());
      SetAddon(std::move(button));
      // Show loading animation for title if `item` has a dummy title.
      if (item_->title() == u"CoralTitle") {
        title_->SetVisible(false);

        BuildTitleLoadingAnimation();
        title_loading_animated_image_->Play(
            birch_animation_utils::GetLottiePlaybackConfig(
                *title_loading_animated_image_->animated_image()->skottie(),
                // TODO(yulunwu) replace loading animation when available.
                IDR_MAHI_LOADING_OUTLINES_ANIMATION));
      } else {
        // Show title and delete the loading animation.
        title_->SetVisible(true);
        if (!!title_loading_animated_image_) {
          title_loading_animated_image_->Stop();
          titles_container_->RemoveChildViewT(
              std::exchange(title_loading_animated_image_, nullptr));
        }
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
      // TODO(yulunwu) implement behavior
      break;
    case base::to_underlying(CommandId::kCoralSaveForLater):
      // TODO(yulunwu) implement behavior
      break;
    case base::to_underlying(CommandId::kProvideFeedback):
      Shell::Get()->shell_delegate()->OpenFeedbackDialog(
          ShellDelegate::FeedbackSource::kOverview,
          /*description_template=*/std::string(),
          /*category_tag=*/"Coral");
      break;
    default:
      birch_bar_controller->ExecuteMenuCommand(command_id, /*from_chip=*/true);
  }
}

void BirchChipButton::SetAddon(std::unique_ptr<views::View> addon_view) {
  if (addon_view_) {
    RemoveChildViewT(addon_view_);
  } else {
    flex_layout_->SetInteriorMargin(kInteriorMarginsWithAddon);
  }
  addon_view_ = AddChildView(std::move(addon_view));
}

void BirchChipButton::StylizeIconForItemType(
    BirchItemType type,
    SecondaryIconType secondary_icon_type,
    bool use_smaller_dimension) {
  int icon_size;
  int rounded_corners;
  std::optional<ui::ColorId> background_color_id;

  switch (type) {
    case BirchItemType::kTest:
    case BirchItemType::kCalendar:
    case BirchItemType::kAttachment:
    case BirchItemType::kFile:
      icon_size = kAppIconSize;
      rounded_corners = kAppCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
    case BirchItemType::kWeather:
      icon_size = kWeatherImageSize;
      break;
    case BirchItemType::kReleaseNotes:
      icon_size = kIllustrationSize;
      rounded_corners = kIllustrationCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
    case BirchItemType::kCoral:
      icon_size = kCoralGroupedImageSize;
      break;
    case BirchItemType::kTab:
    case BirchItemType::kSelfShare:
    case BirchItemType::kMostVisited:
    case BirchItemType::kLastActive:
    case BirchItemType::kLostMedia:
      // When `use_smaller_dimension` is true, we use the smaller app icon sizes
      // because we have access only to smaller icons.
      use_smaller_dimension ? icon_size = kAppIconSize
                            : icon_size = kFaviconSize;
      rounded_corners = kFaviconCornerRadius;
      background_color_id = kIconBackgroundColorId;
      break;
  }

  primary_icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  primary_icon_view_->SetBounds(0, 0, kMainIconViewSize, kMainIconViewSize);

  primary_icon_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets((kMainIconViewSize - icon_size) / 2)));

  if (background_color_id.has_value()) {
    primary_icon_view_->SetBackground(views::CreateThemedRoundedRectBackground(
        background_color_id.value(), rounded_corners));
  }

  // Due to https://b/364912772, self share items are created with `kNoIcon`
  // when no form factor is found. We still want to show a generic self share
  // icon in this case.
  if (secondary_icon_type == SecondaryIconType::kNoIcon &&
      item_->GetType() != BirchItemType::kSelfShare) {
    secondary_icon_view_->SetVisible(false);
    return;
  }

  secondary_icon_view_->SetImageSize(
      gfx::Size(kSecondaryIconImageSize, kSecondaryIconImageSize));
  secondary_icon_view_->SetBounds(24, 24, kSecondaryIconViewSize,
                                  kSecondaryIconViewSize);
  secondary_icon_view_->SetBackground(views::CreateThemedRoundedRectBackground(
      kSecondaryIconBackgroundColorId, kSecondaryIconViewSize / 2));
  secondary_icon_view_->SetBorder(views::CreateThemedRoundedRectBorder(
      1, kSecondaryIconViewSize / 2, cros_tokens::kCrosSysSystemOnBaseOpaque));
}

void BirchChipButton::SetIconImage(const ui::ImageModel& icon_image,
                                   SecondaryIconType secondary_icon_type) {
  primary_icon_view_->SetImage(icon_image);

  if (secondary_icon_type != SecondaryIconType::kNoIcon) {
    ui::ImageModel secondary_icon_image;

    switch (secondary_icon_type) {
      case SecondaryIconType::kTabFromDesktop:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconDesktopIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kTabFromPhone:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconPortraitIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kTabFromTablet:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconLandscapeIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kTabFromUnknown:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconUnknownIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kLostMediaAudio:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconAudioIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kLostMediaVideo:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconVideoIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kLostMediaVideoConference:
        secondary_icon_image = ui::ImageModel::FromVectorIcon(
            kBirchSecondaryIconVideoConferenceIcon, kSecondaryIconColorId);
        break;
      case SecondaryIconType::kNoIcon:
        break;
    }
    secondary_icon_view_->SetImage(secondary_icon_image);
  }

  // TODO(https://b/364912772): Remove temporary fix by adding sender's device
  // form_factor to `SelfTabToSelfEntry`.
  if (item_->GetType() == BirchItemType::kSelfShare) {
    // All Self Share Birch Items will utilize a generic share secondary icon as
    // part of a temporary fix.
    secondary_icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        kBirchSecondaryIconGenericShareIcon, kSecondaryIconColorId));
  }

  bool use_smaller_dimension = icon_image.Size().width() <= kAppIconSize ||
                               icon_image.Size().height() <= kAppIconSize;
  StylizeIconForItemType(item_->GetType(), secondary_icon_type,
                         use_smaller_dimension);
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
    tab_app_selection_widget_->Hide();
  }
}

void BirchChipButton::BuildTitleLoadingAnimation() {
  // Build `title_loading_animated_image_` and insert into the
  // front of `titles_container_`.
  // TODO(yulunwu) update animation file when available.
  std::unique_ptr<views::AnimatedImageView> title_loading_animated_image =
      views::Builder<views::AnimatedImageView>()
          .SetAnimatedImage(birch_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_OUTLINES_ANIMATION))
          .SetImageSize(kLoadingAnimationSize)
          .SetVisible(true)
          .Build();
  // Setup rounder corners for `title_loading_animated_image_`.
  title_loading_animated_image->SetPaintToLayer();
  title_loading_animated_image->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kLoadingAnimationRadius));
  title_loading_animated_image_ =
      titles_container_->AddChildViewAt(std::move(title_loading_animated_image),
                                        /*index=*/0);
}

BEGIN_METADATA(BirchChipButton)
END_METADATA

}  // namespace ash
