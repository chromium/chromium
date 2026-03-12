// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_test_utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/viz/test/paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace cc {

namespace {

// On pixel mismatch, output a HTML-based diff page containing both images,
// instead of the base64-encoded actual and expected images separately.
BASE_FEATURE(kCcPixelTestHtmlDiffPage,
#if BUILDFLAG(IS_ANDROID)
             // Android test runners don't have access to the diff page.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

std::string GetImageDiffUrl(const SkBitmap& left, const SkBitmap& right) {
  std::stringstream test_name;
  if (const ::testing::TestInfo* test_info =
          ::testing::UnitTest::GetInstance()->current_test_info()) {
    test_name << test_info->test_suite_name() << "." << test_info->name();
    const char* type_param = test_info->type_param();
    const char* value_param = test_info->value_param();
    if (type_param != nullptr || value_param != nullptr) {
      test_name << ", where ";
      if (type_param != nullptr) {
        test_name << "TypeParam = " << type_param;
        if (value_param != nullptr) {
          test_name << " and ";
        }
      }
      if (value_param != nullptr) {
        test_name << "GetParam() = " << value_param;
      }
    }
  }

  // Avoid using `viz::Paths::DIR_TEST_DATA`, since there may be callers of this
  // helper from outside Viz.
  const base::FilePath image_diff_path =
      base::PathService::CheckedGet(base::DIR_SRC_TEST_DATA_ROOT)
          .AppendASCII("components")
          .AppendASCII("test")
          .AppendASCII("data")
          .AppendASCII("viz")
          .AppendASCII("image_diff.html");

  const bool use_plus = true;
  return base::StringPrintf(
      "file:///%s?testName=%s&leftName=%s&leftSrc=%s&rightName=%s&"
      "rightSrc=%s",
      image_diff_path.AsUTF8Unsafe(),
      base::EscapeUrlEncodedData(test_name.str(), use_plus).c_str(), "Actual",
      base::EscapeUrlEncodedData(GetPNGDataUrl(left), use_plus).c_str(),
      "Expected",
      base::EscapeUrlEncodedData(GetPNGDataUrl(right), use_plus).c_str());
}

}  // namespace

bool WritePNGFile(const SkBitmap& bitmap,
                  const base::FilePath& file_path,
                  bool discard_transparency) {
  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, discard_transparency);
  if (png_data && base::CreateDirectory(file_path.DirName())) {
    return base::WriteFile(file_path, png_data.value());
  }
  return false;
}

std::string GetPNGDataUrl(const SkBitmap& bitmap) {
  std::optional<std::vector<uint8_t>> png_data =
      gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false);
  return base::StrCat(
      {"data:image/png;base64,",
       base::Base64Encode(png_data.value_or(std::vector<uint8_t>()))});
}

SkBitmap ReadPNGFile(const base::FilePath& file_path) {
  std::optional<std::vector<uint8_t>> png_data =
      base::ReadFileToBytes(file_path);
  if (!png_data) {
    return SkBitmap();
  }

  return gfx::PNGCodec::Decode(png_data.value());
}

bool MatchesBitmap(const SkBitmap& gen_bmp,
                   const SkBitmap& ref_bmp,
                   const PixelComparator& comparator) {
  bool pixels_match = true;

  // Check if images size matches
  if (gen_bmp.width() != ref_bmp.width() ||
      gen_bmp.height() != ref_bmp.height()) {
    LOG(ERROR)
        << "Dimensions do not match! "
        << "Actual: " << gen_bmp.width() << "x" << gen_bmp.height()
        << "; "
        << "Expected: " << ref_bmp.width() << "x" << ref_bmp.height();
    pixels_match = false;
  }

  // Shortcut for empty images. They are always equal.
  if (pixels_match && (gen_bmp.width() == 0 || gen_bmp.height() == 0))
    return true;

  if (pixels_match && !comparator.Compare(gen_bmp, ref_bmp)) {
    LOG(ERROR) << "Pixels do not match!";
    pixels_match = false;
  }

  if (!pixels_match) {
    if (base::FeatureList::IsEnabled(kCcPixelTestHtmlDiffPage)) {
      LOG(ERROR) << "Diff (open in browser):\n"
                 << GetImageDiffUrl(gen_bmp, ref_bmp) << "\n";
    }
    LOG(ERROR) << "Actual (open in browser):\n" << GetPNGDataUrl(gen_bmp);
    LOG(ERROR) << "Expected (open in browser):\n" << GetPNGDataUrl(ref_bmp);
  }
  return pixels_match;
}

bool MatchesPNGFile(const SkBitmap& gen_bmp,
                    base::FilePath ref_img_path,
                    const PixelComparator& comparator) {
  SkBitmap ref_bmp = ReadPNGFile(ref_img_path);
  if (ref_bmp.isNull()) {
    LOG(ERROR) << "Cannot read reference image: " << ref_img_path.value();
    return false;
  }

  return MatchesBitmap(gen_bmp, ref_bmp, comparator);
}

}  // namespace cc
