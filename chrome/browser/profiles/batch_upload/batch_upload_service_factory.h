// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class BatchUploadService;

// Singleton that manages the `BatchUploadService` per `Profile`.
class BatchUploadServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BatchUploadService* GetForProfile(Profile* profile);

  // Returns an instance of the BatchUploadServiceFactory singleton.
  static BatchUploadServiceFactory* GetInstance();

  BatchUploadServiceFactory(const BatchUploadServiceFactory&) = delete;
  BatchUploadServiceFactory& operator=(const BatchUploadServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<BatchUploadServiceFactory>;

  BatchUploadServiceFactory();
  ~BatchUploadServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PROFILES_BATCH_UPLOAD_BATCH_UPLOAD_SERVICE_FACTORY_H_
