// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_
#define CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace multi_capture {

class MultiCaptureDataService;

// This factory reacts to profile creation and instantiates profile-keyed
// services that manages policy and component data for the `getAllScreensMedia`
// API.
class MultiCaptureDataServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static MultiCaptureDataService* GetForProfile(Profile* context);
  static MultiCaptureDataServiceFactory* GetInstance();

  MultiCaptureDataServiceFactory(const MultiCaptureDataServiceFactory&) =
      delete;
  MultiCaptureDataServiceFactory& operator=(
      const MultiCaptureDataServiceFactory&) = delete;

 private:
  friend base::NoDestructor<MultiCaptureDataServiceFactory>;

  MultiCaptureDataServiceFactory();
  ~MultiCaptureDataServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace multi_capture

#endif  // CHROME_BROWSER_MEDIA_WEBRTC_MULTI_CAPTURE_MULTI_CAPTURE_DATA_SERVICE_FACTORY_H_
