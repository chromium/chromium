// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_H_

#include <map>
#include <memory>
#include <set>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/shelf/shelf_extension_app_updater.h"
#endif

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace extensions {

class ChromeAppIcon;
class ChromeAppIconDelegate;

// Factory for ChromeAppIcon. Each created icon is tracked by this service.
// Once some condition that affects how extension app icon should look is
// changed then corresponded app icons are automatically updated. This service
// is bound to content::BrowserContext.
// Usage: ChromeAppIconService::Get(context)->CreateIcon().
class ChromeAppIconService : public KeyedService,
#if BUILDFLAG(IS_CHROMEOS_ASH)
                             public ShelfAppUpdater::Delegate,
#endif
                             public ExtensionRegistryObserver {
 public:
  using ResizeFunction =
      base::RepeatingCallback<void(const gfx::Size&, gfx::ImageSkia*)>;

  explicit ChromeAppIconService(content::BrowserContext* context);

  ChromeAppIconService(const ChromeAppIconService&) = delete;
  ChromeAppIconService& operator=(const ChromeAppIconService&) = delete;

  ~ChromeAppIconService() override;

  // Convenience function to get the ChromeAppIconService for a
  // BrowserContext.
  static ChromeAppIconService* Get(content::BrowserContext* context);

  // Creates extension app icon for requested app and size. Icon updates are
  // dispatched via |delegate|.
  // |resize_function| overrides icon resizing behavior if non-null. Otherwise
  // IconLoader with perform the resizing. In both cases |resource_size_in_dip|
  // is used to pick the correct icon representation from resources.
  std::unique_ptr<ChromeAppIcon> CreateIcon(
      ChromeAppIconDelegate* delegate,
      const std::string& app_id,
      int resource_size_in_dip,
      const ResizeFunction& resize_function);

  std::unique_ptr<ChromeAppIcon> CreateIcon(ChromeAppIconDelegate* delegate,
                                            const std::string& app_id,
                                            int resource_size_in_dip);

  // KeyedService:
  void Shutdown() override;

 private:
  class Updater;

  // System may have multiple icons for the same app id with different
  // dimensions. For example icon in shelf and app launcher.
  using IconMap =
      std::map<std::string, std::set<raw_ptr<ChromeAppIcon, SetExperimental>>>;

  // Called from ChromeAppIcon DTOR.
  void OnIconDestroyed(ChromeAppIcon* icon);

  // Called from Updater when corresponded app icons need to be updated.
  void OnAppUpdated(const std::string& app_id);

  // ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ShelfAppUpdater::Delegate:
  void OnAppUpdated(content::BrowserContext* browser_context,
                    const std::string& app_id,
                    bool reload_icon) override;
#endif

  // Unowned pointer.
  raw_ptr<content::BrowserContext> context_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS this handles Chrome app life-cycle events that may change how
  // extension based app icon looks like.
  std::unique_ptr<ShelfExtensionAppUpdater> app_updater_;
#endif

  // Deletes the icon set for |app_id| from the map if it is empty.
  void MaybeCleanupIconSet(const std::string& app_id);

  IconMap icon_map_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      observation_{this};

  base::WeakPtrFactory<ChromeAppIconService> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_APP_ICON_SERVICE_H_
