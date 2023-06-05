// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/storage_access_api/storage_access_api_service_impl.h"

StorageAccessAPIServiceImpl::StorageAccessAPIServiceImpl(
    content::BrowserContext* browser_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(browser_context);
}

StorageAccessAPIServiceImpl::~StorageAccessAPIServiceImpl() = default;

void StorageAccessAPIServiceImpl::RenewPermissionGrant(
    const url::Origin& embedded_origin,
    const url::Origin& top_frame_origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(https://crbug.com/1450356): implement grant renewal.
}

void StorageAccessAPIServiceImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}
