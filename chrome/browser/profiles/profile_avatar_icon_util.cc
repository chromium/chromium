// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_avatar_icon_util.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "url/url_canon.h"

#if defined(OS_WIN)
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "ui/gfx/icon_util.h"  // For Iconutil::kLargeIconSize.
#endif

// Helper methods for transforming and drawing avatar icons.
namespace {

#if defined(OS_WIN)
// 2x sized versions of the old profile avatar icons.
// TODO(crbug.com/937834): Clean this up.
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

// Returns a copied SkBitmap for the given resource id that can be safely passed
// to another thread.
SkBitmap GetImageResourceSkBitmapCopy(int resource_id) {
  const gfx::Image image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
  return GetSkBitmapCopy(image);
}
#endif  // OS_WIN

const int kOldAvatarIconWidth = 38;
const int kOldAvatarIconHeight = 31;

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

  enum AvatarBorder {
    BORDER_NONE,
    BORDER_NORMAL,
    BORDER_ETCHED,
  };

  AvatarImageSource(gfx::ImageSkia avatar,
                    const gfx::Size& canvas_size,
                    int width,
                    AvatarPosition position,
                    AvatarBorder border,
                    profiles::AvatarShape shape);

  AvatarImageSource(gfx::ImageSkia avatar,
                    const gfx::Size& canvas_size,
                    int width,
                    AvatarPosition position,
                    AvatarBorder border);

  ~AvatarImageSource() override;

  // CanvasImageSource override:
  void Draw(gfx::Canvas* canvas) override;

 private:
  gfx::ImageSkia avatar_;
  const gfx::Size canvas_size_;
  const int width_;
  const int height_;
  const AvatarPosition position_;
  const AvatarBorder border_;
  const profiles::AvatarShape shape_;

  DISALLOW_COPY_AND_ASSIGN(AvatarImageSource);
};

AvatarImageSource::AvatarImageSource(gfx::ImageSkia avatar,
                                     const gfx::Size& canvas_size,
                                     int width,
                                     AvatarPosition position,
                                     AvatarBorder border,
                                     profiles::AvatarShape shape)
    : gfx::CanvasImageSource(canvas_size),
      canvas_size_(canvas_size),
      width_(width),
      height_(GetScaledAvatarHeightForWidth(width, avatar)),
      position_(position),
      border_(border),
      shape_(shape) {
  avatar_ = gfx::ImageSkiaOperations::CreateResizedImage(
      avatar, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(width_, height_));
}

AvatarImageSource::AvatarImageSource(gfx::ImageSkia avatar,
                                     const gfx::Size& canvas_size,
                                     int width,
                                     AvatarPosition position,
                                     AvatarBorder border)
    : AvatarImageSource(avatar,
                        canvas_size,
                        width,
                        position,
                        border,
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

#if defined(OS_ANDROID)
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

  // The border should be square.
  int border_size = std::max(width_, height_);
  // Reset the x and y for the square border.
  x = (canvas_size_.width() - border_size) / 2;
  y = (canvas_size_.height() - border_size) / 2;

  if (border_ == BORDER_NORMAL) {
    // Draw a gray border on the inside of the avatar.
    SkColor border_color = SkColorSetARGB(83, 0, 0, 0);

    // Offset the rectangle by a half pixel so the border is drawn within the
    // appropriate pixels no matter the scale factor. Subtract 1 from the right
    // and bottom sizes to specify the endpoints, yielding -0.5.
    SkPath path;
    path.addRect(SkFloatToScalar(x + 0.5f),  // left
                 SkFloatToScalar(y + 0.5f),  // top
                 SkFloatToScalar(x + border_size - 0.5f),   // right
                 SkFloatToScalar(y + border_size - 0.5f));  // bottom

    cc::PaintFlags flags;
    flags.setColor(border_color);
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(1));

    canvas->DrawPath(path, flags);
  } else if (border_ == BORDER_ETCHED) {
    // Give the avatar an etched look by drawing a highlight on the bottom and
    // right edges.
    SkColor shadow_color = SkColorSetARGB(83, 0, 0, 0);
    SkColor highlight_color = SkColorSetARGB(96, 255, 255, 255);

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(SkIntToScalar(1));

    SkPath path;

    // Left and top shadows. To support higher scale factors than 1, position
    // the orthogonal dimension of each line on the half-pixel to separate the
    // pixel. For a vertical line, this means adding 0.5 to the x-value.
    path.moveTo(SkFloatToScalar(x + 0.5f), SkIntToScalar(y + height_));

    // Draw up to the top-left. Stop with the y-value at a half-pixel.
    path.rLineTo(SkIntToScalar(0), SkFloatToScalar(-height_ + 0.5f));

    // Draw right to the top-right, stopping within the last pixel.
    path.rLineTo(SkFloatToScalar(width_ - 0.5f), SkIntToScalar(0));

    flags.setColor(shadow_color);
    canvas->DrawPath(path, flags);

    path.reset();

    // Bottom and right highlights. Note that the shadows own the shared corner
    // pixels, so reduce the sizes accordingly.
    path.moveTo(SkIntToScalar(x + 1), SkFloatToScalar(y + height_ - 0.5f));

    // Draw right to the bottom-right.
    path.rLineTo(SkFloatToScalar(width_ - 1.5f), SkIntToScalar(0));

    // Draw up to the top-right.
    path.rLineTo(SkIntToScalar(0), SkFloatToScalar(-height_ + 1.5f));

    flags.setColor(highlight_color);
    canvas->DrawPath(path, flags);
  }
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
constexpr char kGAIAPictureFileName[] = "Google Profile Picture.png";
constexpr char kHighResAvatarFolderName[] = "Avatars";

// The size of the function-static kDefaultAvatarIconResources array below.
#if defined(OS_ANDROID)
constexpr size_t kDefaultAvatarIconsCount = 1;
#elif defined(OS_CHROMEOS)
constexpr size_t kDefaultAvatarIconsCount = 27;
#else
constexpr size_t kDefaultAvatarIconsCount = 56;
#endif

#if !defined(OS_ANDROID)
// The first 8 icons are generic.
constexpr size_t kGenericAvatarIconsCount = 8;
#else
constexpr size_t kGenericAvatarIconsCount = 0;
#endif

#if !defined(OS_ANDROID)
// The avatar used as a placeholder.
constexpr size_t kPlaceholderAvatarIndex = 26;
#else
constexpr size_t kPlaceholderAvatarIndex = 0;
#endif

gfx::ImageSkia GetGuestAvatar(int size) {
  return gfx::CreateVectorIcon(kUserAccountAvatarIcon, size,
                               gfx::kGoogleGrey500);
}

gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height,
                              AvatarShape shape) {
  if (!is_rectangle && image.Height() <= height)
    return image;

  gfx::Size size(width, height);

  // Source for a centered, sized icon. GAIA images get a border.
  std::unique_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(*image.ToImageSkia(), size, std::min(width, height),
                            AvatarImageSource::POSITION_CENTER,
                            AvatarImageSource::BORDER_NONE, shape));

  return gfx::Image(gfx::ImageSkia(std::move(source), size));
}

gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height) {
  return GetSizedAvatarIcon(image, is_rectangle, width, height,
                            profiles::SHAPE_SQUARE);
}

gfx::Image GetAvatarIconForWebUI(const gfx::Image& image,
                                 bool is_rectangle) {
  return GetSizedAvatarIcon(image, is_rectangle, kAvatarIconSize,
                            kAvatarIconSize);
}

gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    bool is_gaia_image,
                                    int dst_width,
                                    int dst_height) {
  // The image requires no border or resizing.
  if (!is_gaia_image && image.Height() <= kAvatarIconSize)
    return image;

  int size = std::min({kAvatarIconSize, dst_width, dst_height});
  gfx::Size dst_size(dst_width, dst_height);

  // Source for a sized icon drawn at the bottom center of the canvas,
  // with an etched border (for GAIA images).
  std::unique_ptr<gfx::ImageSkiaSource> source(
      new AvatarImageSource(*image.ToImageSkia(), dst_size, size,
                            AvatarImageSource::POSITION_BOTTOM_CENTER,
                            is_gaia_image ? AvatarImageSource::BORDER_ETCHED
                                          : AvatarImageSource::BORDER_NONE));

  return gfx::Image(gfx::ImageSkia(std::move(source), dst_size));
}

#if defined(OS_MACOSX)
gfx::Image GetAvatarIconForNSMenu(const base::FilePath& profile_path) {
  // Always use the low-res, small default avatars in the menu.
  gfx::Image icon;
  AvatarMenu::GetImageForMenuButton(profile_path, &icon);

  // The image might be too large and need to be resized, e.g. if this is a
  // signed-in user using the GAIA profile photo.
  constexpr int kMenuAvatarIconSize = 38;
  if (icon.Width() > kMenuAvatarIconSize ||
      icon.Height() > kMenuAvatarIconSize) {
    icon = profiles::GetSizedAvatarIcon(
        icon, /*is_rectangle=*/true, kMenuAvatarIconSize, kMenuAvatarIconSize);
  }
  return icon;
}
#endif

SkBitmap GetAvatarIconAsSquare(const SkBitmap& source_bitmap,
                               int scale_factor) {
  SkBitmap square_bitmap;
  if ((source_bitmap.width() == scale_factor * kOldAvatarIconWidth) &&
      (source_bitmap.height() == scale_factor * kOldAvatarIconHeight)) {
    // If |source_bitmap| matches the old avatar icon dimensions, i.e. it's an
    // old avatar icon, shave a couple of columns so the |source_bitmap| is more
    // square. So when resized to a square aspect ratio it looks pretty.
    gfx::Rect frame(scale_factor * profiles::kAvatarIconSize,
                    scale_factor * profiles::kAvatarIconSize);
    frame.Inset(scale_factor * 2, 0, scale_factor * 2, 0);
    source_bitmap.extractSubset(&square_bitmap, gfx::RectToSkIRect(frame));
  } else {
    // If it's not an old avatar icon, the image should be square.
    DCHECK(source_bitmap.width() == source_bitmap.height());
    square_bitmap = source_bitmap;
  }
  return square_bitmap;
}

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
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return GetPlaceholderAvatarIndex() + 1;
#else
  // Only use the placeholder avatar on ChromeOS and Android.
  // TODO(crbug.com/937834): Clean up code and remove code dependencies from
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
  return IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE;
}

std::string GetPlaceholderAvatarIconUrl() {
  return "chrome://theme/IDR_PROFILE_AVATAR_PLACEHOLDER_LARGE";
}

const IconResourceInfo* GetDefaultAvatarIconResourceInfo(size_t index) {
  CHECK_LT(index, kDefaultAvatarIconsCount);
  static const IconResourceInfo resource_info[kDefaultAvatarIconsCount] = {
  // Old avatar icons:
#if !defined(OS_ANDROID)
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
    {IDR_PROFILE_AVATAR_26, NULL, -1},

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
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

int GetDefaultAvatarIconResourceIDAtIndex(size_t index) {
  return GetDefaultAvatarIconResourceInfo(index)->resource_id;
}

const char* GetDefaultAvatarIconFileNameAtIndex(size_t index) {
  CHECK_NE(index, kPlaceholderAvatarIndex);
  return GetDefaultAvatarIconResourceInfo(index)->filename;
}

base::FilePath GetPathOfHighResAvatarAtIndex(size_t index) {
  const char* file_name = GetDefaultAvatarIconFileNameAtIndex(index);
  base::FilePath user_data_dir;
  CHECK(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
  return user_data_dir.AppendASCII(
      kHighResAvatarFolderName).AppendASCII(file_name);
}

std::string GetDefaultAvatarIconUrl(size_t index) {
#if !defined(OS_ANDROID)
  CHECK(IsDefaultAvatarIconIndex(index));
#endif
  return base::StringPrintf("%s%" PRIuS, kDefaultUrlPrefix, index);
}

int GetDefaultAvatarLabelResourceIDAtIndex(size_t index) {
  CHECK_NE(index, kPlaceholderAvatarIndex);
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
  if (base::StringToInt(base::StringPiece(url.begin() +
                                          strlen(kDefaultUrlPrefix),
                                          url.end()),
                        &int_value)) {
    if (int_value < 0 ||
        int_value >= static_cast<int>(kDefaultAvatarIconsCount))
      return false;
    *icon_index = int_value;
    return true;
  }

  return false;
}

std::unique_ptr<base::ListValue> GetDefaultProfileAvatarIconsAndLabels(
    size_t selected_avatar_idx) {
  std::unique_ptr<base::ListValue> avatars(new base::ListValue());

  for (size_t i = GetModernAvatarIconStartIndex();
       i < GetDefaultAvatarIconCount(); ++i) {
    std::unique_ptr<base::DictionaryValue> avatar_info(
        new base::DictionaryValue());
    avatar_info->SetString("url", profiles::GetDefaultAvatarIconUrl(i));
    avatar_info->SetString(
        "label", l10n_util::GetStringUTF16(
                     profiles::GetDefaultAvatarLabelResourceIDAtIndex(i)));
    if (i == selected_avatar_idx)
      avatar_info->SetBoolean("selected", true);

    avatars->Append(std::move(avatar_info));
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

#if defined(OS_WIN)
void GetWinAvatarImages(ProfileAttributesEntry* entry,
                        SkBitmap* avatar_image_1x,
                        SkBitmap* avatar_image_2x) {
  // The profile might be using the Gaia avatar, which is not in the
  // resources array.
  if (entry->IsUsingGAIAPicture()) {
    const gfx::Image* image = entry->GetGAIAPicture();
    if (image) {
      *avatar_image_1x = GetSkBitmapCopy(*image);
      // Gaia images are 256px, which makes them big enough to use in the
      // large icon case as well.
      DCHECK_GE(image->Width(), IconUtil::kLargeIconSize);
      *avatar_image_2x = *avatar_image_1x;
      return;
    }
  }

  // If the profile isn't using a Gaia image, or if the Gaia image did not
  // exist, revert to the previously used avatar icon.
  const size_t icon_index = entry->GetAvatarIconIndex();
  *avatar_image_1x = GetImageResourceSkBitmapCopy(
      profiles::GetDefaultAvatarIconResourceIDAtIndex(icon_index));

  if (profiles::IsModernAvatarIconIndex(icon_index)) {
    // Modern avatars are large(192px) by default, which makes them big
    // enough for 2x.
    *avatar_image_2x = *avatar_image_1x;
  } else {
    *avatar_image_2x =
        GetImageResourceSkBitmapCopy(kProfileAvatarIconResources2x[icon_index]);
  }
}

SkBitmap GetBadgedWinIconBitmapForAvatar(const SkBitmap& app_icon_bitmap,
                                         const SkBitmap& avatar_bitmap,
                                         int scale_factor) {
  // TODO(dfried): This function often doesn't actually do the thing it claims
  // to. We should probably fix it.
  SkBitmap source_bitmap =
      profiles::GetAvatarIconAsSquare(avatar_bitmap, scale_factor);

  int avatar_badge_width = kProfileAvatarBadgeSizeWin;
  if (app_icon_bitmap.width() != kShortcutIconSizeWin) {
    avatar_badge_width = std::ceilf(
        app_icon_bitmap.width() *
        (float{kProfileAvatarBadgeSizeWin} / float{kShortcutIconSizeWin}));
  }

  // Resize the avatar image down to the desired badge size, maintaining aspect
  // ratio (but prefer more square than rectangular when rounding).
  const int avatar_badge_height =
      std::ceilf(avatar_badge_width * (float{source_bitmap.height()} /
                                       float{source_bitmap.width()}));
  SkBitmap sk_icon = skia::ImageOperations::Resize(
      source_bitmap, skia::ImageOperations::RESIZE_LANCZOS3,
      avatar_badge_height, avatar_badge_width);

  // Sanity check - avatars shouldn't be taller than they are wide.
  DCHECK_GE(avatar_badge_width, avatar_badge_height);

  // Overlay the avatar on the icon, anchoring it to the bottom-right of the
  // icon.
  SkBitmap badged_bitmap;
  badged_bitmap.allocN32Pixels(app_icon_bitmap.width(),
                               app_icon_bitmap.height());
  SkCanvas offscreen_canvas(badged_bitmap);
  offscreen_canvas.clear(SK_ColorTRANSPARENT);
  offscreen_canvas.drawBitmap(app_icon_bitmap, 0, 0);

  // Render the avatar in a cutout circle. If the avatar is not square, center
  // it in the circle but favor pushing it further down.
  const int cutout_size = avatar_badge_width;
  const int cutout_left = app_icon_bitmap.width() - cutout_size;
  const int cutout_top = app_icon_bitmap.height() - cutout_size;
  const int icon_left = cutout_left;
  const int icon_top =
      cutout_top + int{std::ceilf((cutout_size - avatar_badge_height) / 2.0f)};
  const SkRRect clip_circle = SkRRect::MakeOval(
      SkRect::MakeXYWH(cutout_left, cutout_top, cutout_size, cutout_size));

  offscreen_canvas.clipRRect(clip_circle, true);
  offscreen_canvas.drawBitmap(sk_icon, icon_left, icon_top);
  return badged_bitmap;
}
#endif  // OS_WIN

}  // namespace profiles
