// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profile_resetter/reset_report_uploader_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"

// static
ResetReportUploaderFactory* ResetReportUploaderFactory::GetInstance() {
  static base::NoDestructor<ResetReportUploaderFactory> instance;
  return instance.get();
}

// static
ResetReportUploader* ResetReportUploaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ResetReportUploader*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

ResetReportUploaderFactory::ResetReportUploaderFactory()
    : ProfileKeyedServiceFactory(
          "ResetReportUploaderFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

ResetReportUploaderFactory::~ResetReportUploaderFactory() = default;

std::unique_ptr<KeyedService>
ResetReportUploaderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ResetReportUploader>(
      context->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess());
}
