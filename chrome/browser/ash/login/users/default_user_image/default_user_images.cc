// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/default_user_image/default_user_images.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/values.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace default_user_image {

// Resource IDs of default user images.
// clang-format off
const int kDefaultImageResourceIDs[] = {
    IDR_LOGIN_DEFAULT_USER,

    // Original set of images.
    IDR_LOGIN_DEFAULT_USER_1,
    IDR_LOGIN_DEFAULT_USER_2,
    IDR_LOGIN_DEFAULT_USER_3,
    IDR_LOGIN_DEFAULT_USER_4,
    IDR_LOGIN_DEFAULT_USER_5,
    IDR_LOGIN_DEFAULT_USER_6,
    IDR_LOGIN_DEFAULT_USER_7,
    IDR_LOGIN_DEFAULT_USER_8,
    IDR_LOGIN_DEFAULT_USER_9,
    IDR_LOGIN_DEFAULT_USER_10,
    IDR_LOGIN_DEFAULT_USER_11,
    IDR_LOGIN_DEFAULT_USER_12,
    IDR_LOGIN_DEFAULT_USER_13,
    IDR_LOGIN_DEFAULT_USER_14,
    IDR_LOGIN_DEFAULT_USER_15,
    IDR_LOGIN_DEFAULT_USER_16,
    IDR_LOGIN_DEFAULT_USER_17,
    IDR_LOGIN_DEFAULT_USER_18,

    // Second set of images.
    IDR_LOGIN_DEFAULT_USER_19,
    IDR_LOGIN_DEFAULT_USER_20,
    IDR_LOGIN_DEFAULT_USER_21,
    IDR_LOGIN_DEFAULT_USER_22,
    IDR_LOGIN_DEFAULT_USER_23,
    IDR_LOGIN_DEFAULT_USER_24,
    IDR_LOGIN_DEFAULT_USER_25,
    IDR_LOGIN_DEFAULT_USER_26,
    IDR_LOGIN_DEFAULT_USER_27,
    IDR_LOGIN_DEFAULT_USER_28,
    IDR_LOGIN_DEFAULT_USER_29,
    IDR_LOGIN_DEFAULT_USER_30,
    IDR_LOGIN_DEFAULT_USER_31,
    IDR_LOGIN_DEFAULT_USER_32,
    IDR_LOGIN_DEFAULT_USER_33,

    // Third set of images.
    IDR_LOGIN_DEFAULT_USER_34,
    IDR_LOGIN_DEFAULT_USER_35,
    IDR_LOGIN_DEFAULT_USER_36,
    IDR_LOGIN_DEFAULT_USER_37,
    IDR_LOGIN_DEFAULT_USER_38,
    IDR_LOGIN_DEFAULT_USER_39,
    IDR_LOGIN_DEFAULT_USER_40,
    IDR_LOGIN_DEFAULT_USER_41,
    IDR_LOGIN_DEFAULT_USER_42,
    IDR_LOGIN_DEFAULT_USER_43,
    IDR_LOGIN_DEFAULT_USER_44,
    IDR_LOGIN_DEFAULT_USER_45,
    IDR_LOGIN_DEFAULT_USER_46,
    IDR_LOGIN_DEFAULT_USER_47,
    IDR_LOGIN_DEFAULT_USER_48,
    IDR_LOGIN_DEFAULT_USER_49,
    IDR_LOGIN_DEFAULT_USER_50,
    IDR_LOGIN_DEFAULT_USER_51,
    IDR_LOGIN_DEFAULT_USER_52,
    IDR_LOGIN_DEFAULT_USER_53,
    IDR_LOGIN_DEFAULT_USER_54,
    IDR_LOGIN_DEFAULT_USER_55,
    IDR_LOGIN_DEFAULT_USER_56,
    IDR_LOGIN_DEFAULT_USER_57,
    IDR_LOGIN_DEFAULT_USER_58,
    IDR_LOGIN_DEFAULT_USER_59,
    IDR_LOGIN_DEFAULT_USER_60,
    IDR_LOGIN_DEFAULT_USER_61,
    IDR_LOGIN_DEFAULT_USER_62,
    IDR_LOGIN_DEFAULT_USER_63,
    IDR_LOGIN_DEFAULT_USER_64,
    IDR_LOGIN_DEFAULT_USER_65,
    IDR_LOGIN_DEFAULT_USER_66,
    IDR_LOGIN_DEFAULT_USER_67,
    IDR_LOGIN_DEFAULT_USER_68,
    IDR_LOGIN_DEFAULT_USER_69,
    IDR_LOGIN_DEFAULT_USER_70,
    IDR_LOGIN_DEFAULT_USER_71,
    IDR_LOGIN_DEFAULT_USER_72,
    IDR_LOGIN_DEFAULT_USER_73,
    IDR_LOGIN_DEFAULT_USER_74,
    IDR_LOGIN_DEFAULT_USER_75,
    IDR_LOGIN_DEFAULT_USER_76,
    IDR_LOGIN_DEFAULT_USER_77,
    IDR_LOGIN_DEFAULT_USER_78,
    IDR_LOGIN_DEFAULT_USER_79,
    IDR_LOGIN_DEFAULT_USER_80,
    IDR_LOGIN_DEFAULT_USER_81,
    IDR_LOGIN_DEFAULT_USER_82,
};
// clang-format on

const int kDefaultImagesCount = base::size(kDefaultImageResourceIDs);

const int kFirstDefaultImageIndex = 34;

// Limit random default image index to prevent undesirable UI behavior when
// selecting an image with a high index. E.g. automatic scrolling of picture
// list that is used to present default images.
const int kLastRandomDefaultImageIndex = 47;

// The order and the values of these constants are important for histograms
// of different Chrome OS versions to be merged smoothly.
const int kHistogramImageFromCamera = 19;
const int kHistogramImageFromFile = 20;
const int kHistogramImageOld = 21;
const int kHistogramImageFromProfile = 22;
// const int kHistogramVideoFromCamera = 23;  // Unused
// const int kHistogramVideoFromFile = 24;  // Unused
const int kHistogramImagesCount = kDefaultImagesCount + 6;

namespace {

const char kDefaultUrlPrefix[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER_";
const char kZeroDefaultUrl[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER";

// clang-format off
const int kDefaultImageAuthorIDs[] = {
    IDS_LOGIN_DEFAULT_USER_AUTHOR,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_1,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_2,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_3,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_4,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_5,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_6,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_7,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_8,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_9,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_10,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_11,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_12,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_13,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_14,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_15,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_16,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_17,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_18,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_19,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_20,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_21,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_22,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_23,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_24,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_25,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_26,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_27,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_28,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_29,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_30,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_31,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_32,
    IDS_LOGIN_DEFAULT_USER_AUTHOR_33,
};
// clang-format on

const int kDefaultImageAuthorMaxID = base::size(kDefaultImageAuthorIDs);

// clang-format off
const int kDefaultImageWebsiteIDs[] = {
    IDS_LOGIN_DEFAULT_USER_WEBSITE,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_1,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_2,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_3,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_4,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_5,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_6,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_7,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_8,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_9,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_10,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_11,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_12,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_13,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_14,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_15,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_16,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_17,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_18,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_19,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_20,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_21,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_22,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_23,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_24,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_25,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_26,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_27,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_28,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_29,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_30,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_31,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_32,
    IDS_LOGIN_DEFAULT_USER_WEBSITE_33,
};

const int kDefaultImageWebsiteMaxID = base::size(kDefaultImageWebsiteIDs);
// clang-format on

// IDs of default user image descriptions.
const int kDefaultImageDescriptions[] = {
    0,  // No description for deprecated user image 0.
    0,  // No description for deprecated user image 1.
    0,  // No description for deprecated user image 2.
    0,  // No description for deprecated user image 3.
    0,  // No description for deprecated user image 4.
    0,  // No description for deprecated user image 5.
    0,  // No description for deprecated user image 6.
    0,  // No description for deprecated user image 7.
    0,  // No description for deprecated user image 8.
    0,  // No description for deprecated user image 9.
    0,  // No description for deprecated user image 10.
    0,  // No description for deprecated user image 11.
    0,  // No description for deprecated user image 12.
    0,  // No description for deprecated user image 13.
    0,  // No description for deprecated user image 14.
    0,  // No description for deprecated user image 15.
    0,  // No description for deprecated user image 16.
    0,  // No description for deprecated user image 17.
    0,  // No description for deprecated user image 18.
    IDS_LOGIN_DEFAULT_USER_DESC_19,
    IDS_LOGIN_DEFAULT_USER_DESC_20,
    IDS_LOGIN_DEFAULT_USER_DESC_21,
    IDS_LOGIN_DEFAULT_USER_DESC_22,
    IDS_LOGIN_DEFAULT_USER_DESC_23,
    IDS_LOGIN_DEFAULT_USER_DESC_24,
    IDS_LOGIN_DEFAULT_USER_DESC_25,
    IDS_LOGIN_DEFAULT_USER_DESC_26,
    IDS_LOGIN_DEFAULT_USER_DESC_27,
    IDS_LOGIN_DEFAULT_USER_DESC_28,
    IDS_LOGIN_DEFAULT_USER_DESC_29,
    IDS_LOGIN_DEFAULT_USER_DESC_30,
    IDS_LOGIN_DEFAULT_USER_DESC_31,
    IDS_LOGIN_DEFAULT_USER_DESC_32,
    IDS_LOGIN_DEFAULT_USER_DESC_33,
    IDS_LOGIN_DEFAULT_USER_DESC_34,
    IDS_LOGIN_DEFAULT_USER_DESC_35,
    IDS_LOGIN_DEFAULT_USER_DESC_36,
    IDS_LOGIN_DEFAULT_USER_DESC_37,
    IDS_LOGIN_DEFAULT_USER_DESC_38,
    IDS_LOGIN_DEFAULT_USER_DESC_39,
    IDS_LOGIN_DEFAULT_USER_DESC_40,
    IDS_LOGIN_DEFAULT_USER_DESC_41,
    IDS_LOGIN_DEFAULT_USER_DESC_42,
    IDS_LOGIN_DEFAULT_USER_DESC_43,
    IDS_LOGIN_DEFAULT_USER_DESC_44,
    IDS_LOGIN_DEFAULT_USER_DESC_45,
    IDS_LOGIN_DEFAULT_USER_DESC_46,
    IDS_LOGIN_DEFAULT_USER_DESC_47,
    IDS_LOGIN_DEFAULT_USER_DESC_48,
    IDS_LOGIN_DEFAULT_USER_DESC_49,
    IDS_LOGIN_DEFAULT_USER_DESC_50,
    IDS_LOGIN_DEFAULT_USER_DESC_51,
    IDS_LOGIN_DEFAULT_USER_DESC_52,
    IDS_LOGIN_DEFAULT_USER_DESC_53,
    IDS_LOGIN_DEFAULT_USER_DESC_54,
    IDS_LOGIN_DEFAULT_USER_DESC_55,
    IDS_LOGIN_DEFAULT_USER_DESC_56,
    IDS_LOGIN_DEFAULT_USER_DESC_57,
    IDS_LOGIN_DEFAULT_USER_DESC_58,
    IDS_LOGIN_DEFAULT_USER_DESC_59,
    IDS_LOGIN_DEFAULT_USER_DESC_60,
    IDS_LOGIN_DEFAULT_USER_DESC_61,
    IDS_LOGIN_DEFAULT_USER_DESC_62,
    IDS_LOGIN_DEFAULT_USER_DESC_63,
    IDS_LOGIN_DEFAULT_USER_DESC_64,
    IDS_LOGIN_DEFAULT_USER_DESC_65,
    IDS_LOGIN_DEFAULT_USER_DESC_66,
    IDS_LOGIN_DEFAULT_USER_DESC_67,
    IDS_LOGIN_DEFAULT_USER_DESC_68,
    IDS_LOGIN_DEFAULT_USER_DESC_69,
    IDS_LOGIN_DEFAULT_USER_DESC_70,
    IDS_LOGIN_DEFAULT_USER_DESC_71,
    IDS_LOGIN_DEFAULT_USER_DESC_72,
    IDS_LOGIN_DEFAULT_USER_DESC_73,
    IDS_LOGIN_DEFAULT_USER_DESC_74,
    IDS_LOGIN_DEFAULT_USER_DESC_75,
    IDS_LOGIN_DEFAULT_USER_DESC_76,
    IDS_LOGIN_DEFAULT_USER_DESC_77,
    IDS_LOGIN_DEFAULT_USER_DESC_78,
    IDS_LOGIN_DEFAULT_USER_DESC_79,
    IDS_LOGIN_DEFAULT_USER_DESC_80,
    IDS_LOGIN_DEFAULT_USER_DESC_81,
    IDS_LOGIN_DEFAULT_USER_DESC_82,
};

const int kDefaultImageDescriptionsMaxID =
    base::size(kDefaultImageDescriptions);

// Returns true if the string specified consists of the prefix and one of
// the default images indices. Returns the index of the image in `image_id`
// variable.
bool IsDefaultImageString(const std::string& s,
                          const std::string& prefix,
                          int* image_id) {
  DCHECK(image_id);
  if (!base::StartsWith(s, prefix, base::CompareCase::SENSITIVE))
    return false;

  int image_index = -1;
  if (base::StringToInt(
          base::MakeStringPiece(s.begin() + prefix.length(), s.end()),
          &image_index)) {
    if (image_index < 0 || image_index >= kDefaultImagesCount)
      return false;
    *image_id = image_index;
    return true;
  }

  return false;
}

void GetFirstLastIndex(int* first, int* last) {
  if (first)
    *first = kFirstDefaultImageIndex;
  if (last)
    *last = kDefaultImagesCount - 1;
}

}  // namespace

std::string GetDefaultImageUrl(int index) {
  if (index <= 0 || index >= kDefaultImagesCount)
    return kZeroDefaultUrl;
  return base::StringPrintf("%s%d", kDefaultUrlPrefix, index);
}

bool IsDefaultImageUrl(const std::string& url, int* image_id) {
  if (url == kZeroDefaultUrl) {
    *image_id = 0;
    return true;
  }
  return IsDefaultImageString(url, kDefaultUrlPrefix, image_id);
}

const gfx::ImageSkia& GetDefaultImage(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  return *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      kDefaultImageResourceIDs[index]);
}

int GetRandomDefaultImageIndex() {
  int first;
  GetFirstLastIndex(&first, nullptr);
  return base::RandInt(first, kLastRandomDefaultImageIndex);
}

bool IsValidIndex(int index) {
  return index >= 0 && index < kDefaultImagesCount;
}

bool IsInCurrentImageSet(int index) {
  int first, last;
  GetFirstLastIndex(&first, &last);
  return index >= first && index <= last;
}

int GetDefaultImageHistogramValue(int index) {
  DCHECK(index >= 0 && index < kDefaultImagesCount);
  // Create a gap in histogram values for
  // [kHistogramImageFromCamera..kHistogramImageFromProfile] block to fit.
  if (index < kHistogramImageFromCamera)
    return index;
  return index + 6;
}

std::unique_ptr<base::ListValue> GetAsDictionary(bool all) {
  int first, last;
  GetFirstLastIndex(&first, &last);
  if (all)
    first = 0;

  auto image_urls = std::make_unique<base::ListValue>();
  for (int i = first; i <= last; ++i) {
    auto image_data = std::make_unique<base::DictionaryValue>();
    image_data->SetString("url", default_user_image::GetDefaultImageUrl(i));
    image_data->SetInteger("index", i);
    if (i < kDefaultImageAuthorMaxID) {
      image_data->SetString("author",
                            l10n_util::GetStringUTF16(
                                default_user_image::kDefaultImageAuthorIDs[i]));
    }
    if (i < kDefaultImageWebsiteMaxID) {
      image_data->SetString(
          "website", l10n_util::GetStringUTF16(
                         default_user_image::kDefaultImageWebsiteIDs[i]));
    }
    if (i < kDefaultImageDescriptionsMaxID) {
      int string_id = kDefaultImageDescriptions[i];
      image_data->SetString("title", string_id
                                         ? l10n_util::GetStringUTF16(string_id)
                                         : std::u16string());
    }
    image_urls->Append(std::move(image_data));
  }
  return image_urls;
}

int GetFirstDefaultImage() {
  int first;
  GetFirstLastIndex(&first, nullptr);
  return first;
}

}  // namespace default_user_image
}  // namespace ash
