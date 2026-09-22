// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_
#define CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_

#include <string_view>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
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
}  // namespace ui

// A representation of a theme. All theme properties can be accessed through the
// public methods. Subclasses are expected to override all methods which should
// provide non-default values. This class also connects those theme properties
// with Chrome's color pipeline via AddColorMixers(), and provides protected
// helpers for subclasses that store colors to mutate and propagate frame and
// toolbar colors.
class CustomThemeSupplier
    : public ui::ColorProviderKey::ThemeInitializerSupplier {
 public:
  using ThemeInitializerSupplier::ThemeInitializerSupplier;
  CustomThemeSupplier(const CustomThemeSupplier&) = delete;
  CustomThemeSupplier& operator=(const CustomThemeSupplier&) = delete;

  // The ID of the extension this theme was installed from. Defaults to
  // ThemeHelper::kDefaultThemeID unless get_theme_type() is kExtension; the ID
  // itself is stored by BrowserThemePack, which is the only supplier built from
  // an extension.
  virtual std::string_view extension_id() const;

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
  virtual scoped_refptr<base::RefCountedMemory> GetRawData(
      int id,
      ui::ResourceScaleFactor scale_factor) const;

  // Whether this theme provides an image for |id|.
  bool HasCustomImage(int id) const override;

  // Maps the theme properties returned by GetColor() and GetDisplayProperty()
  // onto Chrome's color pipeline.
  //
  // ui::ColorProviderKey::ThemeInitializerSupplier:
  void AddColorMixers(ui::ColorProvider* provider,
                      const ui::ColorProviderKey& key) const override;

  virtual ui::NativeTheme* GetNativeTheme() const;

 protected:
  ~CustomThemeSupplier() override;

  // Sets the color for `id`. Must be overridden by subclasses that call
  // SetColorIfUnspecified() or SetFrameAndToolbarRelatedColors(); subclasses
  // that do not store colors (e.g. SystemThemeLinux) inherit a NOTREACHED().
  virtual void SetColor(int id, SkColor color);

  // Sets the color for `id` only if GetColor(id, ...) returns false.
  void SetColorIfUnspecified(int id, SkColor color);

  // Sets frame, toolbar, and related colors (e.g. text, button icon, omnibox,
  // tab foreground) based on the colors and tints currently set on `this`.
  // Called by subclasses after setting primary theme colors.
  void SetFrameAndToolbarRelatedColors();

 private:
  friend class base::RefCountedThreadSafe<CustomThemeSupplier>;
};

#endif  // CHROME_BROWSER_THEMES_CUSTOM_THEME_SUPPLIER_H_
