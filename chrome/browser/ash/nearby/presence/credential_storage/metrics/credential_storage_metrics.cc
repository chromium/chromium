// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/nearby/presence/credential_storage/metrics/credential_storage_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace ash::nearby::presence::metrics {

void RecordCredentialStorageInitializationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.Initialization.Result", success);
}

void RecordCredentialStorageLocalPublicInitializationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage."
      "LocalPublicDatabaseInitializationResult",
      success);
}

void RecordCredentialStorageRemotePublicInitializationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage."
      "RemotePublicDatabaseInitializationResult",
      success);
}

void RecordCredentialStoragePrivateInitializationResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.PrivateDatabaseInitializationResult",
      success);
}

void RecordCredentialStorageLocalPublicDatabaseInitializationDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage."
      "LocalPublicDatabaseInitializationDuration",
      duration);
}

void RecordCredentialStorageRemotePublicDatabaseInitializationDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage."
      "RemotePublicDatabaseInitializationDuration",
      duration);
}

void RecordCredentialStoragePrivateDatabaseInitializationDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage."
      "PrivateDatabaseInitializationDuration",
      duration);
}

void RecordCredentialStorageSaveLocalPublicCredentialsResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.SaveLocalPublicCredentials.Result",
      success);
}

void RecordCredentialStorageSaveRemotePublicCredentialsResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.SaveRemotePublicCredentials.Result",
      success);
}

void RecordCredentialStorageSavePrivateCredentialsResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.SavePrivateCredentials.Result",
      success);
}

void RecordCredentialStorageRetrieveLocalPublicCredentialsResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.RetrieveLocalPublicCredentials."
      "Result",
      success);
}

void RecordCredentialStorageRetrieveRemotePublicCredentialsResult(
    bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.RetrieveRemotePublicCredentials."
      "Result",
      success);
}

void RecordCredentialStorageRetrievePrivateCredentialsResult(bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Presence.Credentials.Storage.RetrievePrivateCredentials.Result",
      success);
}

void RecordCredentialStorageRetrieveLocalPublicCredentialsDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveLocalPublicCredentialsDuration",
      duration);
}

void RecordCredentialStorageRetrieveRemotePublicCredentialsDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage."
      "RetrieveRemotePublicCredentialsDuration",
      duration);
}

void RecordCredentialStorageRetrievePrivateCredentialsDuration(
    base::TimeDelta duration) {
  base::UmaHistogramMicrosecondsTimes(
      "Nearby.Presence.Credentials.Storage.RetrievePrivateCredentialsDuration",
      duration);
}

void RecordNumberOfLocalSharedCredentials(int num_credentials) {
  base::UmaHistogramCounts100(
      "Nearby.Presence.Credentials.Storage.LocalSharedCredentials.Count",
      num_credentials);
}

void RecordNumberOfRemoteSharedCredentials(int num_credentials) {
  base::UmaHistogramCounts100(
      "Nearby.Presence.Credentials.Storage.RemoteSharedCredentials.Count",
      num_credentials);
}

void RecordSizeOfLocalSharedCredentials(size_t credentials_size_in_bytes) {
  base::UmaHistogramMemoryKB(
      "Nearby.Presence.Credentials.Storage.LocalSharedCredentials.Size",
      credentials_size_in_bytes);
}

void RecordSizeOfRemoteSharedCredentials(size_t credentials_size_in_bytes) {
  base::UmaHistogramMemoryKB(
      "Nearby.Presence.Credentials.Storage.RemoteSharedCredentials.Size",
      credentials_size_in_bytes);
}

}  // namespace ash::nearby::presence::metrics
