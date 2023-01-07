// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_LOADER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/ui/app_icon_loader.h"

class Profile;

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace extensions {

// Implementation of AppIconLoader that uses ChromeAppIcon to load and update
// Chrome app images.
class ChromeAppIconLoader : public AppIconLoader, public ChromeAppIconDelegate {
 public:
  using ResizeFunction =
      base::RepeatingCallback<void(const gfx::Size&, gfx::ImageSkia*)>;

  // |resize_function| overrides icon resizing behavior if non-null. Otherwise
  // IconLoader with perform the resizing. In both cases |resource_size_in_dip|
  // is used to pick the correct icon representation from resources.
  ChromeAppIconLoader(Profile* profile,
                      int icon_size_in_dip,
                      const ResizeFunction& resize_function,
                      AppIconLoaderDelegate* delegate);
  ChromeAppIconLoader(Profile* profile,
                      int icon_size_in_dip,
                      AppIconLoaderDelegate* delegate);

  ChromeAppIconLoader(const ChromeAppIconLoader&) = delete;
  ChromeAppIconLoader& operator=(const ChromeAppIconLoader&) = delete;

  ~ChromeAppIconLoader() override;

  // AppIconLoader overrides:
  bool CanLoadImageForApp(const std::string& id) override;
  void FetchImage(const std::string& id) override;
  void ClearImage(const std::string& id) override;
  void UpdateImage(const std::string& id) override;

  // Sets |extensions_only_| as true to load icons for extensions only.
  void SetExtensionsOnly();

 private:
  using ExtensionIDToChromeAppIconMap =
      std::map<std::string, std::unique_ptr<ChromeAppIcon>>;

  // ChromeAppIconDelegate:
  void OnIconUpdated(ChromeAppIcon* icon) override;

  // Maps from extension id to ChromeAppIcon.
  ExtensionIDToChromeAppIconMap map_;

  // Function to be used to resize the image loaded from a resource. If null,
  // resize will be performed by ImageLoader.
  const ResizeFunction resize_function_;

  // Loads icons for extensions only if true, otherwise loads icon for both
  // Chrome apps and extensions.
  bool extensions_only_ = false;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_LOADER_H_
