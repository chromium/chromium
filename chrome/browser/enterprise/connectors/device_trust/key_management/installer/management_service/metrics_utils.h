// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_METRICS_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_METRICS_UTILS_H_

namespace enterprise_connectors {

// Possible failures of the management service binary. This must be kept in
// sync with the DTManagementServiceError UMA enum.
enum class ManagementServiceError {
  kManagementGroupIdDoesNotExist,
  kBinaryMissingManagementGroupID,
  kFilePathResolutionFailure,
  kPipeNameRetrievalFailure,
  kCommandMissingPipeName,
  kCommandMissingRotateDTKey,
  kCommandMissingDMServerUrl,
  kInvalidPendingUrlLoaderFactory,
  kUnBoundUrlLoaderFactory,
  kDisconnectedUrlLoaderFactory,
  kInvalidRotateCommand,
  kIncorrectlyEncodedArgument,
  kInvalidPlatformChannelEndpoint,
  kInvalidMojoInvitation,
  kInvalidMessagePipeHandle,
  kMaxValue = kInvalidMessagePipeHandle,
};

// Records the `error` of any invalid rotation commands, mojo failures, and
// binary permission errors for the management service binary during the
// key rotation.
void RecordError(ManagementServiceError error);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_MANAGEMENT_SERVICE_METRICS_UTILS_H_
