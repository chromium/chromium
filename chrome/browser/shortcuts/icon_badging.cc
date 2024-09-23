// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/icon_badging.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/containers/enum_set.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/version_info/channel.h"
#include "build/branding_buildflags.h"
#include "build/buildflag.h"
#include "chrome/common/channel_info.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_family.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace shortcuts {

namespace {

enum class ShortcutSize {
  k16,
  k32,
  k48,
  k128,
  k256,
  k512,
  kMaxValue = k512
};

enum class BadgeSize {
  k8,
  k16,
  k24,
  k64,
  k128,
  kMaxValue = k128
};

// Returns the icon sizes needed for shortcut creation on the desktop. k32 is
// needed for the icon in the Create Shortcut dialog.
#if BUILDFLAG(IS_MAC)
constexpr ShortcutSize kSizesNeededForShortcutCreation[] = {
    ShortcutSize::k16, ShortcutSize::k32, ShortcutSize::k128,
    ShortcutSize::k256, ShortcutSize::k512};
#elif BUILDFLAG(IS_LINUX)
constexpr ShortcutSize kSizesNeededForShortcutCreation[] = {ShortcutSize::k32,
                                                            ShortcutSize::k128};
#elif BUILDFLAG(IS_WIN)
constexpr ShortcutSize kSizesNeededForShortcutCreation[] = {
    ShortcutSize::k16, ShortcutSize::k32, ShortcutSize::k48,
    ShortcutSize::k256};
#endif

int ToInt(ShortcutSize size) {
  switch (size) {
    case ShortcutSize::k16:
      return 16;
    case ShortcutSize::k32:
      return 32;
    case ShortcutSize::k48:
      return 48;
    case ShortcutSize::k128:
      return 128;
    case ShortcutSize::k256:
      return 256;
    case ShortcutSize::k512:
      return 512;
  }
}

int ToInt(BadgeSize size) {
  switch (size) {
    case BadgeSize::k8:
      return 8;
    case BadgeSize::k16:
      return 16;
    case BadgeSize::k24:
      return 24;
    case BadgeSize::k64:
      return 64;
    case BadgeSize::k128:
      return 128;
  }
}

// For icon sizes > 128, use a badge size that is 1/4, else use a badge size
// that is 1/2.
BadgeSize GetBadgeSizeFromShortcutSize(ShortcutSize icon_size) {
  switch (icon_size) {
    case ShortcutSize::k16:
      return BadgeSize::k8;
    case ShortcutSize::k32:
      return BadgeSize::k16;
    case ShortcutSize::k48:
      return BadgeSize::k24;
    case ShortcutSize::k128:
      return BadgeSize::k64;
    case ShortcutSize::k256:
      return BadgeSize::k64;
    case ShortcutSize::k512:
      return BadgeSize::k128;
  }
}

constexpr int resource_map_size = static_cast<int>(BadgeSize::kMaxValue) + 1;

using SizeToResourceMap = base::fixed_flat_map<BadgeSize, int, 5>;

// At very low pixels < 16, there is probably no need to recreate the
// resource files necessary for these icons. Resizing them instead would
// work just as better, without the need to increase the chrome binary
// size.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr SizeToResourceMap kStableResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_16_STABLE_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_16_STABLE_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_24_STABLE_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_64_STABLE_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_128_STABLE_SHORTCUTS}});
constexpr SizeToResourceMap kCanaryResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_CANARY_16_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_CANARY_16_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_CANARY_24_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_CANARY_64_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_CANARY_128_SHORTCUTS}});
constexpr SizeToResourceMap kBetaResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_BETA_16_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_BETA_16_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_BETA_24_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_BETA_64_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_BETA_128_SHORTCUTS}});
constexpr SizeToResourceMap kDevResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_DEV_16_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_DEV_16_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_DEV_24_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_DEV_64_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_DEV_128_SHORTCUTS}});

static_assert(static_cast<int>(kStableResourceMap.size()) == resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for stable resource map");
static_assert(static_cast<int>(kCanaryResourceMap.size()) == resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for canary resource map");
static_assert(static_cast<int>(kBetaResourceMap.size()) == resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for beta resource map");
static_assert(static_cast<int>(kDevResourceMap.size()) == resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for dev resource map");

#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
constexpr SizeToResourceMap kChromeForTestingBrandedResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_16_CFT_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_16_CFT_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_24_CFT_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_64_CFT_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_128_CFT_SHORTCUTS}});

static_assert(static_cast<int>(kChromeForTestingBrandedResourceMap.size()) ==
                  resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for Chrome for testing resource map");
#else
constexpr SizeToResourceMap kChromiumResourceMap =
    base::MakeFixedFlatMap<BadgeSize, int>(
        {{BadgeSize::k8, IDR_PRODUCT_LOGO_16_SHORTCUTS},
         {BadgeSize::k16, IDR_PRODUCT_LOGO_16_SHORTCUTS},
         {BadgeSize::k24, IDR_PRODUCT_LOGO_24_SHORTCUTS},
         {BadgeSize::k64, IDR_PRODUCT_LOGO_64_SHORTCUTS},
         {BadgeSize::k128, IDR_PRODUCT_LOGO_128_SHORTCUTS}});

static_assert(static_cast<int>(kChromiumResourceMap.size()) ==
                  resource_map_size,
              "Ensure icon resources are filled for all entries in BadgeSize "
              "for chromium resource map");
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

SizeToResourceMap GetResourceMapForCurrentChannel() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (chrome::GetChannel()) {
    // |version_info::Channel::DEFAULT| is seen on local builds with
    // is_chrome_branded = true.
    case version_info::Channel::DEFAULT:
    case version_info::Channel::STABLE:
      return kStableResourceMap;
    case version_info::Channel::CANARY:
      return kCanaryResourceMap;
    case version_info::Channel::DEV:
      return kDevResourceMap;
    case version_info::Channel::BETA:
      return kBetaResourceMap;
  }
#elif BUILDFLAG(GOOGLE_CHROME_FOR_TESTING_BRANDING)
  return kChromeForTestingBrandedResourceMap;
#else
  return kChromiumResourceMap;
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

gfx::ImageSkia GetMaskForBadging(ShortcutSize icon_size) {
  BadgeSize badge_size = GetBadgeSizeFromShortcutSize(icon_size);

  gfx::ImageSkia* image =
      ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
          GetResourceMapForCurrentChannel().at(badge_size));
  CHECK(image);

  // TODO(crbug.com/337998776): Update if assets are sent by UX.
  const float kWhiteMaskFactor = 1.75f;
  // Set white mask for the loaded icon.
  gfx::ImageSkia masked_badge =
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          image->width() / kWhiteMaskFactor, SK_ColorWHITE, *image);

  int badge_size_int = ToInt(badge_size);
  // Maintain the badging ratio of 1/4th for sizes < 128, and 1/8th
  // for sizes greater. Adding the white background after loading the resource
  // will likely throw that ratio off.
  gfx::ImageSkia resized_post_masking =
      gfx::ImageSkiaOperations::CreateResizedImage(
          masked_badge, skia::ImageOperations::ResizeMethod::RESIZE_LANCZOS3,
          gfx::Size(badge_size_int, badge_size_int));

  return resized_post_masking;
}

int FindClosestIconSizeToUse(const std::vector<int>& sorted_icons,
                             int size_to_use) {
  auto closest_size_iter =
      std::lower_bound(sorted_icons.begin(), sorted_icons.end(), size_to_use);

  // Iterator out of bounds, can happen if the input icon size is too large,
  // like say 1000.
  if (closest_size_iter == sorted_icons.end()) {
    closest_size_iter = sorted_icons.end() - 1;
  } else if (closest_size_iter != sorted_icons.begin()) {
    // Compare with the iterator found and the one just before, and choose the
    // size that is closer. This helps reduce the pixelation when the icon gets
    // resized ultimately.
    int size_before = *(closest_size_iter - 1);
    int size_after = *(closest_size_iter);
    if (std::fabs(size_to_use - size_before) <
        std::fabs(size_to_use - size_after)) {
      --closest_size_iter;
    }
  }

  return *closest_size_iter;
}

using ShortcutSizes =
    base::EnumSet<ShortcutSize, ShortcutSize::k16, ShortcutSize::k512>;
}  // namespace

gfx::ImageFamily ApplyProductLogoBadgeToIcons(std::vector<SkBitmap> icons) {
  gfx::ImageFamily badged_icons;
  CHECK(!icons.empty());

  base::flat_map<int, SkBitmap> sorted_icons;
  std::vector<int> icon_sizes;
  for (const auto& icon : icons) {
    sorted_icons.insert_or_assign(icon.width(), icon);
    icon_sizes.push_back(icon.width());
  }

  std::sort(icon_sizes.begin(), icon_sizes.end());

  for (const ShortcutSize needed_size : kSizesNeededForShortcutCreation) {
    int icon_size = ToInt(needed_size);
    int size_to_use = FindClosestIconSizeToUse(icon_sizes, icon_size);
    SkBitmap bitmap_to_use = sorted_icons.at(size_to_use);

    // Resize the current bitmap to the needed_size, so that the properly
    // selected product logo does not get pixellated.
    gfx::ImageSkia resized_to_fit =
        gfx::ImageSkiaOperations::CreateResizedImage(
            gfx::ImageSkia::CreateFrom1xBitmap(bitmap_to_use),
            skia::ImageOperations::ResizeMethod::RESIZE_BEST,
            gfx::Size(icon_size, icon_size));

    gfx::ImageSkia masked_badge = GetMaskForBadging(needed_size);

    // Apply the masked product logo to the bitmaps.
    gfx::ImageSkia badged_icon = gfx::ImageSkiaOperations::CreateIconWithBadge(
        resized_to_fit, masked_badge);

    // Doing this allows the returned gfx::ImageFamily to be passed across
    // multiple sequences, like when this is passed to the ThreadPool during
    // shortcut creation at the OS level.
    badged_icon.MakeThreadSafe();

    badged_icons.Add(badged_icon);
  }
  return badged_icons;
}

}  // namespace shortcuts
