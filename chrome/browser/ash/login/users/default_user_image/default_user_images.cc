// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/default_user_image.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {
namespace default_user_image {
namespace {

struct DefaultImageInfo {
  // Resource IDs of default user images.
  const int resource_id;

  // Message IDs of default user image descriptions.
  const int description_message_id;

  // Whether the user image is eligible in the current set. If so, user can
  // select the image as avatar through personalization settings.
  Eligibility eligibility;

  // A path of the default user image, will be used to generate gstatic URLs
  // and to cache the user image locally on disk.
  const char* path;
};

// Info of default user images. When adding new entries to this list,
// please also update the enum ChromeOSUserImageId2 in
// tools/metrics/histograms/enums.xml
// When deprecating images, please also update kCurrentImageIndexes accordingly.
// clang-format off
constexpr DefaultImageInfo kDefaultImageInfo[] = {
    // No description for deprecated user image 0-18.
    {IDR_LOGIN_DEFAULT_USER, 0, Eligibility::kDeprecated, "legacy/avatar_anonymous.png"},
    // Original set of images.
    {IDR_LOGIN_DEFAULT_USER_1, 0, Eligibility::kDeprecated, "legacy/avatar_bee.png"},
    {IDR_LOGIN_DEFAULT_USER_2, 0, Eligibility::kDeprecated, "legacy/avatar_briefcase.png"},
    {IDR_LOGIN_DEFAULT_USER_3, 0, Eligibility::kDeprecated, "legacy/avatar_circles.png"},
    {IDR_LOGIN_DEFAULT_USER_4, 0, Eligibility::kDeprecated, "legacy/avatar_cloud.png"},
    {IDR_LOGIN_DEFAULT_USER_5, 0, Eligibility::kDeprecated, "legacy/avatar_cupcake.png"},
    {IDR_LOGIN_DEFAULT_USER_6, 0, Eligibility::kDeprecated, "legacy/avatar_day.png"},
    {IDR_LOGIN_DEFAULT_USER_7, 0, Eligibility::kDeprecated, "legacy/avatar_flower.png"},
    {IDR_LOGIN_DEFAULT_USER_8, 0, Eligibility::kDeprecated, "legacy/avatar_globe.png"},
    {IDR_LOGIN_DEFAULT_USER_9, 0, Eligibility::kDeprecated, "legacy/avatar_hotair.png"},
    {IDR_LOGIN_DEFAULT_USER_10, 0, Eligibility::kDeprecated, "legacy/avatar_ladybug.png"},
    {IDR_LOGIN_DEFAULT_USER_11, 0, Eligibility::kDeprecated, "legacy/avatar_leaf.png"},
    {IDR_LOGIN_DEFAULT_USER_12, 0, Eligibility::kDeprecated, "legacy/avatar_night.png"},
    {IDR_LOGIN_DEFAULT_USER_13, 0, Eligibility::kDeprecated, "legacy/avatar_plane.png"},
    {IDR_LOGIN_DEFAULT_USER_14, 0, Eligibility::kDeprecated, "legacy/avatar_robot_body.png"},
    {IDR_LOGIN_DEFAULT_USER_15, 0, Eligibility::kDeprecated, "legacy/avatar_robot_head.png"},
    {IDR_LOGIN_DEFAULT_USER_16, 0, Eligibility::kDeprecated, "legacy/avatar_toolbox.png"},
    {IDR_LOGIN_DEFAULT_USER_17, 0, Eligibility::kDeprecated, "legacy/avatar_user_color.png"},
    {IDR_LOGIN_DEFAULT_USER_18, 0, Eligibility::kDeprecated, "legacy/avatar_user_enterprise.png"},
    // Second set of images.
    {IDR_LOGIN_DEFAULT_USER_19, IDS_LOGIN_DEFAULT_USER_DESC_19, Eligibility::kDeprecated, "legacy/avatar_bicycle.png"},
    {IDR_LOGIN_DEFAULT_USER_20, IDS_LOGIN_DEFAULT_USER_DESC_20, Eligibility::kDeprecated, "legacy/avatar_bokeh.png"},
    {IDR_LOGIN_DEFAULT_USER_21, IDS_LOGIN_DEFAULT_USER_DESC_21, Eligibility::kDeprecated, "legacy/avatar_chess.png"},
    {IDR_LOGIN_DEFAULT_USER_22, IDS_LOGIN_DEFAULT_USER_DESC_22, Eligibility::kDeprecated, "legacy/avatar_coffee.png"},
    {IDR_LOGIN_DEFAULT_USER_23, IDS_LOGIN_DEFAULT_USER_DESC_23, Eligibility::kDeprecated, "legacy/avatar_dragonfly.png"},
    {IDR_LOGIN_DEFAULT_USER_24, IDS_LOGIN_DEFAULT_USER_DESC_24, Eligibility::kDeprecated, "legacy/avatar_frog.png"},
    {IDR_LOGIN_DEFAULT_USER_25, IDS_LOGIN_DEFAULT_USER_DESC_25, Eligibility::kDeprecated, "legacy/avatar_ganzania.png"},
    {IDR_LOGIN_DEFAULT_USER_26, IDS_LOGIN_DEFAULT_USER_DESC_26, Eligibility::kDeprecated, "legacy/avatar_jackrussellterrier.png"},
    {IDR_LOGIN_DEFAULT_USER_27, IDS_LOGIN_DEFAULT_USER_DESC_27, Eligibility::kDeprecated, "legacy/avatar_jellyfish.png"},
    {IDR_LOGIN_DEFAULT_USER_28, IDS_LOGIN_DEFAULT_USER_DESC_28, Eligibility::kDeprecated, "legacy/avatar_kiwi.png"},
    {IDR_LOGIN_DEFAULT_USER_29, IDS_LOGIN_DEFAULT_USER_DESC_29, Eligibility::kDeprecated, "legacy/avatar_penguin.png"},
    {IDR_LOGIN_DEFAULT_USER_30, IDS_LOGIN_DEFAULT_USER_DESC_30, Eligibility::kDeprecated, "legacy/avatar_rainbowfish.png"},
    {IDR_LOGIN_DEFAULT_USER_31, IDS_LOGIN_DEFAULT_USER_DESC_31, Eligibility::kDeprecated, "legacy/avatar_recordplayer.png"},
    {IDR_LOGIN_DEFAULT_USER_32, IDS_LOGIN_DEFAULT_USER_DESC_32, Eligibility::kDeprecated, "legacy/avatar_upsidedown.png"},
    {IDR_LOGIN_DEFAULT_USER_33, IDS_LOGIN_DEFAULT_USER_DESC_33, Eligibility::kDeprecated, "legacy/avatar_cat.png"},
    // Third set of images.
    {IDR_LOGIN_DEFAULT_USER_34, IDS_LOGIN_DEFAULT_USER_DESC_34, Eligibility::kDeprecated, "origami/avatar_penguin.png"},
    {IDR_LOGIN_DEFAULT_USER_35, IDS_LOGIN_DEFAULT_USER_DESC_35, Eligibility::kDeprecated, "origami/avatar_fox.png"},
    {IDR_LOGIN_DEFAULT_USER_36, IDS_LOGIN_DEFAULT_USER_DESC_36, Eligibility::kDeprecated, "origami/avatar_snail.png"},
    {IDR_LOGIN_DEFAULT_USER_37, IDS_LOGIN_DEFAULT_USER_DESC_37, Eligibility::kDeprecated, "origami/avatar_redbutterfly.png"},
    {IDR_LOGIN_DEFAULT_USER_38, IDS_LOGIN_DEFAULT_USER_DESC_38, Eligibility::kDeprecated, "origami/avatar_cat.png"},
    {IDR_LOGIN_DEFAULT_USER_39, IDS_LOGIN_DEFAULT_USER_DESC_39, Eligibility::kDeprecated, "origami/avatar_corgi.png"},
    {IDR_LOGIN_DEFAULT_USER_40, IDS_LOGIN_DEFAULT_USER_DESC_40, Eligibility::kDeprecated, "origami/avatar_rabbit.png"},
    {IDR_LOGIN_DEFAULT_USER_41, IDS_LOGIN_DEFAULT_USER_DESC_41, Eligibility::kDeprecated, "origami/avatar_pinkbutterfly.png"},
    {IDR_LOGIN_DEFAULT_USER_42, IDS_LOGIN_DEFAULT_USER_DESC_42, Eligibility::kDeprecated, "origami/avatar_monkey.png"},
    {IDR_LOGIN_DEFAULT_USER_43, IDS_LOGIN_DEFAULT_USER_DESC_43, Eligibility::kDeprecated, "origami/avatar_dragon.png"},
    {IDR_LOGIN_DEFAULT_USER_44, IDS_LOGIN_DEFAULT_USER_DESC_44, Eligibility::kDeprecated, "origami/avatar_elephant.png"},
    {IDR_LOGIN_DEFAULT_USER_45, IDS_LOGIN_DEFAULT_USER_DESC_45, Eligibility::kDeprecated, "origami/avatar_panda.png"},
    {IDR_LOGIN_DEFAULT_USER_46, IDS_LOGIN_DEFAULT_USER_DESC_46, Eligibility::kDeprecated, "origami/avatar_unicorn.png"},
    {IDR_LOGIN_DEFAULT_USER_47, IDS_LOGIN_DEFAULT_USER_DESC_47, Eligibility::kDeprecated, "origami/avatar_butterflies.png"},
    {IDR_LOGIN_DEFAULT_USER_48, IDS_LOGIN_DEFAULT_USER_DESC_48, Eligibility::kEligible,   "illustration/avatar_bird.png"},
    {IDR_LOGIN_DEFAULT_USER_49, IDS_LOGIN_DEFAULT_USER_DESC_49, Eligibility::kEligible,   "illustration/avatar_ramen.png"},
    {IDR_LOGIN_DEFAULT_USER_50, IDS_LOGIN_DEFAULT_USER_DESC_50, Eligibility::kEligible,   "illustration/avatar_tamagotchi.png"},
    {IDR_LOGIN_DEFAULT_USER_51, IDS_LOGIN_DEFAULT_USER_DESC_51, Eligibility::kEligible,   "illustration/avatar_cheese.png"},
    {IDR_LOGIN_DEFAULT_USER_52, IDS_LOGIN_DEFAULT_USER_DESC_52, Eligibility::kEligible,   "illustration/avatar_football.png"},
    {IDR_LOGIN_DEFAULT_USER_53, IDS_LOGIN_DEFAULT_USER_DESC_53, Eligibility::kEligible,   "illustration/avatar_basketball.png"},
    {IDR_LOGIN_DEFAULT_USER_54, IDS_LOGIN_DEFAULT_USER_DESC_54, Eligibility::kEligible,   "illustration/avatar_vinyl.png"},
    {IDR_LOGIN_DEFAULT_USER_55, IDS_LOGIN_DEFAULT_USER_DESC_55, Eligibility::kEligible,   "illustration/avatar_sushi.png"},
    {IDR_LOGIN_DEFAULT_USER_56, IDS_LOGIN_DEFAULT_USER_DESC_56, Eligibility::kEligible,   "illustration/avatar_bike.png"},
    {IDR_LOGIN_DEFAULT_USER_57, IDS_LOGIN_DEFAULT_USER_DESC_57, Eligibility::kEligible,   "illustration/avatar_sunglasses.png"},
    {IDR_LOGIN_DEFAULT_USER_58, IDS_LOGIN_DEFAULT_USER_DESC_58, Eligibility::kEligible,   "abstract/avatar_pizza.png"},
    {IDR_LOGIN_DEFAULT_USER_59, IDS_LOGIN_DEFAULT_USER_DESC_59, Eligibility::kEligible,   "abstract/avatar_sandwich.png"},
    {IDR_LOGIN_DEFAULT_USER_60, IDS_LOGIN_DEFAULT_USER_DESC_60, Eligibility::kEligible,   "abstract/avatar_cappuccino.png"},
    {IDR_LOGIN_DEFAULT_USER_61, IDS_LOGIN_DEFAULT_USER_DESC_61, Eligibility::kEligible,   "abstract/avatar_icewater.png"},
    {IDR_LOGIN_DEFAULT_USER_62, IDS_LOGIN_DEFAULT_USER_DESC_62, Eligibility::kEligible,   "abstract/avatar_icecream.png"},
    {IDR_LOGIN_DEFAULT_USER_63, IDS_LOGIN_DEFAULT_USER_DESC_63, Eligibility::kEligible,   "abstract/avatar_onigiri.png"},
    {IDR_LOGIN_DEFAULT_USER_64, IDS_LOGIN_DEFAULT_USER_DESC_64, Eligibility::kEligible,   "abstract/avatar_melon.png"},
    {IDR_LOGIN_DEFAULT_USER_65, IDS_LOGIN_DEFAULT_USER_DESC_65, Eligibility::kEligible,   "abstract/avatar_avocado.png"},
    {IDR_LOGIN_DEFAULT_USER_66, IDS_LOGIN_DEFAULT_USER_DESC_66, Eligibility::kDeprecated, "geo/avatar_geo1.png"},
    {IDR_LOGIN_DEFAULT_USER_67, IDS_LOGIN_DEFAULT_USER_DESC_67, Eligibility::kDeprecated, "geo/avatar_geo2.png"},
    {IDR_LOGIN_DEFAULT_USER_68, IDS_LOGIN_DEFAULT_USER_DESC_68, Eligibility::kDeprecated, "geo/avatar_geo3.png"},
    {IDR_LOGIN_DEFAULT_USER_69, IDS_LOGIN_DEFAULT_USER_DESC_69, Eligibility::kDeprecated, "geo/avatar_geo4.png"},
    {IDR_LOGIN_DEFAULT_USER_70, IDS_LOGIN_DEFAULT_USER_DESC_70, Eligibility::kDeprecated, "geo/avatar_geo5.png"},
    {IDR_LOGIN_DEFAULT_USER_71, IDS_LOGIN_DEFAULT_USER_DESC_71, Eligibility::kEligible,   "imaginary/avatar_botanist.png"},
    {IDR_LOGIN_DEFAULT_USER_72, IDS_LOGIN_DEFAULT_USER_DESC_72, Eligibility::kEligible,   "imaginary/avatar_burger.png"},
    {IDR_LOGIN_DEFAULT_USER_73, IDS_LOGIN_DEFAULT_USER_DESC_73, Eligibility::kEligible,   "imaginary/avatar_graduate.png"},
    {IDR_LOGIN_DEFAULT_USER_74, IDS_LOGIN_DEFAULT_USER_DESC_74, Eligibility::kEligible,   "imaginary/avatar_guitar.png"},
    {IDR_LOGIN_DEFAULT_USER_75, IDS_LOGIN_DEFAULT_USER_DESC_75, Eligibility::kEligible,   "imaginary/avatar_waving.png"},
    {IDR_LOGIN_DEFAULT_USER_76, IDS_LOGIN_DEFAULT_USER_DESC_76, Eligibility::kEligible,   "imaginary/avatar_lion.png"},
    {IDR_LOGIN_DEFAULT_USER_77, IDS_LOGIN_DEFAULT_USER_DESC_77, Eligibility::kEligible,   "imaginary/avatar_planet.png"},
    {IDR_LOGIN_DEFAULT_USER_78, IDS_LOGIN_DEFAULT_USER_DESC_78, Eligibility::kEligible,   "imaginary/avatar_instant_camera.png"},
    {IDR_LOGIN_DEFAULT_USER_79, IDS_LOGIN_DEFAULT_USER_DESC_79, Eligibility::kEligible,   "imaginary/avatar_robot.png"},
    {IDR_LOGIN_DEFAULT_USER_80, IDS_LOGIN_DEFAULT_USER_DESC_80, Eligibility::kEligible,   "imaginary/avatar_sneaker.png"},
    {IDR_LOGIN_DEFAULT_USER_81, IDS_LOGIN_DEFAULT_USER_DESC_81, Eligibility::kEligible,   "imaginary/avatar_van.png"},
    {IDR_LOGIN_DEFAULT_USER_82, IDS_LOGIN_DEFAULT_USER_DESC_82, Eligibility::kEligible,   "imaginary/avatar_watermelon.png"},
    // Material design avatars.
    {IDR_LOGIN_DEFAULT_USER_83, IDS_LOGIN_DEFAULT_USER_DESC_83, Eligibility::kEligible, "material_design/avatar_person_watering_plants.png"},
    {IDR_LOGIN_DEFAULT_USER_84, IDS_LOGIN_DEFAULT_USER_DESC_84, Eligibility::kEligible, "material_design/avatar_person_daydreaming.png"},
    {IDR_LOGIN_DEFAULT_USER_85, IDS_LOGIN_DEFAULT_USER_DESC_85, Eligibility::kEligible, "material_design/avatar_person_with_flowers.png"},
    {IDR_LOGIN_DEFAULT_USER_86, IDS_LOGIN_DEFAULT_USER_DESC_86, Eligibility::kEligible, "material_design/avatar_person_with_cats.png"},
    {IDR_LOGIN_DEFAULT_USER_87, IDS_LOGIN_DEFAULT_USER_DESC_87, Eligibility::kEligible, "material_design/avatar_artist.png"},
    {IDR_LOGIN_DEFAULT_USER_88, IDS_LOGIN_DEFAULT_USER_DESC_88, Eligibility::kEligible, "material_design/avatar_person_doing_taichi.png"},
    {IDR_LOGIN_DEFAULT_USER_89, IDS_LOGIN_DEFAULT_USER_DESC_89, Eligibility::kEligible, "material_design/avatar_signing_thankyou.png"},
    {IDR_LOGIN_DEFAULT_USER_90, IDS_LOGIN_DEFAULT_USER_DESC_90, Eligibility::kEligible, "material_design/avatar_person_with_coffee.png"},
    {IDR_LOGIN_DEFAULT_USER_91, IDS_LOGIN_DEFAULT_USER_DESC_91, Eligibility::kEligible, "material_design/avatar_dog_wagging_tail.png"},
    {IDR_LOGIN_DEFAULT_USER_92, IDS_LOGIN_DEFAULT_USER_DESC_92, Eligibility::kEligible, "material_design/avatar_nurse.png"},
    {IDR_LOGIN_DEFAULT_USER_93, IDS_LOGIN_DEFAULT_USER_DESC_93, Eligibility::kEligible, "material_design/avatar_gamer.png"},
    {IDR_LOGIN_DEFAULT_USER_94, IDS_LOGIN_DEFAULT_USER_DESC_94, Eligibility::kEligible, "material_design/avatar_bookworm.png"},
    {IDR_LOGIN_DEFAULT_USER_95, IDS_LOGIN_DEFAULT_USER_DESC_95, Eligibility::kEligible, "material_design/avatar_biking.png"},
    {IDR_LOGIN_DEFAULT_USER_96, IDS_LOGIN_DEFAULT_USER_DESC_96, Eligibility::kEligible, "material_design/avatar_person_in_snow.png"},
    {IDR_LOGIN_DEFAULT_USER_97, IDS_LOGIN_DEFAULT_USER_DESC_97, Eligibility::kEligible, "material_design/avatar_person_with_megaphone.png"},
};
// clang-format on

// Indexes of the current set of default images in the order that will display
// in the personalization settings page. This list should contain all the
// indexes of egligible default images listed above.
// clang-format off
constexpr int kCurrentImageIndexes[] = {
    // Material design avatars.
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    // Third set of images.
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
};
// clang-format on

// Compile time check that make sure the current default images are the set of
// all the eligible default images.
constexpr bool ValidateCurrentImageIndexes() {
  int num_eligible_images = 0;
  for (const auto info : kDefaultImageInfo) {
    if (info.eligibility == Eligibility::kEligible)
      num_eligible_images++;
  }
  if (num_eligible_images != std::size(kCurrentImageIndexes))
    return false;

  for (const int index : kCurrentImageIndexes) {
    if (kDefaultImageInfo[index].eligibility != Eligibility::kEligible)
      return false;
    if (kDefaultImageInfo[index].description_message_id == 0) {
      // All current and new images must have a description.
      return false;
    }
  }
  return true;
}

static_assert(ValidateCurrentImageIndexes(),
              "kCurrentImageIndexes should contain all the indexes of "
              "egligible default images listed in kDefaultImageInfo.");

// Source info ids of default user images.
struct DefaultImageSourceInfoIds {
  // Message IDs of author info.
  const int author_id;

  // Message IDs of website info.
  const int website_id;
};

// Source info of (deprecated) default user images.
const DefaultImageSourceInfoIds kDefaultImageSourceInfoIds[] = {
    {IDS_LOGIN_DEFAULT_USER_AUTHOR, IDS_LOGIN_DEFAULT_USER_WEBSITE},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_1, IDS_LOGIN_DEFAULT_USER_WEBSITE_1},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_2, IDS_LOGIN_DEFAULT_USER_WEBSITE_2},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_3, IDS_LOGIN_DEFAULT_USER_WEBSITE_3},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_4, IDS_LOGIN_DEFAULT_USER_WEBSITE_4},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_5, IDS_LOGIN_DEFAULT_USER_WEBSITE_5},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_6, IDS_LOGIN_DEFAULT_USER_WEBSITE_6},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_7, IDS_LOGIN_DEFAULT_USER_WEBSITE_7},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_8, IDS_LOGIN_DEFAULT_USER_WEBSITE_8},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_9, IDS_LOGIN_DEFAULT_USER_WEBSITE_9},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_10, IDS_LOGIN_DEFAULT_USER_WEBSITE_10},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_11, IDS_LOGIN_DEFAULT_USER_WEBSITE_11},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_12, IDS_LOGIN_DEFAULT_USER_WEBSITE_12},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_13, IDS_LOGIN_DEFAULT_USER_WEBSITE_13},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_14, IDS_LOGIN_DEFAULT_USER_WEBSITE_14},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_15, IDS_LOGIN_DEFAULT_USER_WEBSITE_15},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_16, IDS_LOGIN_DEFAULT_USER_WEBSITE_16},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_17, IDS_LOGIN_DEFAULT_USER_WEBSITE_17},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_18, IDS_LOGIN_DEFAULT_USER_WEBSITE_18},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_19, IDS_LOGIN_DEFAULT_USER_WEBSITE_19},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_20, IDS_LOGIN_DEFAULT_USER_WEBSITE_20},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_21, IDS_LOGIN_DEFAULT_USER_WEBSITE_21},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_22, IDS_LOGIN_DEFAULT_USER_WEBSITE_22},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_23, IDS_LOGIN_DEFAULT_USER_WEBSITE_23},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_24, IDS_LOGIN_DEFAULT_USER_WEBSITE_24},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_25, IDS_LOGIN_DEFAULT_USER_WEBSITE_25},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_26, IDS_LOGIN_DEFAULT_USER_WEBSITE_26},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_27, IDS_LOGIN_DEFAULT_USER_WEBSITE_27},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_28, IDS_LOGIN_DEFAULT_USER_WEBSITE_28},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_29, IDS_LOGIN_DEFAULT_USER_WEBSITE_29},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_30, IDS_LOGIN_DEFAULT_USER_WEBSITE_30},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_31, IDS_LOGIN_DEFAULT_USER_WEBSITE_31},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_32, IDS_LOGIN_DEFAULT_USER_WEBSITE_32},
    {IDS_LOGIN_DEFAULT_USER_AUTHOR_33, IDS_LOGIN_DEFAULT_USER_WEBSITE_33},
};

constexpr char kDefaultUrlPrefix[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER_";
constexpr char kZeroDefaultUrl[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER";
constexpr char kGstaticImagePrefix[] =
    "https://www.gstatic.com/chromecast/home/chromeos/avatars/";
constexpr char k100PercentPrefix[] = "default_100_percent/";
constexpr char k200PercentPrefix[] = "default_200_percent/";

const std::string GetUrlPrefixForScaleFactor(
    ui::ResourceScaleFactor scale_factor) {
  switch (scale_factor) {
    case ui::kScaleFactorNone:
    case ui::k100Percent:
      return k100PercentPrefix;
    case ui::k200Percent:
      return k200PercentPrefix;
    case ui::k300Percent:
    case ui::NUM_SCALE_FACTORS:
      NOTIMPLEMENTED();
      return k100PercentPrefix;
  }
}

ui::ResourceScaleFactor GetMaximumScaleFactorForDefaultImage(int index) {
  if (index <= kLastLegacyImageIndex)
    return ui::k100Percent;
  else
    return ui::k200Percent;
}

}  // namespace

const int kDefaultImagesCount = std::size(kDefaultImageInfo);

const int kFirstDefaultImageIndex = 48;

const int kLastLegacyImageIndex = 33;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// The order and the values of these constants are important for histograms
// of different Chrome OS versions to be merged smoothly.
const int kHistogramImageFromCamera = 0;
const int kHistogramImageExternal = 1;
const int kHistogramImageFromProfile = 2;
// The special images max count is used to reserve a histogram range (0-9) for
// special images. Default images will have their histogram value starting
// at 10. Check ChromeOSUserImageId in tools/metrics/histograms/enums.xml to see
// how these values are mapped.
const int kHistogramSpecialImagesMaxCount = 10;
const int kHistogramImagesCount =
    kDefaultImagesCount + kHistogramSpecialImagesMaxCount;

ui::ResourceScaleFactor GetAdjustedScaleFactorForDefaultImage(
    int index,
    ui::ResourceScaleFactor scale_factor) {
  ui::ResourceScaleFactor max_scale_factor =
      GetMaximumScaleFactorForDefaultImage(index);
  if (max_scale_factor == ui::k100Percent)
    return max_scale_factor;

  return scale_factor;
}

GURL GetDefaultImageUrl(
    int index,
    ui::ResourceScaleFactor scale_factor /*= ui::k200Percent*/) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);

  if (ash::features::IsAvatarsCloudMigrationEnabled()) {
    ui::ResourceScaleFactor adjusted_scale_factor =
        GetAdjustedScaleFactorForDefaultImage(index, scale_factor);
    auto scale_factor_prefix =
        GetUrlPrefixForScaleFactor(adjusted_scale_factor);

    return GURL(base::StrCat({kGstaticImagePrefix, scale_factor_prefix,
                              kDefaultImageInfo[index].path}));
  }

  if (index == 0)
    return GURL(kZeroDefaultUrl);
  return GURL(base::StringPrintf("%s%d", kDefaultUrlPrefix, index));
}

// DEPRECATED: after the full migration of the avatar images to cloud,
// this function should be removed since default images will no longer
// be available in device resources.
const gfx::ImageSkia& GetDefaultImageDeprecated(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kDefaultImageInfo[index].resource_id);
}

int GetDefaultImageResourceId(int index) {
  return kDefaultImageInfo[index].resource_id;
}

int GetRandomDefaultImageIndex() {
  return kCurrentImageIndexes[base::RandInt(
      0, std::size(kCurrentImageIndexes) - 1)];
}

bool IsValidIndex(int index) {
  return index >= 0 && index < kDefaultImagesCount;
}

bool IsInCurrentImageSet(int index) {
  return IsValidIndex(index) &&
         kDefaultImageInfo[index].eligibility == Eligibility::kEligible;
}

DefaultUserImage GetDefaultUserImage(
    int index,
    ui::ResourceScaleFactor scale_factor /*= ui::k200Percent*/) {
  DCHECK(IsValidIndex(index));
  int description_message_id = kDefaultImageInfo[index].description_message_id;
  std::u16string title = description_message_id
                             ? l10n_util::GetStringUTF16(description_message_id)
                             : std::u16string();

  return {index, std::move(title),
          default_user_image::GetDefaultImageUrl(index, scale_factor),
          GetDeprecatedDefaultImageSourceInfo(index)};
}

std::vector<DefaultUserImage> GetCurrentImageSet() {
  std::vector<DefaultUserImage> result;
  for (int index : kCurrentImageIndexes)
    result.push_back(GetDefaultUserImage(index));
  return result;
}

base::Value::List GetCurrentImageSetAsListValue() {
  base::Value::List image_urls;
  for (auto& user_image : GetCurrentImageSet()) {
    base::Value::Dict image_data;
    image_data.Set("index", user_image.index);
    image_data.Set("title", std::move(user_image.title));
    image_data.Set("url", user_image.url.spec());
    image_urls.Append(std::move(image_data));
  }
  return image_urls;
}

absl::optional<DeprecatedSourceInfo> GetDeprecatedDefaultImageSourceInfo(
    size_t index) {
  if (index >= std::size(kDefaultImageSourceInfoIds))
    return absl::nullopt;

  const auto& source_info_ids = kDefaultImageSourceInfoIds[index];
  return DeprecatedSourceInfo(
      l10n_util::GetStringUTF16(source_info_ids.author_id),
      GURL(l10n_util::GetStringUTF16(source_info_ids.website_id)));
}

}  // namespace default_user_image
}  // namespace ash
