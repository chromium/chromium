// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_METRICS_CREDENTIAL_STORAGE_METRICS_H_
#define CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_METRICS_CREDENTIAL_STORAGE_METRICS_H_

#include "base/time/time.h"

namespace ash::nearby::presence::metrics {

void RecordCredentialStorageInitializationResult(bool success);

void RecordCredentialStorageLocalPublicInitializationResult(bool success);
void RecordCredentialStorageRemotePublicInitializationResult(bool success);
void RecordCredentialStoragePrivateInitializationResult(bool success);

void RecordCredentialStorageSaveLocalPublicCredentialsResult(bool success);
void RecordCredentialStorageSaveRemotePublicCredentialsResult(bool success);
void RecordCredentialStorageSavePrivateCredentialsResult(bool success);

void RecordCredentialStorageRetrieveLocalPublicCredentialsDuration(
    base::TimeDelta duration);
void RecordCredentialStorageRetrieveRemotePublicCredentialsDuration(
    base::TimeDelta duration);
void RecordCredentialStorageRetrievePrivateCredentialsDuration(
    base::TimeDelta duration);

}  // namespace ash::nearby::presence::metrics

#endif  // CHROME_BROWSER_ASH_NEARBY_PRESENCE_CREDENTIAL_STORAGE_METRICS_CREDENTIAL_STORAGE_METRICS_H_
