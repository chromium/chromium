// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/batch_upload/batch_upload_service_factory.h"

#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/profile.h"

BatchUploadServiceFactory::BatchUploadServiceFactory()
    : ProfileKeyedServiceFactory("BatchUpload") {}

BatchUploadServiceFactory::~BatchUploadServiceFactory() = default;

// static
BatchUploadService* BatchUploadServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BatchUploadService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BatchUploadServiceFactory* BatchUploadServiceFactory::GetInstance() {
  static base::NoDestructor<BatchUploadServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
BatchUploadServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  // TODO(b/359146556): consider passing in the needed services instead of the
  // `profile` when the providers will be implemented.
  return std::make_unique<BatchUploadService>(
      *Profile::FromBrowserContext(context));
}
