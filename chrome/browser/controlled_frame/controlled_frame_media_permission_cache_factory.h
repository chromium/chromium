// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_FACTORY_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace controlled_frame {

class ControlledFrameMediaPermissionCache;

class ControlledFrameMediaPermissionCacheFactory
    : public ProfileKeyedServiceFactory {
 public:
  static ControlledFrameMediaPermissionCache* GetForBrowserContext(
      content::BrowserContext* context);

  static ControlledFrameMediaPermissionCacheFactory* GetInstance();

  ControlledFrameMediaPermissionCacheFactory(
      const ControlledFrameMediaPermissionCacheFactory&) = delete;
  ControlledFrameMediaPermissionCacheFactory& operator=(
      const ControlledFrameMediaPermissionCacheFactory&) = delete;

 private:
  friend base::NoDestructor<ControlledFrameMediaPermissionCacheFactory>;

  ControlledFrameMediaPermissionCacheFactory();
  ~ControlledFrameMediaPermissionCacheFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MEDIA_PERMISSION_CACHE_FACTORY_H_
