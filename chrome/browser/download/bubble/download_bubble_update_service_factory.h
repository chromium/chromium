// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class DownloadBubbleUpdateService;
class Profile;

class DownloadBubbleUpdateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  DownloadBubbleUpdateServiceFactory(
      const DownloadBubbleUpdateServiceFactory&) = delete;
  DownloadBubbleUpdateServiceFactory& operator=(
      const DownloadBubbleUpdateServiceFactory&) = delete;

  // Returns the singleton instance of DownloadBubbleUpdateServiceFactory.
  static DownloadBubbleUpdateServiceFactory* GetInstance();

  // Returns the DownloadBubbleUpdateService associated with |profile|.
  static DownloadBubbleUpdateService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<DownloadBubbleUpdateServiceFactory>;

  DownloadBubbleUpdateServiceFactory();
  ~DownloadBubbleUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DOWNLOAD_BUBBLE_DOWNLOAD_BUBBLE_UPDATE_SERVICE_FACTORY_H_
