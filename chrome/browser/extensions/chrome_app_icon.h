// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/chromeos_buildflags.h"
#include "extensions/browser/extension_icon_image.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
class ChromeAppIconDelegate;

// This represents how an extension app icon should finally look. As a base,
// extension icon is used and effects that depend on extension type, state and
// some external conditions are applied. Resulting image is sent via
// ChromeAppIconDelegate. Several updates are expected in case extension
// state or some external conditions are changed.
class ChromeAppIcon : public IconImage::Observer {
 public:
  using DestroyedCallback = base::OnceCallback<void(ChromeAppIcon*)>;
  using ResizeFunction =
      base::RepeatingCallback<void(const gfx::Size&, gfx::ImageSkia*)>;

  // Type of badges that can be applied to app icons.
  enum class Badge {
    kNone,     // No badge applied
    kChrome,   // Applied to Chrome apps that have ARC++ 'duplicate' installed.
    kBlocked,  // Applied to disabled apps.
    kPaused    // Applied to apps that run out of daily time limit.
  };

  // Applies image processing effects to |image_skia|, such as resizing, adding
  // badges, converting to gray and rounding corners.
  static void ApplyEffects(int resource_size_in_dip,
                           const ResizeFunction& resize_function,
                           bool app_launchable,
                           bool rounded_corners,
                           Badge badge_type,
                           gfx::ImageSkia* image_skia);

  // |resize_function| overrides icon resizing behavior if non-null. Otherwise
  // IconLoader with perform the resizing. In both cases |resource_size_in_dip|
  // is used to pick the correct icon representation from resources.
  ChromeAppIcon(ChromeAppIconDelegate* delegate,
                content::BrowserContext* browser_context,
                DestroyedCallback destroyed_callback,
                const std::string& app_id,
                int resource_size_in_dip,
                const ResizeFunction& resize_function);

  ChromeAppIcon(const ChromeAppIcon&) = delete;
  ChromeAppIcon& operator=(const ChromeAppIcon&) = delete;

  ~ChromeAppIcon() override;

  // Reloads icon.
  void Reload();

  // Returns true if the icon still refers to existing extension. Once extension
  // is disabled it is discarded from the icon.
  bool IsValid() const;

  // Re-applies app effects over the current extension icon and dispatches the
  // result via |delegate_|.
  void UpdateIcon();

  const gfx::ImageSkia& image_skia() const { return image_skia_; }
  const std::string& app_id() const { return app_id_; }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Returns whether the icon is badged because it's an extension app that has
  // its Android analog installed.
  bool has_chrome_badge() const { return has_chrome_badge_; }
#endif

 private:
  const Extension* GetExtension();

  // IconImage::Observer:
  void OnExtensionIconImageChanged(IconImage* image) override;

  // Unowned pointers.
  const raw_ptr<ChromeAppIconDelegate> delegate_;
  const raw_ptr<content::BrowserContext, AcrossTasksDanglingUntriaged>
      browser_context_;

  // Called when this instance of ChromeAppIcon is destroyed.
  DestroyedCallback destroyed_callback_;

  const std::string app_id_;

  // Contains current icon image. This is static image with applied effects and
  // it is updated each time when |icon_| is updated.
  gfx::ImageSkia image_skia_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Whether the icon got badged because it's an extension app that has its
  // Android analog installed.
  bool has_chrome_badge_ = false;
#endif

  const int resource_size_in_dip_;

  // Function to be used to resize the image loaded from a resource. If null,
  // resize will be performed by ImageLoader.
  const ResizeFunction resize_function_;

  std::unique_ptr<IconImage> icon_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_H_
