// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_cleanup_service.h"

#include "chrome/browser/dips/dips_cleanup_service_factory.h"
#include "chrome/browser/dips/dips_storage.h"
#include "content/public/common/content_features.h"

DIPSCleanupService::DIPSCleanupService(content::BrowserContext* context) {
  DCHECK(!base::FeatureList::IsEnabled(features::kDIPS));
  DIPSStorage::DeleteDatabaseFiles(
      GetDIPSFilePath(context),
      base::BindOnce(&DIPSCleanupService::OnCleanupFinished,
                     weak_factory_.GetWeakPtr()));
}

DIPSCleanupService::~DIPSCleanupService() = default;

/* static */
DIPSCleanupService* DIPSCleanupService::Get(content::BrowserContext* context) {
  return DIPSCleanupServiceFactory::GetForBrowserContext(context);
}

void DIPSCleanupService::WaitOnCleanupForTesting() {
  wait_for_cleanup_.Run();
}

void DIPSCleanupService::OnCleanupFinished() {
  wait_for_cleanup_.Quit();
}
