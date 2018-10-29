// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/browser_theme_pack.h"

#include <limits.h>
#include <stddef.h>

#include <limits>
#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/common/extensions/manifest_handlers/theme_handler.h"
#include "chrome/grit/theme_resources.h"
#include "components/crx_file/id_util.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/base/resource/data_pack.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using content::BrowserThread;
using extensions::Extension;
using TP = ThemeProperties;

namespace {

// The tallest tab height in any mode.
constexpr int kTallestTabHeight = 41;

// The tallest height above the tabs in any mode is 19 DIP.
constexpr int kTallestFrameHeight = kTallestTabHeight + 19;

// Version number of the current theme pack. We just throw out and rebuild
// theme packs that aren't int-equal to this. Increment this number if you
// change default theme assets or if you need themes to recreate their generated
// images (which are cached).
const int kThemePackVersion = 60;

// IDs that are in the DataPack won't clash with the positive integer
// uint16_t. kHeaderID should always have the maximum value because we want the
// "header" to be written last. That way we can detect whether the pack was
// successfully written and ignore and regenerate if it was only partially
// written (i.e. chrome crashed on a different thread while writing the pack).
const int kMaxID = 0x0000FFFF;  // Max unsigned 16-bit int.
const int kHeaderID = kMaxID - 1;
const int kTintsID = kMaxID - 2;
const int kColorsID = kMaxID - 3;
const int kDisplayPropertiesID = kMaxID - 4;
const int kSourceImagesID = kMaxID - 5;
const int kScaleFactorsID = kMaxID - 6;

// Persistent constants for the main images that we need. These have the same
// names as their IDR_* counterparts but these values will always stay the
// same.
const int PRS_THEME_FRAME = 1;
const int PRS_THEME_FRAME_INACTIVE = 2;
const int PRS_THEME_FRAME_INCOGNITO = 3;
const int PRS_THEME_FRAME_INCOGNITO_INACTIVE = 4;
const int PRS_THEME_TOOLBAR = 5;
const int PRS_THEME_TAB_BACKGROUND = 6;
const int PRS_THEME_TAB_BACKGROUND_INCOGNITO = 7;
const int PRS_THEME_TAB_BACKGROUND_V = 8;
const int PRS_THEME_NTP_BACKGROUND = 9;
const int PRS_THEME_FRAME_OVERLAY = 10;
const int PRS_THEME_FRAME_OVERLAY_INACTIVE = 11;
const int PRS_THEME_BUTTON_BACKGROUND = 12;
const int PRS_THEME_NTP_ATTRIBUTION = 13;
const int PRS_THEME_WINDOW_CONTROL_BACKGROUND = 14;
const int PRS_THEME_TAB_BACKGROUND_INACTIVE = 15;
const int PRS_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE = 16;

struct PersistingImagesTable {
  // A non-changing integer ID meant to be saved in theme packs. This ID must
  // not change between versions of chrome.
  int persistent_id;

  // The IDR that depends on the whims of GRIT and therefore changes whenever
  // someone adds a new resource.
  int idr_id;

  // String to check for when parsing theme manifests or NULL if this isn't
  // supposed to be changeable by the user.
  const char* key;
};

// IDR_* resource names change whenever new resources are added; use persistent
// IDs when storing to a cached pack.
const PersistingImagesTable kPersistingImages[] = {
    {PRS_THEME_FRAME, IDR_THEME_FRAME, "theme_frame"},
    {PRS_THEME_FRAME_INACTIVE, IDR_THEME_FRAME_INACTIVE,
     "theme_frame_inactive"},
    {PRS_THEME_FRAME_INCOGNITO, IDR_THEME_FRAME_INCOGNITO,
     "theme_frame_incognito"},
    {PRS_THEME_FRAME_INCOGNITO_INACTIVE, IDR_THEME_FRAME_INCOGNITO_INACTIVE,
     "theme_frame_incognito_inactive"},
    {PRS_THEME_TOOLBAR, IDR_THEME_TOOLBAR, "theme_toolbar"},
    {PRS_THEME_TAB_BACKGROUND, IDR_THEME_TAB_BACKGROUND,
     "theme_tab_background"},
    {PRS_THEME_TAB_BACKGROUND_INACTIVE, IDR_THEME_TAB_BACKGROUND_INACTIVE,
     "theme_tab_background_inactive"},
    {PRS_THEME_TAB_BACKGROUND_INCOGNITO, IDR_THEME_TAB_BACKGROUND_INCOGNITO,
     "theme_tab_background_incognito"},
    {PRS_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE,
     IDR_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE,
     "theme_tab_background_incognito_inactive"},
    {PRS_THEME_TAB_BACKGROUND_V, IDR_THEME_TAB_BACKGROUND_V,
     "theme_tab_background_v"},
    {PRS_THEME_NTP_BACKGROUND, IDR_THEME_NTP_BACKGROUND,
     "theme_ntp_background"},
    {PRS_THEME_FRAME_OVERLAY, IDR_THEME_FRAME_OVERLAY, "theme_frame_overlay"},
    {PRS_THEME_FRAME_OVERLAY_INACTIVE, IDR_THEME_FRAME_OVERLAY_INACTIVE,
     "theme_frame_overlay_inactive"},
    {PRS_THEME_BUTTON_BACKGROUND, IDR_THEME_BUTTON_BACKGROUND,
     "theme_button_background"},
    {PRS_THEME_NTP_ATTRIBUTION, IDR_THEME_NTP_ATTRIBUTION,
     "theme_ntp_attribution"},
    {PRS_THEME_WINDOW_CONTROL_BACKGROUND, IDR_THEME_WINDOW_CONTROL_BACKGROUND,
     "theme_window_control_background"},
};
const size_t kPersistingImagesLength = arraysize(kPersistingImages);

int GetPersistentIDByNameHelper(const std::string& key,
                                const PersistingImagesTable* image_table,
                                size_t image_table_size) {
  for (size_t i = 0; i < image_table_size; ++i) {
    if (image_table[i].key &&
        base::LowerCaseEqualsASCII(key, image_table[i].key)) {
      return image_table[i].persistent_id;
    }
  }
  return -1;
}

int GetPersistentIDByName(const std::string& key) {
  return GetPersistentIDByNameHelper(key,
                                     kPersistingImages,
                                     kPersistingImagesLength);
}

int GetPersistentIDByIDR(int idr) {
  static std::map<int,int>* lookup_table = new std::map<int,int>();
  if (lookup_table->empty()) {
    for (size_t i = 0; i < kPersistingImagesLength; ++i) {
      int idr = kPersistingImages[i].idr_id;
      int prs_id = kPersistingImages[i].persistent_id;
      (*lookup_table)[idr] = prs_id;
    }
  }
  auto it = lookup_table->find(idr);
  return (it == lookup_table->end()) ? -1 : it->second;
}

// Returns the maximum persistent id.
int GetMaxPersistentId() {
  static int max_prs_id = -1;
  if (max_prs_id == -1) {
    for (size_t i = 0; i < kPersistingImagesLength; ++i) {
      if (kPersistingImages[i].persistent_id > max_prs_id)
        max_prs_id = kPersistingImages[i].persistent_id;
    }
  }
  return max_prs_id;
}

// Returns true if the scales in |input| match those in |expected|.
// The order must match as the index is used in determining the raw id.
bool InputScalesValid(const base::StringPiece& input,
                      const std::vector<ui::ScaleFactor>& expected) {
  if (input.size() != expected.size() * sizeof(float))
    return false;
  std::unique_ptr<float[]> scales(new float[expected.size()]);
  // Do a memcpy to avoid misaligned memory access.
  memcpy(scales.get(), input.data(), input.size());
  for (size_t index = 0; index < expected.size(); ++index) {
    if (scales[index] != ui::GetScaleForScaleFactor(expected[index]))
      return false;
  }
  return true;
}

// Returns |scale_factors| as a string to be written to disk.
std::string GetScaleFactorsAsString(
    const std::vector<ui::ScaleFactor>& scale_factors) {
  std::unique_ptr<float[]> scales(new float[scale_factors.size()]);
  for (size_t i = 0; i < scale_factors.size(); ++i)
    scales[i] = ui::GetScaleForScaleFactor(scale_factors[i]);
  std::string out_string = std::string(
      reinterpret_cast<const char*>(scales.get()),
      scale_factors.size() * sizeof(float));
  return out_string;
}

struct StringToIntTable {
  const char* key;
  TP::OverwritableByUserThemeProperty id;
};

// Strings used by themes to identify tints in the JSON.
const StringToIntTable kTintTable[] = {
    {"buttons", TP::TINT_BUTTONS},
    {"frame", TP::TINT_FRAME},
    {"frame_inactive", TP::TINT_FRAME_INACTIVE},
    {"frame_incognito", TP::TINT_FRAME_INCOGNITO},
    {"frame_incognito_inactive", TP::TINT_FRAME_INCOGNITO_INACTIVE},
    {"background_tab", TP::TINT_BACKGROUND_TAB},
};
const size_t kTintTableLength = arraysize(kTintTable);

// Strings used by themes to identify colors in the JSON.
constexpr StringToIntTable kOverwritableColorTable[] = {
    {"frame", TP::COLOR_FRAME},
    {"frame_inactive", TP::COLOR_FRAME_INACTIVE},
    {"frame_incognito", TP::COLOR_FRAME_INCOGNITO},
    {"frame_incognito_inactive", TP::COLOR_FRAME_INCOGNITO_INACTIVE},
    {"background_tab", TP::COLOR_BACKGROUND_TAB},
    {"background_tab_inactive", TP::COLOR_BACKGROUND_TAB_INACTIVE},
    {"background_tab_incognito", TP::COLOR_BACKGROUND_TAB_INCOGNITO},
    {"background_tab_incognito_inactive",
     TP::COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE},
    {"toolbar", TP::COLOR_TOOLBAR},
    {"tab_text", TP::COLOR_TAB_TEXT},
    {"tab_text_inactive", TP::COLOR_TAB_TEXT_INACTIVE},
    {"tab_background_text", TP::COLOR_BACKGROUND_TAB_TEXT},
    {"tab_background_text_inactive", TP::COLOR_BACKGROUND_TAB_TEXT_INACTIVE},
    {"tab_background_text_incognito", TP::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO},
    {"tab_background_text_incognito_inactive",
     TP::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE},
    {"bookmark_text", TP::COLOR_BOOKMARK_TEXT},
    {"ntp_background", TP::COLOR_NTP_BACKGROUND},
    {"ntp_text", TP::COLOR_NTP_TEXT},
    {"ntp_link", TP::COLOR_NTP_LINK},
    {"ntp_header", TP::COLOR_NTP_HEADER},
    {"button_background", TP::COLOR_BUTTON_BACKGROUND},
};
constexpr size_t kOverwritableColorTableLength =
    base::size(kOverwritableColorTable);

// Colors generated based on the theme, but not overwritable in the theme file.
constexpr int kNonOverwritableColorTable[] = {
    TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE,
    TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
    TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE,
    TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE,
};
constexpr size_t kNonOverwritableColorTableLength =
    base::size(kNonOverwritableColorTable);

// The maximum number of colors we may need to store (includes ones that can be
// specified by the theme, and ones that we calculate but can't be specified).
constexpr size_t kColorsArrayLength =
    kOverwritableColorTableLength + kNonOverwritableColorTableLength;

// Strings used by themes to identify display properties keys in JSON.
const StringToIntTable kDisplayProperties[] = {
    {"ntp_background_alignment", TP::NTP_BACKGROUND_ALIGNMENT},
    {"ntp_background_repeat", TP::NTP_BACKGROUND_TILING},
    {"ntp_logo_alternate", TP::NTP_LOGO_ALTERNATE},
};
const size_t kDisplayPropertiesSize = arraysize(kDisplayProperties);

int GetIntForString(const std::string& key,
                    const StringToIntTable* table,
                    size_t table_length) {
  for (size_t i = 0; i < table_length; ++i) {
    if (base::LowerCaseEqualsASCII(key, table[i].key)) {
      return table[i].id;
    }
  }

  return -1;
}

struct CropEntry {
  int prs_id;

  // The maximum useful height of the image at |prs_id|.
  int max_height;
};

// The images which should be cropped before being saved to the data pack. The
// maximum heights are meant to be conservative as to give room for the UI to
// change without the maximum heights having to be modified.
// |kThemePackVersion| must be incremented if any of the maximum heights below
// are modified.
const struct CropEntry kImagesToCrop[] = {
    {PRS_THEME_FRAME, kTallestFrameHeight},
    {PRS_THEME_FRAME_INACTIVE, kTallestFrameHeight},
    {PRS_THEME_FRAME_INCOGNITO, kTallestFrameHeight},
    {PRS_THEME_FRAME_INCOGNITO_INACTIVE, kTallestFrameHeight},
    {PRS_THEME_FRAME_OVERLAY, kTallestFrameHeight},
    {PRS_THEME_FRAME_OVERLAY_INACTIVE, kTallestFrameHeight},
    {PRS_THEME_TOOLBAR, 200},
    {PRS_THEME_BUTTON_BACKGROUND, 60},
    {PRS_THEME_WINDOW_CONTROL_BACKGROUND, 50},
};

// A list of images that don't need tinting or any other modification and can
// be byte-copied directly into the finished DataPack. This should contain the
// persistent IDs for all themeable image IDs that aren't in kFrameValues,
// kTabBackgroundMap or kImagesToCrop.
const int kPreloadIDs[] = {
  PRS_THEME_NTP_BACKGROUND,
  PRS_THEME_NTP_ATTRIBUTION,
};

// Returns a piece of memory with the contents of the file |path|.
scoped_refptr<base::RefCountedMemory> ReadFileData(const base::FilePath& path) {
  if (!path.empty()) {
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (file.IsValid()) {
      int64_t length = file.GetLength();
      if (length > 0 && length < INT_MAX) {
        int size = static_cast<int>(length);
        std::vector<unsigned char> raw_data;
        raw_data.resize(size);
        char* data = reinterpret_cast<char*>(&(raw_data.front()));
        if (file.ReadAtCurrentPos(data, size) == length)
          return base::RefCountedBytes::TakeVector(&raw_data);
      }
    }
  }

  return nullptr;
}

// Computes a bitmap at one scale from a bitmap at a different scale.
SkBitmap CreateLowQualityResizedBitmap(const SkBitmap& source_bitmap,
                                       ui::ScaleFactor source_scale_factor,
                                       ui::ScaleFactor desired_scale_factor) {
  gfx::Size scaled_size = gfx::ScaleToCeiledSize(
      gfx::Size(source_bitmap.width(), source_bitmap.height()),
      ui::GetScaleForScaleFactor(desired_scale_factor) /
          ui::GetScaleForScaleFactor(source_scale_factor));
  SkBitmap scaled_bitmap;
  scaled_bitmap.allocN32Pixels(scaled_size.width(), scaled_size.height());
  scaled_bitmap.eraseARGB(0, 0, 0, 0);
  SkCanvas canvas(scaled_bitmap);
  SkRect scaled_bounds = RectToSkRect(gfx::Rect(scaled_size));
  // Note(oshima): The following scaling code doesn't work with
  // a mask image.
  canvas.drawBitmapRect(source_bitmap, scaled_bounds, NULL);
  return scaled_bitmap;
}

// A ImageSkiaSource that scales 100P image to the target scale factor
// if the ImageSkiaRep for the target scale factor isn't available.
class ThemeImageSource: public gfx::ImageSkiaSource {
 public:
  explicit ThemeImageSource(const gfx::ImageSkia& source) : source_(source) {
  }
  ~ThemeImageSource() override {}

  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    if (source_.HasRepresentation(scale))
      return source_.GetRepresentation(scale);
    const gfx::ImageSkiaRep& rep_100p = source_.GetRepresentation(1.0f);
    SkBitmap scaled_bitmap = CreateLowQualityResizedBitmap(
        rep_100p.GetBitmap(), ui::SCALE_FACTOR_100P,
        ui::GetSupportedScaleFactor(scale));
    return gfx::ImageSkiaRep(scaled_bitmap, scale);
  }

 private:
  const gfx::ImageSkia source_;

  DISALLOW_COPY_AND_ASSIGN(ThemeImageSource);
};

// An ImageSkiaSource that delays decoding PNG data into bitmaps until
// needed. Missing data for a scale factor is computed by scaling data for an
// available scale factor. Computed bitmaps are stored for future look up.
class ThemeImagePngSource : public gfx::ImageSkiaSource {
 public:
  typedef std::map<ui::ScaleFactor,
                   scoped_refptr<base::RefCountedMemory> > PngMap;

  explicit ThemeImagePngSource(const PngMap& png_map) : png_map_(png_map) {}

  ~ThemeImagePngSource() override {}

 private:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactor(scale);
    // Look up the bitmap for |scale factor| in the bitmap map. If found
    // return it.
    BitmapMap::const_iterator exact_bitmap_it = bitmap_map_.find(scale_factor);
    if (exact_bitmap_it != bitmap_map_.end())
      return gfx::ImageSkiaRep(exact_bitmap_it->second, scale);

    // Look up the raw PNG data for |scale_factor| in the png map. If found,
    // decode it, store the result in the bitmap map and return it.
    PngMap::const_iterator exact_png_it = png_map_.find(scale_factor);
    if (exact_png_it != png_map_.end()) {
      SkBitmap bitmap;
      if (!gfx::PNGCodec::Decode(exact_png_it->second->front(),
                                 exact_png_it->second->size(),
                                 &bitmap)) {
        NOTREACHED();
        return gfx::ImageSkiaRep();
      }
      bitmap_map_[scale_factor] = bitmap;
      return gfx::ImageSkiaRep(bitmap, scale);
    }

    // Find an available PNG for another scale factor. We want to use the
    // highest available scale factor.
    PngMap::const_iterator available_png_it = png_map_.end();
    for (PngMap::const_iterator png_it = png_map_.begin();
         png_it != png_map_.end(); ++png_it) {
      if (available_png_it == png_map_.end() ||
          ui::GetScaleForScaleFactor(png_it->first) >
          ui::GetScaleForScaleFactor(available_png_it->first)) {
        available_png_it = png_it;
      }
    }
    if (available_png_it == png_map_.end())
      return gfx::ImageSkiaRep();
    ui::ScaleFactor available_scale_factor = available_png_it->first;

    // Look up the bitmap for |available_scale_factor| in the bitmap map.
    // If not found, decode the corresponging png data, store the result
    // in the bitmap map.
    BitmapMap::const_iterator available_bitmap_it =
        bitmap_map_.find(available_scale_factor);
    if (available_bitmap_it == bitmap_map_.end()) {
      SkBitmap available_bitmap;
      if (!gfx::PNGCodec::Decode(available_png_it->second->front(),
                                 available_png_it->second->size(),
                                 &available_bitmap)) {
        NOTREACHED();
        return gfx::ImageSkiaRep();
      }
      bitmap_map_[available_scale_factor] = available_bitmap;
      available_bitmap_it = bitmap_map_.find(available_scale_factor);
    }

    // Scale the available bitmap to the desired scale factor, store the result
    // in the bitmap map and return it.
    SkBitmap scaled_bitmap = CreateLowQualityResizedBitmap(
        available_bitmap_it->second,
        available_scale_factor,
        scale_factor);
    bitmap_map_[scale_factor] = scaled_bitmap;
    return gfx::ImageSkiaRep(scaled_bitmap, scale);
  }

  PngMap png_map_;

  typedef std::map<ui::ScaleFactor, SkBitmap> BitmapMap;
  BitmapMap bitmap_map_;

  DISALLOW_COPY_AND_ASSIGN(ThemeImagePngSource);
};

class TabBackgroundImageSource: public gfx::CanvasImageSource {
 public:
  TabBackgroundImageSource(SkColor background_color,
                           const gfx::ImageSkia& image_to_tint,
                           const gfx::ImageSkia& overlay,
                           const color_utils::HSL& hsl_shift,
                           int vertical_offset)
      : gfx::CanvasImageSource(
            image_to_tint.isNull() ? overlay.size() : image_to_tint.size(),
            false),
        background_color_(background_color),
        image_to_tint_(image_to_tint),
        overlay_(overlay),
        hsl_shift_(hsl_shift),
        vertical_offset_(vertical_offset) {}

  ~TabBackgroundImageSource() override {}

  // Overridden from CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawColor(background_color_);

    // Begin with the frame background image, if any.  Since the frame and tabs
    // have grown taller and changed alignment over time, not all themes have a
    // sufficiently tall image; tiling by vertically mirroring in this case is
    // the least-glitchy-looking option.  Note that the behavior here needs to
    // stay in sync with how the browser frame will actually be drawn.
    if (!image_to_tint_.isNull()) {
      gfx::ImageSkia bg_tint = gfx::ImageSkiaOperations::CreateHSLShiftedImage(
          image_to_tint_, hsl_shift_);
      canvas->TileImageInt(bg_tint, 0, vertical_offset_, 0, 0, size().width(),
                           size().height(), 1.0f, SkShader::kRepeat_TileMode,
                           SkShader::kMirror_TileMode);
    }

    // If the theme has a custom tab background image, overlay it.  Vertical
    // mirroring is used for the same reason as above.  This behavior needs to
    // stay in sync with how tabs are drawn.
    if (!overlay_.isNull()) {
      canvas->TileImageInt(overlay_, 0, 0, 0, 0, size().width(),
                           size().height(), 1.0f, SkShader::kRepeat_TileMode,
                           SkShader::kMirror_TileMode);
    }
  }

 private:
  const SkColor background_color_;
  const gfx::ImageSkia image_to_tint_;
  const gfx::ImageSkia overlay_;
  const color_utils::HSL hsl_shift_;
  const int vertical_offset_;

  DISALLOW_COPY_AND_ASSIGN(TabBackgroundImageSource);
};

class ControlButtonBackgroundImageSource : public gfx::CanvasImageSource {
 public:
  ControlButtonBackgroundImageSource(SkColor background_color,
                                     const gfx::ImageSkia& bg_image,
                                     const gfx::Size& dest_size)
      : gfx::CanvasImageSource(dest_size, false),
        background_color_(background_color),
        bg_image_(bg_image) {
    DCHECK(!bg_image.isNull());
  }

  ~ControlButtonBackgroundImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override {
    canvas->DrawColor(background_color_);

    if (!bg_image_.isNull())
      canvas->DrawImageInt(bg_image_, 0, 0);
  }

 private:
  const SkColor background_color_;
  const gfx::ImageSkia bg_image_;

  DISALLOW_COPY_AND_ASSIGN(ControlButtonBackgroundImageSource);
};

}  // namespace

BrowserThemePack::~BrowserThemePack() {
  if (!data_pack_.get()) {
    delete header_;
    delete [] tints_;
    delete [] colors_;
    delete [] display_properties_;
    delete [] source_images_;
  }
}

void BrowserThemePack::SetColor(int id, SkColor color) {
  DCHECK(colors_);

  int first_available_color = -1;
  for (size_t i = 0; i < kColorsArrayLength; ++i) {
    if (colors_[i].id == id) {
      colors_[i].color = color;
      return;
    }
    if (colors_[i].id == -1 && first_available_color == -1)
      first_available_color = i;
  }

  DCHECK_NE(-1, first_available_color);
  colors_[first_available_color].id = id;
  colors_[first_available_color].color = color;
}

void BrowserThemePack::ComputeColorFromImage(int color_id,
                                             int height,
                                             const gfx::Image& image) {
  SkColor temp_color;
  if (!GetColor(color_id, &temp_color)) {
    // Include all colors in the analysis.
    constexpr color_utils::HSL kNoBounds = {-1, -1, -1};
    const SkColor color = color_utils::CalculateKMeanColorOfBitmap(
        *image.ToSkBitmap(), height, kNoBounds, kNoBounds, false);
    SetColor(color_id, color);
  }
}

// static
void BrowserThemePack::BuildFromExtension(
    const extensions::Extension* extension,
    scoped_refptr<BrowserThemePack> pack) {
  DCHECK(extension);
  DCHECK(extension->is_theme());
  DCHECK(!pack->is_valid());

  pack->BuildHeader(extension);
  pack->BuildTintsFromJSON(extensions::ThemeInfo::GetTints(extension));
  pack->BuildColorsFromJSON(extensions::ThemeInfo::GetColors(extension));
  pack->BuildDisplayPropertiesFromJSON(
      extensions::ThemeInfo::GetDisplayProperties(extension));

  // Builds the images. (Image building is dependent on tints).
  FilePathMap file_paths;
  pack->ParseImageNamesFromJSON(
      extensions::ThemeInfo::GetImages(extension),
      extension->path(),
      &file_paths);
  pack->BuildSourceImagesArray(file_paths);

  if (!pack->LoadRawBitmapsTo(file_paths, &pack->images_))
    return;

  pack->CropImages(&pack->images_);

  // Create frame images, and generate frame colors from images where relevant.
  // This must be done after reading colors from JSON (so they aren't
  // overwritten).
  pack->CreateFrameImagesAndColors(&pack->images_);

  // Generate any missing frame colors.  This must be done after generating
  // colors from the frame images, so only colors with no matching images are
  // generated.
  pack->GenerateFrameColors();

  // Generate background color information for window control buttons.  This
  // must be done after frame colors are set, since they are used when
  // determining window control button colors.
  pack->GenerateWindowControlButtonColor(&pack->images_);

  // Create the tab background images, and generate colors where relevant.  This
  // must be done after all frame colors are set, since they are used when
  // creating these.
  pack->CreateTabBackgroundImagesAndColors(&pack->images_);

  // Generate any missing text colors.  This must be done after generating frame
  // and tab colors, as generated text colors will try to appropriately contrast
  // with the frame/tab behind them.
  pack->GenerateMissingTextColors();

  // Make sure the |images_on_file_thread_| has bitmaps for supported
  // scale factors before passing to FILE thread.
  pack->images_on_file_thread_ = pack->images_;
  for (auto it = pack->images_on_file_thread_.begin();
       it != pack->images_on_file_thread_.end(); ++it) {
    gfx::ImageSkia* image_skia =
        const_cast<gfx::ImageSkia*>(it->second.ToImageSkia());
    image_skia->MakeThreadSafe();
  }

  // Set ThemeImageSource on |images_| to resample the source
  // image if a caller of BrowserThemePack::GetImageNamed() requests an
  // ImageSkiaRep for a scale factor not specified by the theme author.
  // Callers of BrowserThemePack::GetImageNamed() to be able to retrieve
  // ImageSkiaReps for all supported scale factors.
  for (auto it = pack->images_.begin(); it != pack->images_.end(); ++it) {
    const gfx::ImageSkia source_image_skia = it->second.AsImageSkia();
    auto source = std::make_unique<ThemeImageSource>(source_image_skia);
    gfx::ImageSkia image_skia(std::move(source), source_image_skia.size());
    it->second = gfx::Image(image_skia);
  }

  // Generate raw images (for new-tab-page attribution and background) for
  // any missing scale from an available scale image.
  for (size_t i = 0; i < arraysize(kPreloadIDs); ++i) {
    pack->GenerateRawImageForAllSupportedScales(kPreloadIDs[i]);
  }

  // The BrowserThemePack is now in a consistent state.
  pack->is_valid_ = true;
}

// static
scoped_refptr<BrowserThemePack> BrowserThemePack::BuildFromDataPack(
    const base::FilePath& path, const std::string& expected_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Allow IO on UI thread due to deep-seated theme design issues.
  // (see http://crbug.com/80206)
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  scoped_refptr<BrowserThemePack> pack(new BrowserThemePack);
  // Scale factor parameter is moot as data pack has image resources for all
  // supported scale factors.
  pack->data_pack_.reset(
      new ui::DataPack(ui::SCALE_FACTOR_NONE));

  if (!pack->data_pack_->LoadFromPath(path)) {
    LOG(ERROR) << "Failed to load theme data pack.";
    return NULL;
  }

  base::StringPiece pointer;
  if (!pack->data_pack_->GetStringPiece(kHeaderID, &pointer))
    return NULL;
  pack->header_ = reinterpret_cast<BrowserThemePackHeader*>(const_cast<char*>(
      pointer.data()));

  if (pack->header_->version != kThemePackVersion) {
    DLOG(ERROR) << "BuildFromDataPack failure! Version mismatch!";
    return NULL;
  }
  // TODO(erg): Check endianess once DataPack works on the other endian.
  std::string theme_id(reinterpret_cast<char*>(pack->header_->theme_id),
                       crx_file::id_util::kIdSize);
  std::string truncated_id = expected_id.substr(0, crx_file::id_util::kIdSize);
  if (theme_id != truncated_id) {
    DLOG(ERROR) << "Wrong id: " << theme_id << " vs " << expected_id;
    return NULL;
  }

  if (!pack->data_pack_->GetStringPiece(kTintsID, &pointer))
    return NULL;
  pack->tints_ = reinterpret_cast<TintEntry*>(const_cast<char*>(
      pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kColorsID, &pointer))
    return NULL;
  pack->colors_ =
      reinterpret_cast<ColorPair*>(const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kDisplayPropertiesID, &pointer))
    return NULL;
  pack->display_properties_ = reinterpret_cast<DisplayPropertyPair*>(
      const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kSourceImagesID, &pointer))
    return NULL;
  pack->source_images_ = reinterpret_cast<int*>(
      const_cast<char*>(pointer.data()));

  if (!pack->data_pack_->GetStringPiece(kScaleFactorsID, &pointer))
    return NULL;

  if (!InputScalesValid(pointer, pack->scale_factors_)) {
    DLOG(ERROR) << "BuildFromDataPack failure! The pack scale factors differ "
                << "from those supported by platform.";
    return NULL;
  }
  pack->is_valid_ = true;
  return pack;
}

// static
bool BrowserThemePack::IsPersistentImageID(int id) {
  for (size_t i = 0; i < kPersistingImagesLength; ++i)
    if (kPersistingImages[i].idr_id == id)
      return true;

  return false;
}

BrowserThemePack::BrowserThemePack() : CustomThemeSupplier(EXTENSION) {
  scale_factors_ = ui::GetSupportedScaleFactors();
  // On Windows HiDPI SCALE_FACTOR_100P may not be supported by default.
  if (!base::ContainsValue(scale_factors_, ui::SCALE_FACTOR_100P))
    scale_factors_.push_back(ui::SCALE_FACTOR_100P);
}

bool BrowserThemePack::WriteToDisk(const base::FilePath& path) const {
  // Add resources for each of the property arrays.
  RawDataForWriting resources;
  resources[kHeaderID] = base::StringPiece(
      reinterpret_cast<const char*>(header_), sizeof(BrowserThemePackHeader));
  resources[kTintsID] = base::StringPiece(
      reinterpret_cast<const char*>(tints_),
      sizeof(TintEntry[kTintTableLength]));
  resources[kColorsID] =
      base::StringPiece(reinterpret_cast<const char*>(colors_),
                        sizeof(ColorPair[kColorsArrayLength]));
  resources[kDisplayPropertiesID] = base::StringPiece(
      reinterpret_cast<const char*>(display_properties_),
      sizeof(DisplayPropertyPair[kDisplayPropertiesSize]));

  int source_count = 1;
  int* end = source_images_;
  for (; *end != -1 ; end++)
    source_count++;
  resources[kSourceImagesID] = base::StringPiece(
      reinterpret_cast<const char*>(source_images_),
      source_count * sizeof(*source_images_));

  // Store results of GetScaleFactorsAsString() in std::string as
  // base::StringPiece does not copy data in constructor.
  std::string scale_factors_string = GetScaleFactorsAsString(scale_factors_);
  resources[kScaleFactorsID] = scale_factors_string;

  AddRawImagesTo(image_memory_, &resources);

  RawImages reencoded_images;
  RepackImages(images_on_file_thread_, &reencoded_images);
  AddRawImagesTo(reencoded_images, &resources);

  return ui::DataPack::WritePack(path, resources, ui::DataPack::BINARY);
}

bool BrowserThemePack::GetTint(int id, color_utils::HSL* hsl) const {
  if (tints_) {
    for (size_t i = 0; i < kTintTableLength; ++i) {
      if (tints_[i].id == id) {
        hsl->h = tints_[i].h;
        hsl->s = tints_[i].s;
        hsl->l = tints_[i].l;
        return true;
      }
    }
  }

  return false;
}

bool BrowserThemePack::GetColor(int id, SkColor* color) const {
  static const base::NoDestructor<
      base::flat_set<TP::OverwritableByUserThemeProperty>>
      kOpaqueColors(
          // Explicitly creating a base::flat_set here is not strictly
          // necessary according to C++, but we do so to work around
          // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=84849.
          base::flat_set<TP::OverwritableByUserThemeProperty>({
              // Background tabs must be opaque since the tabstrip expects to be
              // able to render text opaquely atop them.
              TP::COLOR_BACKGROUND_TAB, TP::COLOR_BACKGROUND_TAB_INACTIVE,
              TP::COLOR_BACKGROUND_TAB_INCOGNITO,
              TP::COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE,
              // The frame colors will be used for background tabs when not
              // otherwise overridden and thus must be opaque as well.
              TP::COLOR_FRAME, TP::COLOR_FRAME_INACTIVE,
              TP::COLOR_FRAME_INCOGNITO, TP::COLOR_FRAME_INCOGNITO_INACTIVE,
              // The toolbar is used as the foreground tab color, so it must be
              // opaque just like background tabs.
              TP::COLOR_TOOLBAR,
          }));

  if (colors_) {
    for (size_t i = 0; i < kColorsArrayLength; ++i) {
      if (colors_[i].id == id) {
        *color = colors_[i].color;
        if (base::ContainsKey(*kOpaqueColors, id))
          *color = SkColorSetA(*color, SK_AlphaOPAQUE);
        return true;
      }
    }
  }

  return false;
}

bool BrowserThemePack::GetDisplayProperty(int id, int* result) const {
  if (display_properties_) {
    for (size_t i = 0; i < kDisplayPropertiesSize; ++i) {
      if (display_properties_[i].id == id) {
        *result = display_properties_[i].property;
        return true;
      }
    }
  }

  return false;
}

gfx::Image BrowserThemePack::GetImageNamed(int idr_id) {
  int prs_id = GetPersistentIDByIDR(idr_id);
  if (prs_id == -1)
    return gfx::Image();

  // Check if the image is cached.
  ImageCache::const_iterator image_iter = images_.find(prs_id);
  if (image_iter != images_.end())
    return image_iter->second;

  ThemeImagePngSource::PngMap png_map;
  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    scoped_refptr<base::RefCountedMemory> memory =
        GetRawData(idr_id, scale_factors_[i]);
    if (memory.get())
      png_map[scale_factors_[i]] = memory;
  }
  if (!png_map.empty()) {
    gfx::ImageSkia image_skia(std::make_unique<ThemeImagePngSource>(png_map),
                              1.0f);
    gfx::Image ret = gfx::Image(image_skia);
    images_[prs_id] = ret;
    return ret;
  }

  return gfx::Image();
}

base::RefCountedMemory* BrowserThemePack::GetRawData(
    int idr_id,
    ui::ScaleFactor scale_factor) const {
  base::RefCountedMemory* memory = NULL;
  int prs_id = GetPersistentIDByIDR(idr_id);
  int raw_id = GetRawIDByPersistentID(prs_id, scale_factor);

  if (raw_id != -1) {
    if (data_pack_.get()) {
      memory = data_pack_->GetStaticMemory(raw_id);
    } else {
      auto it = image_memory_.find(raw_id);
      if (it != image_memory_.end()) {
        memory = it->second.get();
      }
    }
  }

  return memory;
}

bool BrowserThemePack::HasCustomImage(int idr_id) const {
  int prs_id = GetPersistentIDByIDR(idr_id);
  if (prs_id == -1)
    return false;

  int* img = source_images_;
  for (; *img != -1; ++img) {
    if (*img == prs_id)
      return true;
  }

  return false;
}

// private:

void BrowserThemePack::BuildHeader(const Extension* extension) {
  header_ = new BrowserThemePackHeader;
  header_->version = kThemePackVersion;

// TODO(erg): Need to make this endian safe on other computers. Prerequisite
// is that ui::DataPack removes this same check.
#if defined(__BYTE_ORDER)
  // Linux check
  static_assert(__BYTE_ORDER == __LITTLE_ENDIAN,
                "datapack assumes little endian");
#elif defined(__BIG_ENDIAN__)
// Mac check
#error DataPack assumes little endian
#endif
  header_->little_endian = 1;

  const std::string& id = extension->id();
  memcpy(header_->theme_id, id.c_str(), crx_file::id_util::kIdSize);
}

void BrowserThemePack::BuildTintsFromJSON(
    const base::DictionaryValue* tints_value) {
  tints_ = new TintEntry[kTintTableLength];
  for (size_t i = 0; i < kTintTableLength; ++i) {
    tints_[i].id = -1;
    tints_[i].h = -1;
    tints_[i].s = -1;
    tints_[i].l = -1;
  }

  if (!tints_value)
    return;

  // Parse the incoming data from |tints_value| into an intermediary structure.
  std::map<int, color_utils::HSL> temp_tints;
  for (base::DictionaryValue::Iterator iter(*tints_value); !iter.IsAtEnd();
       iter.Advance()) {
    const base::ListValue* tint_list;
    if (iter.value().GetAsList(&tint_list) &&
        (tint_list->GetSize() == 3)) {
      color_utils::HSL hsl = { -1, -1, -1 };

      if (tint_list->GetDouble(0, &hsl.h) &&
          tint_list->GetDouble(1, &hsl.s) &&
          tint_list->GetDouble(2, &hsl.l)) {
        MakeHSLShiftValid(&hsl);
        int id = GetIntForString(iter.key(), kTintTable, kTintTableLength);
        if (id != -1) {
          temp_tints[id] = hsl;
        }
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, color_utils::HSL>::const_iterator it =
           temp_tints.begin();
       it != temp_tints.end() && count < kTintTableLength;
       ++it, ++count) {
    tints_[count].id = it->first;
    tints_[count].h = it->second.h;
    tints_[count].s = it->second.s;
    tints_[count].l = it->second.l;
  }
}

void BrowserThemePack::BuildColorsFromJSON(
    const base::DictionaryValue* colors_value) {
  colors_ = new ColorPair[kColorsArrayLength];
  for (size_t i = 0; i < kColorsArrayLength; ++i) {
    colors_[i].id = -1;
    colors_[i].color = SkColorSetRGB(0, 0, 0);
  }

  std::map<int, SkColor> temp_colors;
  if (colors_value)
    ReadColorsFromJSON(colors_value, &temp_colors);

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, SkColor>::const_iterator it = temp_colors.begin();
       it != temp_colors.end() && count < kOverwritableColorTableLength;
       ++it, ++count) {
    colors_[count].id = it->first;
    colors_[count].color = it->second;
  }
}

void BrowserThemePack::ReadColorsFromJSON(
    const base::DictionaryValue* colors_value,
    std::map<int, SkColor>* temp_colors) {
  // Parse the incoming data from |colors_value| into an intermediary structure.
  for (base::DictionaryValue::Iterator iter(*colors_value); !iter.IsAtEnd();
       iter.Advance()) {
    const base::ListValue* color_list;
    if (iter.value().GetAsList(&color_list) &&
        ((color_list->GetSize() == 3) || (color_list->GetSize() == 4))) {
      SkColor color = SK_ColorWHITE;
      int r, g, b;
      if (color_list->GetInteger(0, &r) && r >= 0 && r <= 255 &&
          color_list->GetInteger(1, &g) && g >= 0 && g <= 255 &&
          color_list->GetInteger(2, &b) && b >= 0 && b <= 255) {
        if (color_list->GetSize() == 4) {
          double alpha;
          int alpha_int;
          if (color_list->GetDouble(3, &alpha) && alpha >= 0 && alpha <= 1) {
            color = SkColorSetARGB(gfx::ToRoundedInt(alpha * 255), r, g, b);
          } else if (color_list->GetInteger(3, &alpha_int) &&
                     (alpha_int == 0 || alpha_int == 1)) {
            color = SkColorSetARGB(alpha_int ? 255 : 0, r, g, b);
          } else {
            // Invalid entry for part 4.
            continue;
          }
        } else {
          color = SkColorSetRGB(r, g, b);
        }

        if (iter.key() == "ntp_section") {
          // We no longer use ntp_section, but to support legacy
          // themes we still need to use it as a fallback for
          // ntp_header.
          if (!temp_colors->count(TP::COLOR_NTP_HEADER))
            (*temp_colors)[TP::COLOR_NTP_HEADER] = color;
        } else {
          int id = GetIntForString(iter.key(), kOverwritableColorTable,
                                   kOverwritableColorTableLength);
          if (id != -1)
            (*temp_colors)[id] = color;
        }
      }
    }
  }
}

void BrowserThemePack::BuildDisplayPropertiesFromJSON(
    const base::DictionaryValue* display_properties_value) {
  display_properties_ = new DisplayPropertyPair[kDisplayPropertiesSize];
  for (size_t i = 0; i < kDisplayPropertiesSize; ++i) {
    display_properties_[i].id = -1;
    display_properties_[i].property = 0;
  }

  if (!display_properties_value)
    return;

  std::map<int, int> temp_properties;
  for (base::DictionaryValue::Iterator iter(*display_properties_value);
       !iter.IsAtEnd(); iter.Advance()) {
    int property_id = GetIntForString(iter.key(), kDisplayProperties,
                                      kDisplayPropertiesSize);
    switch (property_id) {
      case TP::NTP_BACKGROUND_ALIGNMENT: {
        std::string val;
        if (iter.value().GetAsString(&val)) {
          temp_properties[TP::NTP_BACKGROUND_ALIGNMENT] =
              TP::StringToAlignment(val);
        }
        break;
      }
      case TP::NTP_BACKGROUND_TILING: {
        std::string val;
        if (iter.value().GetAsString(&val)) {
          temp_properties[TP::NTP_BACKGROUND_TILING] = TP::StringToTiling(val);
        }
        break;
      }
      case TP::NTP_LOGO_ALTERNATE: {
        int val = 0;
        if (iter.value().GetAsInteger(&val))
          temp_properties[TP::NTP_LOGO_ALTERNATE] = val;
        break;
      }
    }
  }

  // Copy data from the intermediary data structure to the array.
  size_t count = 0;
  for (std::map<int, int>::const_iterator it = temp_properties.begin();
       it != temp_properties.end() && count < kDisplayPropertiesSize;
       ++it, ++count) {
    display_properties_[count].id = it->first;
    display_properties_[count].property = it->second;
  }
}

void BrowserThemePack::ParseImageNamesFromJSON(
    const base::DictionaryValue* images_value,
    const base::FilePath& images_path,
    FilePathMap* file_paths) const {
  if (!images_value)
    return;

  for (base::DictionaryValue::Iterator iter(*images_value); !iter.IsAtEnd();
       iter.Advance()) {
    if (iter.value().is_dict()) {
      const base::DictionaryValue* inner_value = NULL;
      if (iter.value().GetAsDictionary(&inner_value)) {
        for (base::DictionaryValue::Iterator inner_iter(*inner_value);
             !inner_iter.IsAtEnd();
             inner_iter.Advance()) {
          std::string name;
          ui::ScaleFactor scale_factor = ui::SCALE_FACTOR_NONE;
          if (GetScaleFactorFromManifestKey(inner_iter.key(), &scale_factor) &&
              inner_iter.value().is_string() &&
              inner_iter.value().GetAsString(&name)) {
            AddFileAtScaleToMap(iter.key(),
                                scale_factor,
                                images_path.AppendASCII(name),
                                file_paths);
          }
        }
      }
    } else if (iter.value().is_string()) {
      std::string name;
      if (iter.value().GetAsString(&name)) {
        AddFileAtScaleToMap(iter.key(),
                            ui::SCALE_FACTOR_100P,
                            images_path.AppendASCII(name),
                            file_paths);
      }
    }
  }
}

void BrowserThemePack::AddFileAtScaleToMap(const std::string& image_name,
                                           ui::ScaleFactor scale_factor,
                                           const base::FilePath& image_path,
                                           FilePathMap* file_paths) const {
  int id = GetPersistentIDByName(image_name);
  if (id != -1)
    (*file_paths)[id][scale_factor] = image_path;
}

void BrowserThemePack::BuildSourceImagesArray(const FilePathMap& file_paths) {
  std::vector<int> ids;
  for (auto it = file_paths.begin(); it != file_paths.end(); ++it) {
    ids.push_back(it->first);
  }

  source_images_ = new int[ids.size() + 1];
  std::copy(ids.begin(), ids.end(), source_images_);
  source_images_[ids.size()] = -1;
}

bool BrowserThemePack::LoadRawBitmapsTo(
    const FilePathMap& file_paths,
    ImageCache* image_cache) {
  // Themes should be loaded on the file thread, not the UI thread.
  // http://crbug.com/61838
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  for (auto it = file_paths.begin(); it != file_paths.end(); ++it) {
    int prs_id = it->first;
    // Some images need to go directly into |image_memory_|. No modification is
    // necessary or desirable.
    bool is_copyable = false;
    for (size_t i = 0; i < arraysize(kPreloadIDs); ++i) {
      if (kPreloadIDs[i] == prs_id) {
        is_copyable = true;
        break;
      }
    }
    gfx::ImageSkia image_skia;
    for (int pass = 0; pass < 2; ++pass) {
      // Two passes: In the first pass, we process only scale factor
      // 100% and in the second pass all other scale factors. We
      // process scale factor 100% first because the first image added
      // in image_skia.AddRepresentation() determines the DIP size for
      // all representations.
      for (auto s2f = it->second.begin(); s2f != it->second.end(); ++s2f) {
        ui::ScaleFactor scale_factor = s2f->first;
        if ((pass == 0 && scale_factor != ui::SCALE_FACTOR_100P) ||
            (pass == 1 && scale_factor == ui::SCALE_FACTOR_100P)) {
          continue;
        }
        scoped_refptr<base::RefCountedMemory> raw_data(
            ReadFileData(s2f->second));
        if (!raw_data.get() || !raw_data->size()) {
          LOG(ERROR) << "Could not load theme image"
                     << " prs_id=" << prs_id
                     << " scale_factor_enum=" << scale_factor
                     << " file=" << s2f->second.value()
                     << (raw_data.get() ? " (zero size)" : " (read error)");
          return false;
        }
        if (is_copyable) {
          int raw_id = GetRawIDByPersistentID(prs_id, scale_factor);
          image_memory_[raw_id] = raw_data;
        } else {
          SkBitmap bitmap;
          if (gfx::PNGCodec::Decode(raw_data->front(), raw_data->size(),
                                    &bitmap)) {
            image_skia.AddRepresentation(
                gfx::ImageSkiaRep(bitmap,
                                  ui::GetScaleForScaleFactor(scale_factor)));
          } else {
            NOTREACHED() << "Unable to decode theme image resource "
                         << it->first;
          }
        }
      }
    }
    if (!is_copyable && !image_skia.isNull())
      (*image_cache)[prs_id] = gfx::Image(image_skia);
  }

  return true;
}

void BrowserThemePack::CropImages(ImageCache* images) const {
  for (size_t i = 0; i < arraysize(kImagesToCrop); ++i) {
    int prs_id = kImagesToCrop[i].prs_id;
    auto it = images->find(prs_id);
    if (it == images->end())
      continue;

    int crop_height = kImagesToCrop[i].max_height;
    gfx::ImageSkia image_skia = it->second.AsImageSkia();
    (*images)[prs_id] = gfx::Image(gfx::ImageSkiaOperations::ExtractSubset(
        image_skia, gfx::Rect(0, 0, image_skia.width(), crop_height)));
  }
}

void BrowserThemePack::CreateFrameImagesAndColors(ImageCache* images) {
  static constexpr struct FrameValues {
    int prs_id;
    int tint_id;
    base::Optional<int> color_id;
  } kFrameValues[] = {
      {PRS_THEME_FRAME, TP::TINT_FRAME, TP::COLOR_FRAME},
      {PRS_THEME_FRAME_INACTIVE, TP::TINT_FRAME_INACTIVE,
       TP::COLOR_FRAME_INACTIVE},
      {PRS_THEME_FRAME_OVERLAY, TP::TINT_FRAME, base::nullopt},
      {PRS_THEME_FRAME_OVERLAY_INACTIVE, TP::TINT_FRAME_INACTIVE,
       base::nullopt},
      {PRS_THEME_FRAME_INCOGNITO, TP::TINT_FRAME_INCOGNITO,
       TP::COLOR_FRAME_INCOGNITO},
      {PRS_THEME_FRAME_INCOGNITO_INACTIVE, TP::TINT_FRAME_INCOGNITO_INACTIVE,
       TP::COLOR_FRAME_INCOGNITO_INACTIVE},
  };

  // Create all the output images in a separate cache and move them back into
  // the input images because there can be name collisions.
  ImageCache temp_output;

  for (const auto frame_values : kFrameValues) {
    int src_id = frame_values.prs_id;
    // If the theme doesn't provide an image, attempt to fall back to one it
    // does.
    if (!images->count(src_id)) {
      // Fall back from inactive overlay to active overlay.
      if (src_id == PRS_THEME_FRAME_OVERLAY_INACTIVE)
        src_id = PRS_THEME_FRAME_OVERLAY;

      // Fall back from inactive incognito to active incognito.
      if (src_id == PRS_THEME_FRAME_INCOGNITO_INACTIVE)
        src_id = PRS_THEME_FRAME_INCOGNITO;

      // For all non-overlay images, fall back to PRS_THEME_FRAME as a last
      // resort.
      if (!images->count(src_id) && src_id != PRS_THEME_FRAME_OVERLAY)
        src_id = PRS_THEME_FRAME;
    }

    // Note that if the original ID and all the fallbacks are absent, the caller
    // will rely on the frame colors instead.
    const auto image = images->find(src_id);
    if (image != images->end()) {
      const gfx::Image dest_image(
          gfx::ImageSkiaOperations::CreateHSLShiftedImage(
              *image->second.ToImageSkia(),
              GetTintInternal(frame_values.tint_id)));

      temp_output[frame_values.prs_id] = dest_image;

      if (frame_values.color_id) {
        ComputeColorFromImage(frame_values.color_id.value(),
                              kTallestFrameHeight, dest_image);
      }
    }
  }
  MergeImageCaches(temp_output, images);
}

void BrowserThemePack::GenerateFrameColors() {
  SkColor frame;
  if (!GetColor(TP::COLOR_FRAME, &frame)) {
    frame = TP::GetDefaultColor(TP::COLOR_FRAME, false);
    SetColor(TP::COLOR_FRAME, HSLShift(frame, GetTintInternal(TP::TINT_FRAME)));
  }

  SkColor temp;
  if (!GetColor(TP::COLOR_FRAME_INACTIVE, &temp)) {
    SetColor(TP::COLOR_FRAME_INACTIVE,
             HSLShift(frame, GetTintInternal(TP::TINT_FRAME_INACTIVE)));
  }

  if (!GetColor(TP::COLOR_FRAME_INCOGNITO, &temp)) {
    SetColor(TP::COLOR_FRAME_INCOGNITO,
             HSLShift(frame, GetTintInternal(TP::TINT_FRAME_INCOGNITO)));
  }

  if (!GetColor(TP::COLOR_FRAME_INCOGNITO_INACTIVE, &temp)) {
    SetColor(
        TP::COLOR_FRAME_INCOGNITO_INACTIVE,
        HSLShift(frame, GetTintInternal(TP::TINT_FRAME_INCOGNITO_INACTIVE)));
  }
}

void BrowserThemePack::GenerateWindowControlButtonColor(ImageCache* images) {
  static constexpr struct ControlBGValue {
    // The color to compute and store.
    int color_id;

    // The frame color to use as the base of this button background.
    int frame_color_id;
  } kControlButtonBackgroundMap[] = {
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_ACTIVE, TP::COLOR_FRAME},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INACTIVE,
       TP::COLOR_FRAME_INACTIVE},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_ACTIVE,
       TP::COLOR_FRAME_INCOGNITO},
      {TP::COLOR_WINDOW_CONTROL_BUTTON_BACKGROUND_INCOGNITO_INACTIVE,
       TP::COLOR_FRAME_INCOGNITO_INACTIVE},
  };

  // Get data related to the control button background image and color first,
  // since they are shared by all variants.
  gfx::ImageSkia bg_image;
  ImageCache::const_iterator bg_img_it =
      images->find(PRS_THEME_WINDOW_CONTROL_BACKGROUND);
  if (bg_img_it != images->end())
    bg_image = bg_img_it->second.AsImageSkia();

  SkColor button_bg_color;
  SkAlpha button_bg_alpha = SK_AlphaTRANSPARENT;
  if (GetColor(TP::COLOR_BUTTON_BACKGROUND, &button_bg_color))
    button_bg_alpha = SkColorGetA(button_bg_color);

  button_bg_alpha =
      WindowFrameUtil::CalculateWindows10GlassCaptionButtonBackgroundAlpha(
          button_bg_alpha);

  // Determine what portion of the image to use in our calculations (we won't
  // use more along the X-axis than the width of the caption buttons).  This
  // should theoretically be the maximum of the size of the caption button area
  // on the glass frame and opaque frame, but it would be rather complicated to
  // determine the size of the opaque frame's caption button area at pack
  // processing time (as it is determined by the size of icons, which we don't
  // have easy access to here), so we use the glass frame area as an
  // approximation.
  gfx::Size dest_size =
      WindowFrameUtil::GetWindows10GlassCaptionButtonAreaSize();

  // To get an accurate sampling, all we need to do is get a representative
  // image that is at MOST the size of the caption button area.  In the case of
  // an image that is smaller - we only need to sample an area the size of the
  // provided image (trying to take tiling into account would be overkill).
  if (!bg_image.isNull()) {
    dest_size.SetToMin(bg_image.size());
  }

  for (const ControlBGValue& bg_pair : kControlButtonBackgroundMap) {
    SkColor frame_color;
    GetColor(bg_pair.frame_color_id, &frame_color);
    SkColor base_color =
        color_utils::AlphaBlend(button_bg_color, frame_color, button_bg_alpha);

    if (bg_image.isNull()) {
      SetColor(bg_pair.color_id, base_color);
      continue;
    }

    auto source = std::make_unique<ControlButtonBackgroundImageSource>(
        base_color, bg_image, dest_size);
    const gfx::Image dest_image(gfx::ImageSkia(std::move(source), dest_size));

    ComputeColorFromImage(bg_pair.color_id, dest_size.height(), dest_image);
  }
}

void BrowserThemePack::CreateTabBackgroundImagesAndColors(ImageCache* images) {
  static constexpr struct TabValues {
    // The background image to create/update.
    int tab_id;

    // For inactive images, the corresponding active image.  If the active
    // images are customized and the inactive ones are not, the inactive ones
    // will be based on the active ones.
    base::Optional<int> fallback_tab_id;

    // The frame image to use as the base of this tab background image.
    int frame_id;

    // The frame color to use as the base of this tab background image.
    int frame_color_id;

    // The color to compute and store for this image, if not present.
    int color_id;
  } kTabBackgroundMap[] = {
      {PRS_THEME_TAB_BACKGROUND, base::nullopt, PRS_THEME_FRAME,
       TP::COLOR_FRAME, TP::COLOR_BACKGROUND_TAB},
      {PRS_THEME_TAB_BACKGROUND_INACTIVE, PRS_THEME_TAB_BACKGROUND,
       PRS_THEME_FRAME_INACTIVE, TP::COLOR_FRAME_INACTIVE,
       TP::COLOR_BACKGROUND_TAB_INACTIVE},
      {PRS_THEME_TAB_BACKGROUND_INCOGNITO, base::nullopt,
       PRS_THEME_FRAME_INCOGNITO, TP::COLOR_FRAME_INCOGNITO,
       TP::COLOR_BACKGROUND_TAB_INCOGNITO},
      {PRS_THEME_TAB_BACKGROUND_INCOGNITO_INACTIVE,
       PRS_THEME_TAB_BACKGROUND_INCOGNITO, PRS_THEME_FRAME_INCOGNITO_INACTIVE,
       TP::COLOR_FRAME_INCOGNITO_INACTIVE,
       TP::COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE},
  };

  ImageCache temp_output;
  for (size_t i = 0; i < arraysize(kTabBackgroundMap); ++i) {
    const int tab_id = kTabBackgroundMap[i].tab_id;
    ImageCache::const_iterator tab_it = images->find(tab_id);

    // Inactive images should be based on the active ones if the active ones
    // were customized.
    if (tab_it == images->end() && kTabBackgroundMap[i].fallback_tab_id)
      tab_it = images->find(*kTabBackgroundMap[i].fallback_tab_id);

    // Generate background tab images when provided with custom frame or
    // background tab images; in the former case the theme author may want the
    // background tabs to appear to tint the frame, and in the latter case the
    // provided background tab image may have transparent regions, which must be
    // made opaque by overlaying atop the original frame.
    const ImageCache::const_iterator frame_it =
        images->find(kTabBackgroundMap[i].frame_id);
    if (frame_it != images->end() || tab_it != images->end()) {
      SkColor frame_color;
      GetColor(kTabBackgroundMap[i].frame_color_id, &frame_color);

      gfx::ImageSkia image_to_tint;
      if (frame_it != images->end())
        image_to_tint = (frame_it->second).AsImageSkia();

      gfx::ImageSkia overlay;
      if (tab_it != images->end())
        overlay = tab_it->second.AsImageSkia();

      auto source = std::make_unique<TabBackgroundImageSource>(
          frame_color, image_to_tint, overlay,
          GetTintInternal(TP::TINT_BACKGROUND_TAB), TP::kFrameHeightAboveTabs);
      gfx::Size dest_size = image_to_tint.size();
      dest_size.SetToMax(overlay.size());
      dest_size.set_height(kTallestTabHeight);
      const gfx::Image dest_image(gfx::ImageSkia(std::move(source), dest_size));
      temp_output[tab_id] = dest_image;

      ComputeColorFromImage(kTabBackgroundMap[i].color_id, kTallestTabHeight,
                            dest_image);
    }
  }
  MergeImageCaches(temp_output, images);
}

void BrowserThemePack::GenerateMissingTextColors() {
  constexpr int kDefaultSourceTextColorId = TP::COLOR_BACKGROUND_TAB_TEXT;

  // Background Tab
  GenerateMissingTextColorForID(TP::COLOR_BACKGROUND_TAB_TEXT,
                                TP::COLOR_BACKGROUND_TAB, TP::COLOR_FRAME,
                                kDefaultSourceTextColorId);

  // Background Tab - Inactive
  GenerateMissingTextColorForID(
      TP::COLOR_BACKGROUND_TAB_TEXT_INACTIVE, TP::COLOR_BACKGROUND_TAB_INACTIVE,
      TP::COLOR_FRAME_INACTIVE, kDefaultSourceTextColorId);

  // Incognito
  GenerateMissingTextColorForID(TP::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO,
                                TP::COLOR_BACKGROUND_TAB_INCOGNITO,
                                TP::COLOR_FRAME_INCOGNITO,
                                kDefaultSourceTextColorId);

  // Incognito - Inactive
  GenerateMissingTextColorForID(
      TP::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO_INACTIVE,
      TP::COLOR_BACKGROUND_TAB_INCOGNITO_INACTIVE,
      TP::COLOR_FRAME_INCOGNITO_INACTIVE,
      TP::COLOR_BACKGROUND_TAB_TEXT_INCOGNITO);
}

void BrowserThemePack::GenerateMissingTextColorForID(int text_color_id,
                                                     int tab_color_id,
                                                     int frame_color_id,
                                                     int source_color_id) {
  SkColor text_color, tab_color, frame_color;
  color_utils::HSL tab_tint;

  const bool has_text_color = GetColor(text_color_id, &text_color);
  const bool has_tab_color = GetColor(tab_color_id, &tab_color);
  const bool has_frame_color = GetColor(frame_color_id, &frame_color);

  const bool has_tab_tint = GetTint(TP::TINT_BACKGROUND_TAB, &tab_tint);
  const bool has_meaningful_tab_tint =
      has_tab_tint && color_utils::IsHSLShiftMeaningful(tab_tint);

  // If there is no tab color specified (also meaning there is no image), fall
  // back to the frame color.
  SkColor bg_color = (has_tab_color ? tab_color : frame_color);
  const bool has_bg_color =
      has_tab_color || has_frame_color || has_meaningful_tab_tint;

  // If no bg color is set, we have nothing to blend against, so there's no way
  // to do this calculation.
  if (!has_bg_color)
    return;

  if (has_meaningful_tab_tint && !has_tab_color) {
    // We need to tint the frame color, so if the theme didn't specify it, grab
    // the default.
    if (!has_frame_color) {
      frame_color = TP::GetDefaultColor(TP::GetLookupID(frame_color_id));
    }
    bg_color = color_utils::HSLShift(frame_color, tab_tint);
  }

  // Determine the text color to start with, in order of preference:
  // 1) The color specified by the theme (if it exists)
  // 2) The color passed in to use as a source function (if it exists)
  // 3) The default color for the text property
  SkColor blend_source_color;
  if (has_text_color) {
    blend_source_color = text_color;
  } else {
    SkColor source_text_color;
    if (GetColor(source_color_id, &source_text_color)) {
      blend_source_color = source_text_color;
    } else {
      // GetDefaultColor() requires incognito-aware lookup, so we first have to
      // get the appropriate lookup ID information.
      TP::PropertyLookupPair lookup_pair = TP::GetLookupID(text_color_id);

      blend_source_color = TP::GetDefaultColor(lookup_pair);
    }
  }

  const SkColor result_color =
      color_utils::GetColorWithMinimumContrast(blend_source_color, bg_color);
  SetColor(text_color_id, result_color);
}

void BrowserThemePack::RepackImages(const ImageCache& images,
                                    RawImages* reencoded_images) const {
  for (auto it = images.begin(); it != images.end(); ++it) {
    gfx::ImageSkia image_skia = *it->second.ToImageSkia();

    typedef std::vector<gfx::ImageSkiaRep> ImageSkiaReps;
    ImageSkiaReps image_reps = image_skia.image_reps();
    if (image_reps.empty()) {
      NOTREACHED() << "No image reps for resource " << it->first << ".";
    }
    for (auto rep_it = image_reps.begin(); rep_it != image_reps.end();
         ++rep_it) {
      std::vector<unsigned char> bitmap_data;
      if (!gfx::PNGCodec::EncodeBGRASkBitmap(rep_it->GetBitmap(), false,
                                             &bitmap_data)) {
        NOTREACHED() << "Image file for resource " << it->first
                     << " could not be encoded.";
      }
      int raw_id = GetRawIDByPersistentID(
          it->first,
          ui::GetSupportedScaleFactor(rep_it->scale()));
      (*reencoded_images)[raw_id] =
          base::RefCountedBytes::TakeVector(&bitmap_data);
    }
  }
}

void BrowserThemePack::MergeImageCaches(
    const ImageCache& source, ImageCache* destination) const {
  for (auto it = source.begin(); it != source.end(); ++it) {
    (*destination)[it->first] = it->second;
  }
}

void BrowserThemePack::AddRawImagesTo(const RawImages& images,
                                      RawDataForWriting* out) const {
  for (auto it = images.begin(); it != images.end(); ++it) {
    (*out)[it->first] = base::StringPiece(
        it->second->front_as<char>(), it->second->size());
  }
}

color_utils::HSL BrowserThemePack::GetTintInternal(int id) const {
  color_utils::HSL hsl;
  if (GetTint(id, &hsl))
    return hsl;

  int original_id = id;
  if (id == TP::TINT_FRAME_INCOGNITO)
    original_id = TP::TINT_FRAME;
  else if (id == TP::TINT_FRAME_INCOGNITO_INACTIVE)
    original_id = TP::TINT_FRAME_INACTIVE;

  return TP::GetDefaultTint(original_id, original_id != id);
}

int BrowserThemePack::GetRawIDByPersistentID(
    int prs_id,
    ui::ScaleFactor scale_factor) const {
  if (prs_id < 0)
    return -1;

  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    if (scale_factors_[i] == scale_factor)
      return ((GetMaxPersistentId() + 1) * i) + prs_id;
  }
  return -1;
}

bool BrowserThemePack::GetScaleFactorFromManifestKey(
    const std::string& key,
    ui::ScaleFactor* scale_factor) const {
  int percent = 0;
  if (base::StringToInt(key, &percent)) {
    float scale = static_cast<float>(percent) / 100.0f;
    for (size_t i = 0; i < scale_factors_.size(); ++i) {
      if (fabs(ui::GetScaleForScaleFactor(scale_factors_[i]) - scale)
              < 0.001) {
        *scale_factor = scale_factors_[i];
        return true;
      }
    }
  }
  return false;
}

void BrowserThemePack::GenerateRawImageForAllSupportedScales(int prs_id) {
  // Compute (by scaling) bitmaps for |prs_id| for any scale factors
  // for which the theme author did not provide a bitmap. We compute
  // the bitmaps using the highest scale factor that theme author
  // provided.
  // Note: We use only supported scale factors. For example, if scale
  // factor 2x is supported by the current system, but 1.8x is not and
  // if the theme author did not provide an image for 2x but one for
  // 1.8x, we will not use the 1.8x image here. Here we will only use
  // images provided for scale factors supported by the current system.

  // See if any image is missing. If not, we're done.
  bool image_missing = false;
  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    int raw_id = GetRawIDByPersistentID(prs_id, scale_factors_[i]);
    if (image_memory_.find(raw_id) == image_memory_.end()) {
      image_missing = true;
      break;
    }
  }
  if (!image_missing)
    return;

  // Find available scale factor with highest scale.
  ui::ScaleFactor available_scale_factor = ui::SCALE_FACTOR_NONE;
  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    int raw_id = GetRawIDByPersistentID(prs_id, scale_factors_[i]);
    if ((available_scale_factor == ui::SCALE_FACTOR_NONE ||
         (ui::GetScaleForScaleFactor(scale_factors_[i]) >
          ui::GetScaleForScaleFactor(available_scale_factor))) &&
        image_memory_.find(raw_id) != image_memory_.end()) {
      available_scale_factor = scale_factors_[i];
    }
  }
  // If no scale factor is available, we're done.
  if (available_scale_factor == ui::SCALE_FACTOR_NONE)
    return;

  // Get bitmap for the available scale factor.
  int available_raw_id = GetRawIDByPersistentID(prs_id, available_scale_factor);
  RawImages::const_iterator it = image_memory_.find(available_raw_id);
  SkBitmap available_bitmap;
  if (!gfx::PNGCodec::Decode(it->second->front(),
                             it->second->size(),
                             &available_bitmap)) {
    NOTREACHED() << "Unable to decode theme image for prs_id="
                 << prs_id << " for scale_factor=" << available_scale_factor;
    return;
  }

  // Fill in all missing scale factors by scaling the available bitmap.
  for (size_t i = 0; i < scale_factors_.size(); ++i) {
    int scaled_raw_id = GetRawIDByPersistentID(prs_id, scale_factors_[i]);
    if (image_memory_.find(scaled_raw_id) != image_memory_.end())
      continue;
    SkBitmap scaled_bitmap =
        CreateLowQualityResizedBitmap(available_bitmap,
                                      available_scale_factor,
                                      scale_factors_[i]);
    std::vector<unsigned char> bitmap_data;
    if (!gfx::PNGCodec::EncodeBGRASkBitmap(scaled_bitmap,
                                           false,
                                           &bitmap_data)) {
      NOTREACHED() << "Unable to encode theme image for prs_id="
                   << prs_id << " for scale_factor=" << scale_factors_[i];
      break;
    }
    image_memory_[scaled_raw_id] =
        base::RefCountedBytes::TakeVector(&bitmap_data);
  }
}
