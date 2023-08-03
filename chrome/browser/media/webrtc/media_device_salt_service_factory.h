// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_DEVICE_SALT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_DEVICE_SALT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace media_device_salt {
class MediaDeviceSaltService;
}  // namespace media_device_salt

class MediaDeviceSaltServiceFactory : public ProfileKeyedServiceFactory {
 public:
  MediaDeviceSaltServiceFactory(const MediaDeviceSaltServiceFactory&) = delete;
  MediaDeviceSaltServiceFactory& operator=(
      const MediaDeviceSaltServiceFactory&) = delete;

  // Returns the singleton instance of the MediaDeviceSaltServiceFactory.
  static MediaDeviceSaltServiceFactory* GetInstance();

  // Returns the MediaDeviceSaltService associated with |context|.
  static media_device_salt::MediaDeviceSaltService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<MediaDeviceSaltServiceFactory>;

  MediaDeviceSaltServiceFactory();
  ~MediaDeviceSaltServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MEDIA_DEVICE_SALT_SERVICE_FACTORY_H_
