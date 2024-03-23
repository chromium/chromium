// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_
#define CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_

#include <string_view>

#include "base/memory/ref_counted.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/color/color_provider_key.h"

namespace base {
class RefCountedMemory;
}

namespace color_utils {
struct HSL;
}

namespace gfx {
class Image;
}

namespace ui {
class ColorProvider;
class NativeTheme;
}

// A representation of a theme. All theme properties can be accessed through the
// public methods. Subclasses are expected to override all methods which should
// provide non-default values.
class CustomThemeSupplier
    : public ui::ColorProviderKey::ThemeInitializerSupplier {
 public:
  using ThemeInitializerSupplier::ThemeInitializerSupplier;
  CustomThemeSupplier(const CustomThemeSupplier&) = delete;
  CustomThemeSupplier& operator=(const CustomThemeSupplier&) = delete;

  const std::string& extension_id() const {
    DCHECK_EQ(get_theme_type(), ThemeType::kExtension);
    return extension_id_;
  }

  // Called when the theme starts being used.
  virtual void StartUsingTheme();

  // Called when the theme is not used anymore.
  virtual void StopUsingTheme();

  // If the theme specifies data for the corresponding |id|, returns true and
  // writes the corresponding value to the output parameter. These methods
  // should not return the default data. These methods should only be called
  // from the UI thread.
  bool GetTint(int id, color_utils::HSL* hsl) const override;
  bool GetColor(int id, SkColor* color) const override;
  bool GetDisplayProperty(int id, int* result) const override;

  // Returns the theme image for |id|. Returns an empty image if no image is
  // found for |id|.
  virtual gfx::Image GetImageNamed(int id) const;

  // Returns the raw PNG encoded data for IDR_THEME_NTP_*. This method only
  // works for the NTP attribution and background resources.
  virtual base::RefCountedMemory* GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const;

  // Whether this theme provides an image for |id|.
  bool HasCustomImage(int id) const override;

  // Returns whether or not the default incognito colors can be used with this
  // theme. This is a workaround for the IncreasedContrastThemeSupplier that
  // doesn't supply all the colors it should (http://crbug.com/1045630).
  virtual bool CanUseIncognitoColors() const;

  // ui::ColorProviderKey::ThemeInitializerSupplier:
  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override {
    // TODO(pkasting): All classes that override GetColor() should override
    // this.
  }

  virtual ui::NativeTheme* GetNativeTheme() const;

 protected:
  ~CustomThemeSupplier() override;

  void set_extension_id(std::string_view id) {
    DCHECK_EQ(get_theme_type(), ThemeType::kExtension);
    extension_id_ = id;
  }

 private:
  friend class base::RefCountedThreadSafe<CustomThemeSupplier>;

  extensions::ExtensionId extension_id_;
};

#endif  // CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_
