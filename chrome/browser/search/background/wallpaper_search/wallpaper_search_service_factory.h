// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class WallpaperSearchService;
class Profile;

class WallpaperSearchServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the WallpaperSearchService for |profile|.
  //
  // Returns null if WallpaperSearch is disabled.
  static WallpaperSearchService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all WallpaperSearchService(s).
  static WallpaperSearchServiceFactory* GetInstance();

  WallpaperSearchServiceFactory(const WallpaperSearchServiceFactory&) = delete;
  WallpaperSearchServiceFactory& operator=(
      const WallpaperSearchServiceFactory&) = delete;

 private:
  friend base::NoDestructor<WallpaperSearchServiceFactory>;

  WallpaperSearchServiceFactory();
  ~WallpaperSearchServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_FACTORY_H_
