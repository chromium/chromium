// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/icon_button.h"

#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/blurred_background_shield.h"
#include "ash/style/style_util.h"
#include "ash/style/text_image.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "chromeos/utils/haptics_util.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
namespace {

constexpr int kXSmallButtonSize = 20;
constexpr int kSmallButtonSize = 24;
constexpr int kMediumButtonSize = 32;
constexpr int kLargeButtonSize = 36;
constexpr int kXLargeButtonSize = 48;

// Icon size of the small, medium and large size buttons.
constexpr int kIconSize = 20;
// Icon size of the extra small size button.
constexpr int kXSmallIconSize = 16;

// The gap between the focus ring and the button's content.
constexpr int kFocusRingPadding = 2;

// The default toggled background and icon color IDs.
constexpr ui::ColorId kDefaultToggledBackgroundColorId =
    cros_tokens::kCrosSysSystemPrimaryContainer;
constexpr ui::ColorId kDefaultToggledIconColorId =
    cros_tokens::kCrosSysSystemOnPrimaryContainer;

int GetButtonSizeOnType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmall:
    case IconButton::Type::kXSmallProminent:
    case IconButton::Type::kXSmallFloating:
    case IconButton::Type::kXSmallProminentFloating:
      return kXSmallButtonSize;
    case IconButton::Type::kSmall:
    case IconButton::Type::kSmallProminent:
    case IconButton::Type::kSmallFloating:
    case IconButton::Type::kSmallProminentFloating:
      return kSmallButtonSize;
    case IconButton::Type::kMedium:
    case IconButton::Type::kMediumProminent:
    case IconButton::Type::kMediumFloating:
    case IconButton::Type::kMediumProminentFloating:
      return kMediumButtonSize;
    case IconButton::Type::kLarge:
    case IconButton::Type::kLargeProminent:
    case IconButton::Type::kLargeFloating:
    case IconButton::Type::kLargeProminentFloating:
      return kLargeButtonSize;
    case IconButton::Type::kXLarge:
    case IconButton::Type::kXLargeProminent:
    case IconButton::Type::kXLargeFloating:
    case IconButton::Type::kXLargeProminentFloating:
      return kXLargeButtonSize;
  }
}

std::optional<ui::ColorId> GetDefaultBackgroundColorId(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmall:
    case IconButton::Type::kSmall:
    case IconButton::Type::kMedium:
    case IconButton::Type::kLarge:
    case IconButton::Type::kXLarge:
      return cros_tokens::kCrosSysSystemOnBase;
    case IconButton::Type::kXSmallProminent:
    case IconButton::Type::kSmallProminent:
    case IconButton::Type::kMediumProminent:
    case IconButton::Type::kLargeProminent:
    case IconButton::Type::kXLargeProminent:
      return cros_tokens::kCrosSysSystemPrimaryContainer;
    default:
      NOTREACHED() << "Floating type button does not have a background";
  }
}

ui::ColorId GetDefaultIconColorId(IconButton::Type type, bool focused) {
  switch (type) {
    case IconButton::Type::kXSmall:
    case IconButton::Type::kXSmallFloating:
    case IconButton::Type::kSmall:
    case IconButton::Type::kSmallFloating:
    case IconButton::Type::kMedium:
    case IconButton::Type::kMediumFloating:
    case IconButton::Type::kLarge:
    case IconButton::Type::kLargeFloating:
    case IconButton::Type::kXLarge:
    case IconButton::Type::kXLargeFloating:
      return cros_tokens::kCrosSysOnSurface;
    case IconButton::Type::kXSmallProminent:
    case IconButton::Type::kSmallProminent:
    case IconButton::Type::kMediumProminent:
    case IconButton::Type::kLargeProminent:
    case IconButton::Type::kXLargeProminent:
      return cros_tokens::kCrosSysSystemOnPrimaryContainer;
    case IconButton::Type::kXSmallProminentFloating:
    case IconButton::Type::kSmallProminentFloating:
    case IconButton::Type::kMediumProminentFloating:
    case IconButton::Type::kLargeProminentFloating:
    case IconButton::Type::kXLargeProminentFloating:
      return focused ? cros_tokens::kCrosSysPrimary
                     : cros_tokens::kCrosSysSecondary;
  }
}

int GetIconSizeOnType(IconButton::Type type) {
  if (type == IconButton::Type::kXSmall ||
      type == IconButton::Type::kXSmallFloating ||
      type == IconButton::Type::kXSmallProminent ||
      type == IconButton::Type::kXSmallProminentFloating) {
    return kXSmallIconSize;
  }
  return kIconSize;
}

bool IsFloatingIconButton(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmallFloating:
    case IconButton::Type::kXSmallProminentFloating:
    case IconButton::Type::kSmallFloating:
    case IconButton::Type::kSmallProminentFloating:
    case IconButton::Type::kMediumFloating:
    case IconButton::Type::kMediumProminentFloating:
    case IconButton::Type::kLargeFloating:
    case IconButton::Type::kLargeProminentFloating:
    case IconButton::Type::kXLargeFloating:
    case IconButton::Type::kXLargeProminentFloating:
      return true;
    default:
      break;
  }

  return false;
}

bool IsProminentFloatingType(IconButton::Type type) {
  switch (type) {
    case IconButton::Type::kXSmallProminentFloating:
    case IconButton::Type::kSmallProminentFloating:
    case IconButton::Type::kMediumProminentFloating:
    case IconButton::Type::kLargeProminentFloating:
    case IconButton::Type::kXLargeProminentFloating:
      return true;
    default:
      break;
  }

  return false;
}

// Create a themed fully rounded rect background for icon button.
std::unique_ptr<views::Background> CreateThemedBackground(
    ui::ColorId color_id,
    IconButton::Type type) {
  return views::CreateThemedRoundedRectBackground(
      color_id, GetButtonSizeOnType(type) / 2);
}

// Create a solid color fully rounded rect background for icon button.
std::unique_ptr<views::Background> CreateSolidBackground(
    SkColor color,
    IconButton::Type type) {
  return views::CreateRoundedRectBackground(color,
                                            GetButtonSizeOnType(type) / 2);
}

// Returns a normal and disabled image model for `symbol`. `is_toggled` and
// `color_id` control styling. The returned model dimensions wil be `icon_size`
// x `icon_size` in pixels.
std::pair<ui::ImageModel, ui::ImageModel> SymbolImages(
    const bool is_toggled,
    ui::ColorId color_id,
    const int icon_size,
    base_icu::UChar32 symbol) {
  const gfx::Size size(icon_size, icon_size);
  ui::ImageModel normal_model = TextImage::AsImageModel(size, symbol, color_id);
  ui::ImageModel disabled_model =
      TextImage::AsImageModel(size, symbol, cros_tokens::kCrosSysDisabled);

  return {normal_model, disabled_model};
}

}  // namespace

IconButton::Builder::Builder()
    : callback_(),
      type_(IconButton::Type::kSmall),
      icon_(nullptr),
      accessible_name_(u""),
      is_togglable_(false),
      has_border_(false) {}

IconButton::Builder::~Builder() = default;

std::unique_ptr<IconButton> IconButton::Builder::Build() {
  if (!character_) {
    // `icon_` must be non-null if there is no character.
    CHECK(icon_);
  }

  std::u16string accessible_name;
  if (absl::holds_alternative<int>(accessible_name_)) {
    accessible_name =
        l10n_util::GetStringUTF16(absl::get<int>(accessible_name_));
  } else {
    accessible_name = absl::get<std::u16string>(accessible_name_);
  }

  auto button = std::make_unique<IconButton>(
      std::move(callback_), type_, icon_, accessible_name,
      /*is_togglable=*/is_togglable_, /*has_border=*/has_border_);
  if (view_id_.has_value()) {
    button->SetID(*view_id_);
  }
  if (enabled_.has_value()) {
    button->SetEnabled(*enabled_);
  }
  if (visible_.has_value()) {
    button->SetVisible(*visible_);
  }
  if (background_image_.has_value()) {
    button->SetBackgroundImage(*background_image_);
  }
  if (background_color_.has_value()) {
    button->SetBackgroundColor(*background_color_);
  }
  if (character_.has_value()) {
    button->SetSymbol(*character_);
  }

  return button;
}

IconButton::Builder& IconButton::Builder::SetCallback(
    PressedCallback callback) {
  callback_ = std::move(callback);
  return *this;
}
IconButton::Builder& IconButton::Builder::SetType(Type type) {
  type_ = type;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetVectorIcon(
    const gfx::VectorIcon* icon) {
  CHECK(icon);
  icon_ = icon;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetSymbol(
    base_icu::UChar32 character) {
  character_ = character;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetAccessibleNameId(
    int accessible_name_id) {
  accessible_name_ = accessible_name_id;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetAccessibleName(
    const std::u16string& accessible_name) {
  accessible_name_ = accessible_name;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetTogglable(bool is_togglable) {
  is_togglable_ = is_togglable;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetBorder(bool has_border) {
  has_border_ = has_border;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetViewId(int view_id) {
  view_id_ = view_id;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetEnabled(bool enabled) {
  enabled_ = enabled;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetVisible(bool visible) {
  visible_ = visible;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetBackgroundImage(
    const gfx::ImageSkia& background_image) {
  background_image_ = background_image;
  return *this;
}
IconButton::Builder& IconButton::Builder::SetBackgroundColor(
    ui::ColorId background_color) {
  background_color_ = background_color;
  return *this;
}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       int accessible_name_id)
    : IconButton(std::move(callback),
                 type,
                 icon,
                 accessible_name_id,
                 /*is_togglable=*/false,
                 /*has_border=*/false) {}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       bool is_togglable,
                       bool has_border)
    : views::ImageButton(std::move(callback)),
      type_(type),
      icon_(icon),
      is_togglable_(is_togglable),
      background_toggled_color_(kDefaultToggledBackgroundColorId),
      icon_color_(GetDefaultIconColorId(type, /*focused=*/false)),
      icon_toggled_color_(kDefaultToggledIconColorId) {
  const int button_size = GetButtonSizeOnType(type);
  SetPreferredSize(gfx::Size(button_size, button_size));

  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false);

  if (!IsFloatingIconButton(type)) {
    background_color_ = GetDefaultBackgroundColorId(type).value();
  }

  UpdateBackground();
  if (icon_) {
    UpdateVectorIcon();
  }
  UpdateAccessibilityProperties();

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  if (has_border) {
    // The focus ring will have the outline padding with the bounds of the
    // buttons.
    focus_ring->SetPathGenerator(
        std::make_unique<views::CircleHighlightPathGenerator>(-gfx::Insets(
            focus_ring->GetHaloThickness() / 2 + kFocusRingPadding)));
  }

  views::InstallCircleHighlightPathGenerator(this);

  enabled_changed_subscription_ = AddEnabledChangedCallback(base::BindRepeating(
      &IconButton::OnEnabledStateChanged, base::Unretained(this)));
}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       const std::u16string& accessible_name,
                       bool is_togglable,
                       bool has_border)
    : IconButton(std::move(callback), type, icon, is_togglable, has_border) {
  SetTooltipText(accessible_name);
}

IconButton::IconButton(PressedCallback callback,
                       IconButton::Type type,
                       const gfx::VectorIcon* icon,
                       int accessible_name_id,
                       bool is_togglable,
                       bool has_border)
    : IconButton(std::move(callback),
                 type,
                 icon,
                 l10n_util::GetStringUTF16(accessible_name_id),
                 is_togglable,
                 has_border) {}

IconButton::~IconButton() = default;

void IconButton::SetButtonBehavior(DisabledButtonBehavior button_behavior) {
  if (button_behavior_ == button_behavior) {
    return;
  }

  button_behavior_ = button_behavior;
  // Change button behavior may impact the toggled state.
  if (toggled_ && !GetEnabled()) {
    UpdateVectorIcon();
  }
}

void IconButton::SetVectorIcon(const gfx::VectorIcon& icon) {
  icon_ = &icon;
  if (!IsToggledOn()) {
    UpdateVectorIcon();
  }
}

void IconButton::SetSymbol(base_icu::UChar32 character) {
  CHECK(base::IsValidCodepoint(character));
  character_ = character;
  UpdateVectorIcon();
}

void IconButton::SetToggledVectorIcon(const gfx::VectorIcon& icon) {
  toggled_icon_ = &icon;
  if (IsToggledOn()) {
    UpdateVectorIcon();
  }
}

void IconButton::SetBackgroundColor(ColorVariant background_color) {
  if (background_color_ == background_color) {
    return;
  }

  background_color_ = background_color;
  if (GetEnabled() && !IsToggledOn()) {
    UpdateBackground();
  }
}

void IconButton::SetBackgroundToggledColor(
    ColorVariant background_toggled_color) {
  if (!is_togglable_ || background_toggled_color == background_toggled_color_) {
    return;
  }

  background_toggled_color_ = background_toggled_color;
  if (GetEnabled() && IsToggledOn()) {
    UpdateBackground();
  }
}

void IconButton::SetBackgroundImage(const gfx::ImageSkia& background_image) {
  background_image_ = gfx::ImageSkiaOperations::CreateResizedImage(
      background_image, skia::ImageOperations::RESIZE_BEST, GetPreferredSize());
  SchedulePaint();
}

void IconButton::SetIconColor(ColorVariant icon_color) {
  if (icon_color_ == icon_color) {
    return;
  }

  icon_color_ = icon_color;
  if (!IsToggledOn()) {
    UpdateVectorIcon(/*color_changes_only=*/true);
  }
}

void IconButton::SetIconToggledColor(ColorVariant icon_toggled_color) {
  if (!is_togglable_ || icon_toggled_color == icon_toggled_color_) {
    return;
  }

  icon_toggled_color_ = icon_toggled_color;
  if (IsToggledOn()) {
    UpdateVectorIcon(/*color_changes_only=*/true);
  }
}

void IconButton::SetIconSize(int size) {
  if (icon_size_ == size) {
    return;
  }

  icon_size_ = size;
  UpdateVectorIcon();
}

void IconButton::SetToggled(bool toggled) {
  if (!is_togglable_ || toggled_ == toggled) {
    return;
  }

  toggled_ = toggled;

  UpdateAccessibilityProperties();

  if (GetEnabled()) {
    UpdateBackground();
  }

  // If toggle state is changed with `toggled_`, update the icon.
  if (GetEnabled() ||
      button_behavior_ ==
          DisabledButtonBehavior::kCanDisplayDisabledToggleValue) {
    UpdateVectorIcon();
  }
}

void IconButton::SetEnableBlurredBackgroundShield(bool enable) {
  if (blurred_background_shield_enabled_ == enable) {
    return;
  }
  blurred_background_shield_enabled_ = enable;
  if (blurred_background_shield_enabled_) {
    SetBackground(nullptr);
  } else {
    blurred_background_shield_.reset();
  }
  UpdateBackground();
}

void IconButton::OnFocus() {
  // Update prominent floating type button's icon color on focus.
  if (IsProminentFloatingType(type_) && !IsToggledOn()) {
    // If prominent floating button is still using default colors, updates its
    // icon color on focus.
    if (absl::holds_alternative<ui::ColorId>(icon_color_) &&
        absl::get<ui::ColorId>(icon_color_) ==
            GetDefaultIconColorId(type_, /*focused=*/false)) {
      icon_color_ = GetDefaultIconColorId(type_, /*focused=*/true);
      UpdateVectorIcon(/*color_changes_only=*/true);
    }
  }
}

void IconButton::OnBlur() {
  // Update prominent floating type button's icon color on blur.
  if (IsProminentFloatingType(type_) && !IsToggledOn()) {
    // If prominent floating button is still using default colors, updates its
    // icon color on focus.
    if (absl::holds_alternative<ui::ColorId>(icon_color_) &&
        absl::get<ui::ColorId>(icon_color_) ==
            GetDefaultIconColorId(type_, /*focused=*/true)) {
      icon_color_ = GetDefaultIconColorId(type_, /*focused=*/false);
      UpdateVectorIcon(/*color_changes_only=*/true);
    }
  }
}

void IconButton::PaintButtonContents(gfx::Canvas* canvas) {
  if (!IsFloatingIconButton(type_) || IsToggledOn()) {
    // Apply the background image. This is painted on top of the |color|.
    if (!background_image_.isNull()) {
      const gfx::Rect rect(GetContentsBounds());
      cc::PaintFlags flags;
      flags.setAntiAlias(true);
      SkPath mask;
      mask.addCircle(rect.CenterPoint().x(), rect.CenterPoint().y(),
                     rect.width() / 2);
      canvas->ClipPath(mask, true);
      canvas->DrawImageInt(background_image_, 0, 0, flags);
    }
  }

  views::ImageButton::PaintButtonContents(canvas);
}

void IconButton::NotifyClick(const ui::Event& event) {
  if (is_togglable_) {
    chromeos::haptics_util::PlayHapticToggleEffect(
        !toggled_, ui::HapticTouchpadEffectStrength::kMedium);
  }

  views::Button::NotifyClick(event);
}

void IconButton::UpdateBackground() {
  if (blurred_background_shield_enabled_) {
    UpdateBlurredBackgroundShield();
    return;
  }

  // The untoggled floating button does not have a background.
  const bool is_toggled = IsToggledOn();
  if (IsFloatingIconButton(type_) && !is_toggled) {
    SetBackground(nullptr);
    return;
  }

  // Create a themed rounded rect background when the button is disabled.
  if (!GetEnabled()) {
    SetBackground(
        CreateThemedBackground(cros_tokens::kCrosSysDisabledContainer, type_));
    return;
  }

  // Create a background according to the toggled state.
  ColorVariant color_variant =
      is_toggled ? background_toggled_color_ : background_color_;
  if (absl::holds_alternative<SkColor>(color_variant)) {
    SetBackground(
        CreateSolidBackground(absl::get<SkColor>(color_variant), type_));
  } else {
    SetBackground(
        CreateThemedBackground(absl::get<ui::ColorId>(color_variant), type_));
  }
}

void IconButton::UpdateBlurredBackgroundShield() {
  CHECK(blurred_background_shield_enabled_);
  const bool is_toggled = IsToggledOn();
  if (IsFloatingIconButton(type_) && !is_toggled) {
    blurred_background_shield_.reset();
    return;
  }

  // Create a new blurred background shield if needed.
  if (!blurred_background_shield_) {
    blurred_background_shield_ = std::make_unique<BlurredBackgroundShield>(
        this, background_color_, ColorProvider::kBackgroundBlurSigma,
        gfx::RoundedCornersF(GetButtonSizeOnType(type_) / 2));
  }

  ColorVariant color_variant =
      GetEnabled()
          ? (is_toggled ? background_toggled_color_ : background_color_)
          : ColorVariant(cros_tokens::kCrosSysDisabledContainer);

  if (absl::holds_alternative<SkColor>(color_variant)) {
    blurred_background_shield_->SetColor(absl::get<SkColor>(color_variant));
  } else {
    blurred_background_shield_->SetColorId(
        absl::get<ui::ColorId>(color_variant));
  }
}

void IconButton::UpdateVectorIcon(bool color_changes_only) {
  std::pair<ui::ImageModel, ui::ImageModel> images;

  const int icon_size = icon_size_.value_or(GetIconSizeOnType(type_));
  const bool is_toggled = IsToggledOn();
  ColorVariant color_variant = is_toggled ? icon_toggled_color_ : icon_color_;

  if (character_.has_value()) {
    images = SymbolImages(is_toggled, absl::get<ui::ColorId>(color_variant),
                          icon_size, *character_);
  } else {
    images = VectorImages(is_toggled, color_variant, icon_size);
  }

  if (images.first.IsEmpty()) {
    return;
  }

  ui::ImageModel new_normal_image_model = images.first;

  if (GetWidget()) {
    // Skip repainting if the incoming icon is the same as the current icon. If
    // the icon has been painted before, |gfx::CreateVectorIcon()| will simply
    // grab the ImageSkia from a cache, so it will be cheap. Note that this
    // assumes that toggled/disabled images changes at the same time as the
    // normal image, which it currently does.
    const gfx::ImageSkia new_normal_image =
        new_normal_image_model.Rasterize(GetColorProvider());
    const gfx::ImageSkia& old_normal_image =
        GetImage(views::Button::STATE_NORMAL);
    if (!new_normal_image.isNull() && !old_normal_image.isNull() &&
        new_normal_image.BackedBySameObjectAs(old_normal_image)) {
      return;
    }
  }

  SetImageModel(views::Button::STATE_NORMAL, new_normal_image_model);
  if (!color_changes_only) {
    SetImageModel(views::Button::STATE_DISABLED, images.second);
  }
}

void IconButton::OnEnabledStateChanged() {
  // Enabled state change may cause toggled state change.
  if (toggled_ && button_behavior_ !=
                      DisabledButtonBehavior::kCanDisplayDisabledToggleValue) {
    UpdateVectorIcon();
  }
  UpdateBackground();
}

SkColor IconButton::GetBackgroundColor() const {
  DCHECK(background());
  return background()->get_color();
}

bool IconButton::IsToggledOn() const {
  return toggled_ &&
         (GetEnabled() ||
          button_behavior_ ==
              DisabledButtonBehavior::kCanDisplayDisabledToggleValue);
}

void IconButton::UpdateAccessibilityProperties() {
  if (is_togglable_) {
    GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    GetViewAccessibility().SetCheckedState(
        toggled_ ? ax::mojom::CheckedState::kTrue
                 : ax::mojom::CheckedState::kFalse);
  } else {
    GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    GetViewAccessibility().RemoveCheckedState();
  }
}

std::pair<ui::ImageModel, ui::ImageModel> IconButton::VectorImages(
    const bool is_toggled,
    const ColorVariant color_variant,
    const int icon_size) {
  const gfx::VectorIcon* icon =
      is_toggled && toggled_icon_ ? toggled_icon_.get() : icon_.get();

  if (!icon) {
    return {ui::ImageModel(), ui::ImageModel()};
  }

  ui::ImageModel new_normal_image_model;
  if (absl::holds_alternative<SkColor>(color_variant)) {
    new_normal_image_model = ui::ImageModel::FromVectorIcon(
        *icon, absl::get<SkColor>(color_variant), icon_size);
  } else {
    new_normal_image_model = ui::ImageModel::FromVectorIcon(
        *icon, absl::get<ui::ColorId>(color_variant), icon_size);
  }

  ui::ImageModel disabled_image_model = ui::ImageModel::FromVectorIcon(
      *icon, cros_tokens::kCrosSysDisabled, icon_size);

  return {std::move(new_normal_image_model), std::move(disabled_image_model)};
}

BEGIN_METADATA(IconButton)
END_METADATA

}  // namespace ash
