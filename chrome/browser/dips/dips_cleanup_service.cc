// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dips/dips_cleanup_service.h"

#include "chrome/browser/dips/dips_cleanup_service_factory.h"
#include "chrome/browser/dips/dips_features.h"
#include "chrome/browser/dips/dips_storage.h"

DIPSCleanupService::DIPSCleanupService(content::BrowserContext* context) {
  DCHECK(!base::FeatureList::IsEnabled(dips::kFeature));
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
