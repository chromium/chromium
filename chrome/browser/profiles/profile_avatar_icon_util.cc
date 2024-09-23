// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/profiles/profile_avatar_icon_util.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/profiles/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/vector_icons/vector_icons.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/dynamic_color/palette_factory.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/native_theme/native_theme.h"
#include "url/url_canon.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/grit/chrome_unscaled_resources.h"  // nogncheck crbug.com/1125897
#include "ui/gfx/icon_util.h"  // For Iconutil::kLargeIconSize.
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#endif

// Helper methods for transforming and drawing avatar icons.
namespace {

// Palette color tones for the material color utils used to select a
// material-appropriate color for a given profile color,
constexpr float kIconToneDark = 40.f;
constexpr float kIconToneLight = 80.f;

#if BUILDFLAG(IS_WIN)
const int kOldAvatarIconWidth = 38;
const int kOldAvatarIconHeight = 31;

// 2x sized versions of the old profile avatar icons.
// TODO(crbug.com/41444689): Clean this up.
const int kProfileAvatarIconResources2x[] = {
    IDR_PROFILE_AVATAR_2X_0,  IDR_PROFILE_AVATAR_2X_1,
    IDR_PROFILE_AVATAR_2X_2,  IDR_PROFILE_AVATAR_2X_3,
    IDR_PROFILE_AVATAR_2X_4,  IDR_PROFILE_AVATAR_2X_5,
    IDR_PROFILE_AVATAR_2X_6,  IDR_PROFILE_AVATAR_2X_7,
    IDR_PROFILE_AVATAR_2X_8,  IDR_PROFILE_AVATAR_2X_9,
    IDR_PROFILE_AVATAR_2X_10, IDR_PROFILE_AVATAR_2X_11,
    IDR_PROFILE_AVATAR_2X_12, IDR_PROFILE_AVATAR_2X_13,
    IDR_PROFILE_AVATAR_2X_14, IDR_PROFILE_AVATAR_2X_15,
    IDR_PROFILE_AVATAR_2X_16, IDR_PROFILE_AVATAR_2X_17,
    IDR_PROFILE_AVATAR_2X_18, IDR_PROFILE_AVATAR_2X_19,
    IDR_PROFILE_AVATAR_2X_20, IDR_PROFILE_AVATAR_2X_21,
    IDR_PROFILE_AVATAR_2X_22, IDR_PROFILE_AVATAR_2X_23,
    IDR_PROFILE_AVATAR_2X_24, IDR_PROFILE_AVATAR_2X_25,
    IDR_PROFILE_AVATAR_2X_26,
};

// Returns a copied SkBitmap for the given image that can be safely passed to
// another thread.
SkBitmap GetSkBitmapCopy(const gfx::Image& image) {
  DCHECK(!image.IsEmpty());
  const SkBitmap* image_bitmap = image.ToSkBitmap();
  SkBitmap bitmap_copy;
  if (bitmap_copy.tryAllocPixels(image_bitmap->info()))
    image_bitmap->readPixels(bitmap_copy.info(), bitmap_copy.getPixels(),
                             bitmap_copy.rowBytes(), 0, 0);
  return bitmap_copy;
}
#endif  // BUILDFLAG(IS_WIN)

// Determine what the scaled height of the avatar icon should be for a
// specified width, to preserve the aspect ratio.
int GetScaledAvatarHeightForWidth(int width, const gfx::ImageSkia& avatar) {
  // Multiply the width by the inverted aspect ratio (height over
  // width), and then add 0.5 to ensure the int truncation rounds nicely.
  int scaled_height = width *
      ((float) avatar.height() / (float) avatar.width()) + 0.5f;
  return scaled_height;
}

// A CanvasImageSource that draws a sized and positioned avatar with an
// optional border independently of the scale factor.
class AvatarImageSource : public gfx::CanvasImageSource {
 public:
  enum AvatarPosition {
    POSITION_CENTER,
    POSITION_BOTTOM_CENTER,
  };

  AvatarImageSource(gfx::ImageSkia avatar,
                    const gfx::Size& canvas_size,
                    int width,
                    AvatarPosition position,
                    profiles::AvatarShape shape);

  AvatarImageSource(gfx::ImageSkia avatar,
                    const gfx::Size& canvas_size,
                    int width,
                    AvatarPosition position);

  AvatarImageSource(const AvatarImageSource&) = delete;
  AvatarImageSource& operator=(const AvatarImageSource&) = delete;

  ~AvatarImageSource() override;

  // CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override;

 private:
  gfx::ImageSkia avatar_;
  const gfx::Size canvas_size_;
  const int width_;
  const int height_;
  const AvatarPosition position_;
  const profiles::AvatarShape shape_;
};

AvatarImageSource::AvatarImageSource(gfx::ImageSkia avatar,
                                     const gfx::Size& canvas_size,
                                     int width,
                                     AvatarPosition position,
                                     profiles::AvatarShape shape)
    : gfx::CanvasImageSource(canvas_size),
      canvas_size_(canvas_size),
      width_(width),
      height_(GetScaledAvatarHeightForWidth(width, avatar)),
      position_(position),
      shape_(shape) {
  avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(width_, height_));
}

AvatarImageSource::AvatarImageSource(gfx::ImageSkia avatar,
                                     const gfx::Size& canvas_size,
                                     int width,
                                     AvatarPosition position)
    : AvatarImageSource(avatar,
                        canvas_size,
                        width,
                        position,
                        profiles::SHAPE_SQUARE) {}

AvatarImageSource::~AvatarImageSource() {
}

void AvatarImageSource::Draw(gfx::Canvas* canvas) {
  // Center the avatar horizontally.
  int x = (canvas_size_.width() - width_) / 2;
  int y;

  if (position_ == POSITION_CENTER) {
    // Draw the avatar centered on the canvas.
    y = (canvas_size_.height() - height_) / 2;
  } else {
    // Draw the avatar on the bottom center of the canvas, leaving 1px below.
    y = canvas_size_.height() - height_ - 1;
  }

#if BUILDFLAG(IS_ANDROID)
  // Circular shape is only available on desktop platforms.
  DCHECK(shape_ != profiles::SHAPE_CIRCLE);
#else
  if (shape_ == profiles::SHAPE_CIRCLE) {
    // Draw the avatar on the bottom center of the canvas; overrides the
    // previous position specification to avoid leaving visible gap below the
    // avatar.
    y = canvas_size_.height() - height_;

    // Calculate the circular mask that will be used to display the avatar
    // image.
    SkPath circular_mask;
    circular_mask.addCircle(SkIntToScalar(canvas_size_.width() / 2),
                            SkIntToScalar(canvas_size_.height() / 2),
                            SkIntToScalar(canvas_size_.width() / 2));
    canvas->ClipPath(circular_mask, true);
  }
#endif

  canvas->DrawImageInt(avatar_, x, y);
}

class ImageWithBackgroundSource : public gfx::CanvasImageSource {
 public:
  ImageWithBackgroundSource(const gfx::ImageSkia& image, SkColor background)
      : gfx::CanvasImageSource(image.size()),
        image_(image),
        background_(background) {}

  ImageWithBackgroundSource(const ImageWithBackgroundSource&) = delete;
  ImageWithBackgroundSource& operator=(const ImageWithBackgroundSource&) =
      delete;

  ~ImageWithBackgroundSource() override = default;

  // gfx::CanvasImageSource override.
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawColor(background_);
    canvas->DrawImageInt(image_, 0, 0);
  }

 private:
  const gfx::ImageSkia image_;
  const SkColor background_;
};

// Returns icon with padding with no background.
const gfx::ImageSkia CreatePaddedIcon(const gfx::VectorIcon& icon,
                                      int size,
                                      SkColor color,
                                      float icon_to_image_ratio) {
  const int padding =
      static_cast<int>(size * (1.0f - icon_to_image_ratio) / 2.0f);

  const gfx::ImageSkia sized_icon =
      gfx::CreateVectorIcon(icon, size - 2 * padding, color);
  return gfx::CanvasImageSource::CreatePadded(sized_icon, gfx::Insets(padding));
}

// Returns a filled person avatar icon.
gfx::Image GetLegacyPlaceholderAvatarIconWithColors(SkColor fill_color,
                                                    SkColor stroke_color,
                                                    int size) {
  CHECK(!base::FeatureList::IsEnabled(kOutlineSilhouetteIcon));

  const gfx::VectorIcon& person_icon =
      size >= 40 ? kPersonFilledPaddedLargeIcon : kPersonFilledPaddedSmallIcon;
  const gfx::ImageSkia icon_without_background = gfx::CreateVectorIcon(
      gfx::IconDescription(person_icon, size, stroke_color));
  const gfx::ImageSkia icon_with_background(
      std::make_unique<ImageWithBackgroundSource>(icon_without_background,
                                                  fill_color),
      gfx::Size(size, size));
  return gfx::Image(icon_with_background);
}

}  // namespace

namespace profiles {

struct IconResourceInfo {
  int resource_id;
  const char* filename;
  int label_id;
};

constexpr int kAvatarIconSize = 96;
constexpr SkColor kAvatarTutorialBackgroundColor =
    SkColorSetRGB(0x42, 0x85, 0xf4);
constexpr SkColor kAvatarTutorialContentTextColor =
    SkColorSetRGB(0xc6, 0xda, 0xfc);
constexpr SkColor kAvatarBubbleAccountsBackgroundColor =
    SkColorSetRGB(0xf3, 0xf3, 0xf3);
constexpr SkColor kAvatarBubbleGaiaBackgroundColor =
    SkColorSetRGB(0xf5, 0xf5, 0xf5);
constexpr SkColor kUserManagerBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);

constexpr char kDefaultUrlPrefix[] = "chrome://theme/IDR_PROFILE_AVATAR_";
constexpr base::FilePath::CharType kGAIAPictureFileName[] =
    FILE_PATH_LITERAL("Google Profile Picture.png");
constexpr base::FilePath::CharType kHighResAvatarFolderName[] =
    FILE_PATH_LITERAL("Avatars");

// The size of the function-static kDefaultAvatarIconResources array below.
#if BUILDFLAG(IS_ANDROID)
constexpr size_t kDefaultAvatarIconsCount = 1;
#elif BUILDFLAG(IS_CHROMEOS_ASH)
constexpr size_t kDefaultAvatarIconsCount = 27;
#else
constexpr size_t kDefaultAvatarIconsCount = 56;
#endif

#if !BUILDFLAG(IS_ANDROID)
// The first 8 icons are generic.
constexpr size_t kGenericAvatarIconsCount = 8;
#else
constexpr size_t kGenericAvatarIconsCount = 0;
#endif

#if !BUILDFLAG(IS_ANDROID)
// The avatar used as a placeholder.
constexpr size_t kPlaceholderAvatarIndex = 26;
#else
constexpr size_t kPlaceholderAvatarIndex = 0;
#endif

ui::ImageModel GetGuestAvatar(int size) {
  return ui::ImageModel::FromVectorIcon(
      kUserAccountAvatarRefreshIcon,
      switches::IsExplicitBrowserSigninUIOnDesktopEnabled()
          ? ui::kColorMenuIcon
          : ui::kColorAvatarIconGuest,
      size);
}

gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              int width,
                              int height,
                              AvatarShape shape) {
  gfx::Size size(width, height);

  // Source for a centered, sized icon.
  std::unique_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(*image.ToImageSkia(), size, std::min(width, height),
                            AvatarImageSource::POSITION_CENTER, shape));

  return gfx::Image(gfx::ImageSkia(std::move(source), size));
}

gfx::Image GetSizedAvatarIcon(const gfx::Image& image, int width, int height) {
  return GetSizedAvatarIcon(image, width, height, profiles::SHAPE_SQUARE);
}

gfx::Image GetAvatarIconForWebUI(const gfx::Image& image) {
  return GetSizedAvatarIcon(image, kAvatarIconSize, kAvatarIconSize);
}

gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    int dst_width,
                                    int dst_height) {
  // The image requires no border or resizing.
  if (image.Height() <= kAvatarIconSize)
    return image;

  int size = std::min({kAvatarIconSize, dst_width, dst_height});
  gfx::Size dst_size(dst_width, dst_height);

  // Source for a sized icon drawn at the bottom center of the canvas,
  // with an etched border (for GAIA images).
  std::unique_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(*image.ToImageSkia(), dst_size, size,
                            AvatarImageSource::POSITION_BOTTOM_CENTER));

  return gfx::Image(gfx::ImageSkia(std::move(source), dst_size));
}

#if BUILDFLAG(IS_MAC)
gfx::Image GetAvatarIconForNSMenu(const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  if (!entry) {
    // This can happen if the user deletes the current profile.
    return gfx::Image();
  }

  PlaceholderAvatarIconParams icon_params =
      GetPlaceholderAvatarIconParamsVisibleAgainstColor(
          ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
              ? SK_ColorBLACK
              : SK_ColorWHITE);
  // Get a higher res than 16px so it looks good after cropping to a circle.
  gfx::Image icon = entry->GetAvatarIcon(
      kAvatarIconSize, /*download_high_res=*/false, icon_params);
  return profiles::GetSizedAvatarIcon(
      icon, kMenuAvatarIconSize, kMenuAvatarIconSize, profiles::SHAPE_CIRCLE);
}
#endif

// Helper methods for accessing, transforming and drawing avatar icons.
size_t GetDefaultAvatarIconCount() {
  return kDefaultAvatarIconsCount;
}

size_t GetGenericAvatarIconCount() {
  return kGenericAvatarIconsCount;
}

size_t GetPlaceholderAvatarIndex() {
  return kPlaceholderAvatarIndex;
}

size_t GetModernAvatarIconStartIndex() {
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
  return GetPlaceholderAvatarIndex() + 1;
#else
  // Only use the placeholder avatar on ChromeOS and Android.
  // TODO(crbug.com/41444689): Clean up code and remove code dependencies from
  // Android and ChromeOS. Avatar icons from this file are not used on these
  // platforms.
  return GetPlaceholderAvatarIndex();
#endif
}

bool IsModernAvatarIconIndex(size_t icon_index) {
  return icon_index >= GetModernAvatarIconStartIndex() &&
         icon_index < GetDefaultAvatarIconCount();
}

int GetPlaceholderAvatarIconResourceID() {
  // TODO(crbug.com/40138086): Replace with the new icon. Consider coloring the
  // icon (i.e. providing the image through
  // ProfileAttributesEntry::GetAvatarIcon(), instead) which would require more
  // refactoring.
  return IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE;
}

std::string GetPlaceholderAvatarIconUrl() {
  // TODO(crbug.com/40138086): Replace with the new icon. Consider coloring the
  // icon (i.e. providing the image through
  // ProfileAttributesEntry::GetAvatarIcon(), instead) which would require more
  // refactoring.
  return "chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE";
}

const IconResourceInfo* GetDefaultAvatarIconResourceInfo(size_t index) {
  CHECK_LT(index, kDefaultAvatarIconsCount);
  static const IconResourceInfo resource_info[kDefaultAvatarIconsCount] = {
  // Old avatar icons:
#if !BUILDFLAG(IS_ANDROID)
    {IDR_PROFILE_AVATAR_0, "avatar_generic.png", IDS_DEFAULT_AVATAR_LABEL_0},
    {IDR_PROFILE_AVATAR_1, "avatar_generic_aqua.png",
     IDS_DEFAULT_AVATAR_LABEL_1},
    {IDR_PROFILE_AVATAR_2, "avatar_generic_blue.png",
     IDS_DEFAULT_AVATAR_LABEL_2},
    {IDR_PROFILE_AVATAR_3, "avatar_generic_green.png",
     IDS_DEFAULT_AVATAR_LABEL_3},
    {IDR_PROFILE_AVATAR_4, "avatar_generic_orange.png",
     IDS_DEFAULT_AVATAR_LABEL_4},
    {IDR_PROFILE_AVATAR_5, "avatar_generic_purple.png",
     IDS_DEFAULT_AVATAR_LABEL_5},
    {IDR_PROFILE_AVATAR_6, "avatar_generic_red.png",
     IDS_DEFAULT_AVATAR_LABEL_6},
    {IDR_PROFILE_AVATAR_7, "avatar_generic_yellow.png",
     IDS_DEFAULT_AVATAR_LABEL_7},
    {IDR_PROFILE_AVATAR_8, "avatar_secret_agent.png",
     IDS_DEFAULT_AVATAR_LABEL_8},
    {IDR_PROFILE_AVATAR_9, "avatar_superhero.png", IDS_DEFAULT_AVATAR_LABEL_9},
    {IDR_PROFILE_AVATAR_10, "avatar_volley_ball.png",
     IDS_DEFAULT_AVATAR_LABEL_10},
    {IDR_PROFILE_AVATAR_11, "avatar_businessman.png",
     IDS_DEFAULT_AVATAR_LABEL_11},
    {IDR_PROFILE_AVATAR_12, "avatar_ninja.png", IDS_DEFAULT_AVATAR_LABEL_12},
    {IDR_PROFILE_AVATAR_13, "avatar_alien.png", IDS_DEFAULT_AVATAR_LABEL_13},
    {IDR_PROFILE_AVATAR_14, "avatar_awesome.png", IDS_DEFAULT_AVATAR_LABEL_14},
    {IDR_PROFILE_AVATAR_15, "avatar_flower.png", IDS_DEFAULT_AVATAR_LABEL_15},
    {IDR_PROFILE_AVATAR_16, "avatar_pizza.png", IDS_DEFAULT_AVATAR_LABEL_16},
    {IDR_PROFILE_AVATAR_17, "avatar_soccer.png", IDS_DEFAULT_AVATAR_LABEL_17},
    {IDR_PROFILE_AVATAR_18, "avatar_burger.png", IDS_DEFAULT_AVATAR_LABEL_18},
    {IDR_PROFILE_AVATAR_19, "avatar_cat.png", IDS_DEFAULT_AVATAR_LABEL_19},
    {IDR_PROFILE_AVATAR_20, "avatar_cupcake.png", IDS_DEFAULT_AVATAR_LABEL_20},
    {IDR_PROFILE_AVATAR_21, "avatar_dog.png", IDS_DEFAULT_AVATAR_LABEL_21},
    {IDR_PROFILE_AVATAR_22, "avatar_horse.png", IDS_DEFAULT_AVATAR_LABEL_22},
    {IDR_PROFILE_AVATAR_23, "avatar_margarita.png",
     IDS_DEFAULT_AVATAR_LABEL_23},
    {IDR_PROFILE_AVATAR_24, "avatar_note.png", IDS_DEFAULT_AVATAR_LABEL_24},
    {IDR_PROFILE_AVATAR_25, "avatar_sun_cloud.png",
     IDS_DEFAULT_AVATAR_LABEL_25},
#endif
    // Placeholder avatar icon:
    {IDR_PROFILE_AVATAR_26, nullptr, IDS_DEFAULT_AVATAR_LABEL_26},

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
    // Modern avatar icons:
    {IDR_PROFILE_AVATAR_27, "avatar_origami_cat.png",
     IDS_DEFAULT_AVATAR_LABEL_27},
    {IDR_PROFILE_AVATAR_28, "avatar_origami_corgi.png",
     IDS_DEFAULT_AVATAR_LABEL_28},
    {IDR_PROFILE_AVATAR_29, "avatar_origami_dragon.png",
     IDS_DEFAULT_AVATAR_LABEL_29},
    {IDR_PROFILE_AVATAR_30, "avatar_origami_elephant.png",
     IDS_DEFAULT_AVATAR_LABEL_30},
    {IDR_PROFILE_AVATAR_31, "avatar_origami_fox.png",
     IDS_DEFAULT_AVATAR_LABEL_31},
    {IDR_PROFILE_AVATAR_32, "avatar_origami_monkey.png",
     IDS_DEFAULT_AVATAR_LABEL_32},
    {IDR_PROFILE_AVATAR_33, "avatar_origami_panda.png",
     IDS_DEFAULT_AVATAR_LABEL_33},
    {IDR_PROFILE_AVATAR_34, "avatar_origami_penguin.png",
     IDS_DEFAULT_AVATAR_LABEL_34},
    {IDR_PROFILE_AVATAR_35, "avatar_origami_pinkbutterfly.png",
     IDS_DEFAULT_AVATAR_LABEL_35},
    {IDR_PROFILE_AVATAR_36, "avatar_origami_rabbit.png",
     IDS_DEFAULT_AVATAR_LABEL_36},
    {IDR_PROFILE_AVATAR_37, "avatar_origami_unicorn.png",
     IDS_DEFAULT_AVATAR_LABEL_37},
    {IDR_PROFILE_AVATAR_38, "avatar_illustration_basketball.png",
     IDS_DEFAULT_AVATAR_LABEL_38},
    {IDR_PROFILE_AVATAR_39, "avatar_illustration_bike.png",
     IDS_DEFAULT_AVATAR_LABEL_39},
    {IDR_PROFILE_AVATAR_40, "avatar_illustration_bird.png",
     IDS_DEFAULT_AVATAR_LABEL_40},
    {IDR_PROFILE_AVATAR_41, "avatar_illustration_cheese.png",
     IDS_DEFAULT_AVATAR_LABEL_41},
    {IDR_PROFILE_AVATAR_42, "avatar_illustration_football.png",
     IDS_DEFAULT_AVATAR_LABEL_42},
    {IDR_PROFILE_AVATAR_43, "avatar_illustration_ramen.png",
     IDS_DEFAULT_AVATAR_LABEL_43},
    {IDR_PROFILE_AVATAR_44, "avatar_illustration_sunglasses.png",
     IDS_DEFAULT_AVATAR_LABEL_44},
    {IDR_PROFILE_AVATAR_45, "avatar_illustration_sushi.png",
     IDS_DEFAULT_AVATAR_LABEL_45},
    {IDR_PROFILE_AVATAR_46, "avatar_illustration_tamagotchi.png",
     IDS_DEFAULT_AVATAR_LABEL_46},
    {IDR_PROFILE_AVATAR_47, "avatar_illustration_vinyl.png",
     IDS_DEFAULT_AVATAR_LABEL_47},
    {IDR_PROFILE_AVATAR_48, "avatar_abstract_avocado.png",
     IDS_DEFAULT_AVATAR_LABEL_48},
    {IDR_PROFILE_AVATAR_49, "avatar_abstract_cappuccino.png",
     IDS_DEFAULT_AVATAR_LABEL_49},
    {IDR_PROFILE_AVATAR_50, "avatar_abstract_icecream.png",
     IDS_DEFAULT_AVATAR_LABEL_50},
    {IDR_PROFILE_AVATAR_51, "avatar_abstract_icewater.png",
     IDS_DEFAULT_AVATAR_LABEL_51},
    {IDR_PROFILE_AVATAR_52, "avatar_abstract_melon.png",
     IDS_DEFAULT_AVATAR_LABEL_52},
    {IDR_PROFILE_AVATAR_53, "avatar_abstract_onigiri.png",
     IDS_DEFAULT_AVATAR_LABEL_53},
    {IDR_PROFILE_AVATAR_54, "avatar_abstract_pizza.png",
     IDS_DEFAULT_AVATAR_LABEL_54},
    {IDR_PROFILE_AVATAR_55, "avatar_abstract_sandwich.png",
     IDS_DEFAULT_AVATAR_LABEL_55},
#endif
  };
  return &resource_info[index];
}

gfx::Image GetPlaceholderAvatarIconVisibleAgainstBackground(
    SkColor profile_color_seed,
    int size,
    AvatarVisibilityAgainstBackground visibility) {
  CHECK(base::FeatureList::IsEnabled(kOutlineSilhouetteIcon));

  const gfx::VectorIcon& person_icon =
      vector_icons::kAccountCircleChromeRefreshIcon;

  // The palette is generated using the user color, which is independent of the
  // profile's light or dark theme.
  const ui::TonalPalette color_palette =
      ui::GeneratePalette(profile_color_seed,
                          ui::ColorProviderKey::SchemeVariant::kTonalSpot)
          ->primary();
  const SkColor visible_stroke_color =
      visibility == AvatarVisibilityAgainstBackground::kVisibleAgainstDarkTheme
          ? color_palette.get(kIconToneLight)
          : color_palette.get(kIconToneDark);

  const gfx::ImageSkia icon_without_background = gfx::CreateVectorIcon(
      gfx::IconDescription(person_icon, size, visible_stroke_color));
  return gfx::Image(icon_without_background);
}

gfx::Image GetPlaceholderAvatarIconWithColors(
    SkColor fill_color,
    SkColor stroke_color,
    int size,
    const PlaceholderAvatarIconParams& icon_params) {
  if (!base::FeatureList::IsEnabled(kOutlineSilhouetteIcon)) {
    return GetLegacyPlaceholderAvatarIconWithColors(fill_color, stroke_color,
                                                    size);
  }

  // If the icon should be an outline icon visible against the background, use
  // `GetPlaceholderAvatarIconVisibleAgainstBackground()` instead.
  CHECK(!icon_params.visibility_against_background.has_value());

  const gfx::VectorIcon& person_icon =
      vector_icons::kAccountCircleChromeRefreshIcon;

  const gfx::ImageSkia avatar_icon_without_background =
      icon_params.has_padding
          ? CreatePaddedIcon(person_icon, size, stroke_color, 0.5f)
          : gfx::CreateVectorIcon(
                gfx::IconDescription(person_icon, size, stroke_color));

  if (icon_params.has_background) {
    return gfx::Image(
        gfx::ImageSkia(std::make_unique<ImageWithBackgroundSource>(
                           avatar_icon_without_background, fill_color),
                       gfx::Size(size, size)));
  } else {
    return gfx::Image(avatar_icon_without_background);
  }
}

int GetDefaultAvatarIconResourceIDAtIndex(size_t index) {
  return GetDefaultAvatarIconResourceInfo(index)->resource_id;
}

#if BUILDFLAG(IS_WIN)
int GetOldDefaultAvatar2xIconResourceIDAtIndex(size_t index) {
  DCHECK_LT(index, std::size(kProfileAvatarIconResources2x));
  return kProfileAvatarIconResources2x[index];
}
#endif  // BUILDFLAG(IS_WIN)

const char* GetDefaultAvatarIconFileNameAtIndex(size_t index) {
  CHECK_NE(index, kPlaceholderAvatarIndex);
  return GetDefaultAvatarIconResourceInfo(index)->filename;
}

base::FilePath GetPathOfHighResAvatarAtIndex(size_t index) {
  const char* file_name = GetDefaultAvatarIconFileNameAtIndex(index);
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  return user_data_dir.Append(kHighResAvatarFolderName).AppendASCII(file_name);
}

std::string GetDefaultAvatarIconUrl(size_t index) {
#if !BUILDFLAG(IS_ANDROID)
  CHECK(IsDefaultAvatarIconIndex(index));
#endif
  return base::StringPrintf("%s%" PRIuS, kDefaultUrlPrefix, index);
}

int GetDefaultAvatarLabelResourceIDAtIndex(size_t index) {
  return GetDefaultAvatarIconResourceInfo(index)->label_id;
}

bool IsDefaultAvatarIconIndex(size_t index) {
  return index < kDefaultAvatarIconsCount;
}

bool IsDefaultAvatarIconUrl(const std::string& url, size_t* icon_index) {
  DCHECK(icon_index);
  if (!base::StartsWith(url, kDefaultUrlPrefix, base::CompareCase::SENSITIVE))
    return false;

  int int_value = -1;
  if (base::StringToInt(base::MakeStringPiece(
                            url.begin() + strlen(kDefaultUrlPrefix), url.end()),
                        &int_value)) {
    if (int_value < 0 ||
        int_value >= static_cast<int>(kDefaultAvatarIconsCount))
      return false;
    *icon_index = int_value;
    return true;
  }

  return false;
}

base::Value::Dict GetAvatarIconAndLabelDict(const std::string& url,
                                            const std::u16string& label,
                                            size_t index,
                                            bool selected,
                                            bool is_gaia_avatar) {
  base::Value::Dict avatar_info;
  avatar_info.Set("url", url);
  avatar_info.Set("label", label);
  avatar_info.Set("index", static_cast<int>(index));
  avatar_info.Set("selected", selected);
  avatar_info.Set("isGaiaAvatar", is_gaia_avatar);
  return avatar_info;
}

base::Value::Dict GetDefaultProfileAvatarIconAndLabel(SkColor fill_color,
                                                      SkColor stroke_color,
                                                      bool selected) {
  gfx::Image icon = profiles::GetPlaceholderAvatarIconWithColors(
      fill_color, stroke_color, kAvatarIconSize);
  size_t index = profiles::GetPlaceholderAvatarIndex();
  return GetAvatarIconAndLabelDict(
      webui::GetBitmapDataUrl(icon.AsBitmap()),
      l10n_util::GetStringUTF16(
          profiles::GetDefaultAvatarLabelResourceIDAtIndex(index)),
      index, selected, /*is_gaia_avatar=*/false);
}

base::Value::List GetCustomProfileAvatarIconsAndLabels(
    size_t selected_avatar_idx) {
  base::Value::List avatars;

  for (size_t i = GetModernAvatarIconStartIndex();
       i < GetDefaultAvatarIconCount(); ++i) {
    avatars.Append(GetAvatarIconAndLabelDict(
        profiles::GetDefaultAvatarIconUrl(i),
        l10n_util::GetStringUTF16(
            profiles::GetDefaultAvatarLabelResourceIDAtIndex(i)),
        i, i == selected_avatar_idx, /*is_gaia_avatar=*/false));
  }
  return avatars;
}

size_t GetRandomAvatarIconIndex(
    const std::unordered_set<size_t>& used_icon_indices) {
  size_t interval_begin = GetModernAvatarIconStartIndex();
  size_t interval_end = GetDefaultAvatarIconCount();
  size_t interval_length = interval_end - interval_begin;

  size_t random_offset = base::RandInt(0, interval_length - 1);
  // Find the next unused index.
  for (size_t i = 0; i < interval_length; ++i) {
    size_t icon_index = interval_begin + (random_offset + i) % interval_length;
    if (used_icon_indices.count(icon_index) == 0u)
      return icon_index;
  }
  // All indices are used, so return a random one.
  return interval_begin + random_offset;
}

#if !BUILDFLAG(IS_ANDROID)
base::Value::List GetIconsAndLabelsForProfileAvatarSelector(
    const base::FilePath& profile_path) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile_path);
  DCHECK(entry);

  bool using_gaia = entry->IsUsingGAIAPicture();
  size_t selected_avatar_idx =
      using_gaia ? SIZE_MAX : entry->GetAvatarIconIndex();

  // Obtain a list of the modern avatar icons.
  base::Value::List avatars(
      GetCustomProfileAvatarIconsAndLabels(selected_avatar_idx));

  if (entry->GetSigninState() == SigninState::kNotSignedIn) {
    ProfileThemeColors colors = entry->GetProfileThemeColors();
    auto generic_avatar_info = GetDefaultProfileAvatarIconAndLabel(
        colors.default_avatar_fill_color, colors.default_avatar_stroke_color,
        selected_avatar_idx == GetPlaceholderAvatarIndex());
    avatars.Insert(avatars.begin(),
                   base::Value(std::move(generic_avatar_info)));
    return avatars;
  }

  // Add the GAIA picture to the beginning of the list if it is available.
  const gfx::Image* icon = entry->GetGAIAPicture();
  if (icon) {
    gfx::Image avatar_icon = GetAvatarIconForWebUI(*icon);
    auto gaia_picture_info = GetAvatarIconAndLabelDict(
        /*url=*/webui::GetBitmapDataUrl(avatar_icon.AsBitmap()),
        /*label=*/
        l10n_util::GetStringUTF16(IDS_SETTINGS_CHANGE_PICTURE_PROFILE_PHOTO),
        /*index=*/0, using_gaia, /*is_gaia_avatar=*/true);
    avatars.Insert(avatars.begin(), base::Value(std::move(gaia_picture_info)));
  }

  return avatars;
}
#endif  // !BUILDFLAG(IS_ANDROID)

void SetDefaultProfileAvatarIndex(Profile* profile, size_t avatar_icon_index) {
  CHECK(IsDefaultAvatarIconIndex(avatar_icon_index));

  PrefService* pref_service = profile->GetPrefs();
  pref_service->SetInteger(prefs::kProfileAvatarIndex, avatar_icon_index);
  pref_service->SetBoolean(prefs::kProfileUsingDefaultAvatar,
                           avatar_icon_index == GetPlaceholderAvatarIndex());
  pref_service->SetBoolean(prefs::kProfileUsingGAIAAvatar, false);

  ProfileMetrics::LogProfileAvatarSelection(avatar_icon_index);
}

#if BUILDFLAG(IS_WIN)
SkBitmap GetWin2xAvatarImage(ProfileAttributesEntry* entry) {
  // Request just one size large enough for all uses.
  return GetSkBitmapCopy(
      entry->GetAvatarIcon(IconUtil::kLargeIconSize, /*use_high_res_file=*/true,
                           /*icon_params=*/{.has_padding = false}));
}

SkBitmap GetWin2xAvatarIconAsSquare(const SkBitmap& source_bitmap) {
  constexpr int kIconScaleFactor = 2;
  if ((source_bitmap.width() != kIconScaleFactor * kOldAvatarIconWidth) ||
      (source_bitmap.height() != kIconScaleFactor * kOldAvatarIconHeight)) {
    // It's not an old avatar icon, the image should be square.
    DCHECK_EQ(source_bitmap.width(), source_bitmap.height());
    return source_bitmap;
  }

  // If |source_bitmap| matches the old avatar icon dimensions, i.e. it's an
  // old avatar icon, shave a couple of columns so the |source_bitmap| is more
  // square. So when resized to a square aspect ratio it looks pretty.
  gfx::Rect frame(gfx::SkIRectToRect(source_bitmap.bounds()));
  frame.Inset(
      gfx::Insets::VH(/*vertical=*/0, /*horizontal=*/kIconScaleFactor * 2));
  SkBitmap cropped_bitmap;
  source_bitmap.extractSubset(&cropped_bitmap, gfx::RectToSkIRect(frame));
  return cropped_bitmap;
}

SkBitmap GetBadgedWinIconBitmapForAvatar(const SkBitmap& app_icon_bitmap,
                                         const SkBitmap& avatar_bitmap) {
  // TODO(dfried): This function often doesn't actually do the thing it claims
  // to. We should probably fix it.
  SkBitmap source_bitmap = profiles::GetWin2xAvatarIconAsSquare(avatar_bitmap);

  int avatar_badge_width = kProfileAvatarBadgeSizeWin;
  if (app_icon_bitmap.width() != kShortcutIconSizeWin) {
    avatar_badge_width = std::ceilf(
        app_icon_bitmap.width() *
        (float{kProfileAvatarBadgeSizeWin} / float{kShortcutIconSizeWin}));
  }

  // Resize the avatar image down to the desired badge size, maintaining aspect
  // ratio (but prefer more square than rectangular when rounding).
  const int avatar_badge_height = base::ClampCeil(
      avatar_badge_width *
      (static_cast<float>(source_bitmap.height()) / source_bitmap.width()));
  SkBitmap sk_icon = skia::ImageOperations::Resize(
      source_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      avatar_badge_width, avatar_badge_height);

  // Sanity check - avatars shouldn't be taller than they are wide.
  DCHECK_GE(sk_icon.width(), sk_icon.height());

  // Overlay the avatar on the icon, anchoring it to the bottom-right of the
  // icon on Win 10 and earlier, and the top right on Win 11. Win 11 moved the
  // taskbar icon badge to the top right from the bottom right, so profile
  // badging needs to move as well, to avoid double badging.
  SkBitmap badged_bitmap;
  badged_bitmap.allocN32Pixels(app_icon_bitmap.width(),
                               app_icon_bitmap.height());
  SkCanvas offscreen_canvas(badged_bitmap, SkSurfaceProps{});
  offscreen_canvas.clear(SK_ColorTRANSPARENT);
  offscreen_canvas.drawImage(app_icon_bitmap.asImage(), 0, 0);

  // Render the avatar in a cutout circle. If the avatar is not square, center
  // it in the circle but favor pushing it further down.
  const int cutout_size = avatar_badge_width;
  const int cutout_left = app_icon_bitmap.width() - cutout_size;
  const int cutout_top = base::win::GetVersion() >= base::win::Version::WIN11
                             ? 0
                             : app_icon_bitmap.height() - cutout_size;
  const int icon_left = cutout_left;

  const int icon_top =
      cutout_top + base::ClampCeil((cutout_size - avatar_badge_height) / 2.0f);
  const SkRRect clip_circle = SkRRect::MakeOval(
      SkRect::MakeXYWH(cutout_left, cutout_top, cutout_size, cutout_size));

  offscreen_canvas.clipRRect(clip_circle, true);
  offscreen_canvas.drawImage(sk_icon.asImage(), icon_left, icon_top);
  return badged_bitmap;
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace profiles
