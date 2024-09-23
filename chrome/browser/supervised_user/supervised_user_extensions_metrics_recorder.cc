// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_extensions_metrics_recorder.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "chrome/browser/ui/supervised_user/parent_permission_dialog.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"

// static
const char SupervisedUserExtensionsMetricsRecorder::kExtensionsHistogramName[] =
    "SupervisedUsers.Extensions2";
const char
    SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedActionName[] =
        "SupervisedUsers_Extensions_ApprovalGranted";
const char SupervisedUserExtensionsMetricsRecorder::
    kPermissionsIncreaseGrantedActionName[] =
        "SupervisedUsers_Extensions_PermissionsIncreaseGranted";
const char
    SupervisedUserExtensionsMetricsRecorder::kApprovalRemovedActionName[] =
        "SupervisedUsers_Extensions_ApprovalRemoved";
const char
    SupervisedUserExtensionsMetricsRecorder::kApprovalGrantedByDefaultName[] =
        "SupervisedUsers_Extensions_ApprovalGrantedByDefault";
const char
    SupervisedUserExtensionsMetricsRecorder::kLocalApprovalGrantedName[] =
        "SupervisedUsers_Extensions_LocalApprovalGranted";
// Extension Install Dialog.
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogHistogramName[] =
        "SupervisedUsers.ExtensionInstallDialog";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogOpenedActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_Opened";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogChildCanceledActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_ChildCanceled";
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionInstallDialogChildAcceptedActionName[] =
        "SupervisedUsers_Extensions_ExtensionInstallDialog_"
        "ChildAccepted";

// Parent Permission Dialog.
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogHistogramName[] =
        "SupervisedUsers.ParentPermissionDialog";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogOpenedActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_Opened";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentApprovedActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_ParentApproved";
const char SupervisedUserExtensionsMetricsRecorder::
    kParentPermissionDialogParentCanceledActionName[] =
        "SupervisedUsers_Extensions_ParentPermissionDialog_ParentCanceled";
const char SupervisedUserExtensionsMetricsRecorder::
    kIncorrectParentPasswordProvidedActionName[] =
        "SupervisedUsers_Extensions_IncorrectParentPasswordProvided";
// Enabling and disabling extensions.
const char SupervisedUserExtensionsMetricsRecorder::kEnablementHistogramName[] =
    "SupervisedUsers.ExtensionEnablement";
const char SupervisedUserExtensionsMetricsRecorder::kEnabledActionName[] =
    "SupervisedUsers_Extensions_Enabled";
const char SupervisedUserExtensionsMetricsRecorder::kDisabledActionName[] =
    "SupervisedUsers_Extensions_Disabled";
const char
    SupervisedUserExtensionsMetricsRecorder::kFailedToEnableActionName[] =
        "SupervisedUsers_Extensions_FailedToEnable";
// Extension approval entry points.
const char SupervisedUserExtensionsMetricsRecorder::
    kExtensionParentApprovalEntryPointHistogramName[] =
        "SupervisedUsers.ExtensionParentApprovalEntryPoint";
const char SupervisedUserExtensionsMetricsRecorder::
    kImplicitParentApprovalGrantEntryPointHistogramName[] =
        "SupervisedUsers.ImplicitParentApprovalGrantEntryPoint";

SupervisedUserExtensionsMetricsRecorder::
    SupervisedUserExtensionsMetricsRecorder() = default;

void SupervisedUserExtensionsMetricsRecorder::OnDialogOpened() {
  RecordExtensionInstallDialogUmaMetrics(ExtensionInstallDialogState::kOpened);
}

void SupervisedUserExtensionsMetricsRecorder::OnDialogAccepted() {
  RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState::kChildAccepted);
}

void SupervisedUserExtensionsMetricsRecorder::OnDialogCanceled() {
  RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState::kChildCanceled);
}

// static
void SupervisedUserExtensionsMetricsRecorder::RecordExtensionsUmaMetrics(
    UmaExtensionState state) {
  base::UmaHistogramEnumeration(kExtensionsHistogramName, state);
  switch (state) {
    case UmaExtensionState::kApprovalGranted:
      // Record UMA metrics for custodian approval for a new extension.
      base::RecordAction(base::UserMetricsAction(kApprovalGrantedActionName));
      break;
    case UmaExtensionState::kPermissionsIncreaseGranted:
      // Record UMA metrics for child approval for a newer version of an
      // existing extension with increased permissions.
      base::RecordAction(
          base::UserMetricsAction(kPermissionsIncreaseGrantedActionName));
      break;
    case UmaExtensionState::kApprovalRemoved:
      // Record UMA metrics for removing an extension.
      base::RecordAction(base::UserMetricsAction(kApprovalRemovedActionName));
      break;
    case UmaExtensionState::kApprovalGrantedByDefault:
      // Record UMA metrics for auto-granting parental approval.
      base::RecordAction(
          base::UserMetricsAction(kApprovalGrantedByDefaultName));
      break;
    case UmaExtensionState::kLocalApprovalGranted:
      // Record UMA metrics for granting local parental approval.
      base::RecordAction(base::UserMetricsAction(kLocalApprovalGrantedName));
      break;
  }
}

void SupervisedUserExtensionsMetricsRecorder::
    RecordExtensionInstallDialogUmaMetrics(ExtensionInstallDialogState state) {
  base::UmaHistogramEnumeration(kExtensionInstallDialogHistogramName, state);
  switch (state) {
    case ExtensionInstallDialogState::kOpened:
      base::RecordAction(
          base::UserMetricsAction(kExtensionInstallDialogOpenedActionName));
      break;
    case ExtensionInstallDialogState::kAskedParentDeprecated:
      NOTREACHED();
    case ExtensionInstallDialogState::kChildCanceled:
      base::RecordAction(base::UserMetricsAction(
          kExtensionInstallDialogChildCanceledActionName));
      break;
    case ExtensionInstallDialogState::kChildAccepted:
      base::RecordAction(base::UserMetricsAction(
          kExtensionInstallDialogChildAcceptedActionName));
      break;
  }
}

void SupervisedUserExtensionsMetricsRecorder::
    RecordParentPermissionDialogUmaMetrics(ParentPermissionDialogState state) {
  base::UmaHistogramEnumeration(kParentPermissionDialogHistogramName, state);
  switch (state) {
    case ParentPermissionDialogState::kOpened:
      base::RecordAction(
          base::UserMetricsAction(kParentPermissionDialogOpenedActionName));
      break;
    case ParentPermissionDialogState::kParentApproved:
      base::RecordAction(base::UserMetricsAction(
          kParentPermissionDialogParentApprovedActionName));
      break;
    case ParentPermissionDialogState::kParentCanceled:
      base::RecordAction(base::UserMetricsAction(
          kParentPermissionDialogParentCanceledActionName));
      break;
    case ParentPermissionDialogState::kIncorrectParentPasswordProvided:
      base::RecordAction(
          base::UserMetricsAction(kIncorrectParentPasswordProvidedActionName));
      break;
    case ParentPermissionDialogState::kFailed:
    case ParentPermissionDialogState::kNoParentError:
      // Nothing to do here.
      break;
  }
}

// static
void SupervisedUserExtensionsMetricsRecorder::
    RecordExtensionParentApprovalDialogEntryPointUmaMetrics(
        SupervisedUserExtensionParentApprovalEntryPoint
            extension_approval_entry_point) {
  base::UmaHistogramEnumeration(kExtensionParentApprovalEntryPointHistogramName,
                                extension_approval_entry_point);
}

// static
void SupervisedUserExtensionsMetricsRecorder::
    RecordImplicitParentApprovalGrantEntryPointEntryPointUmaMetrics(
        ImplicitExtensionApprovalEntryPoint extension_approval_entry_point) {
  base::UmaHistogramEnumeration(
      kImplicitParentApprovalGrantEntryPointHistogramName,
      extension_approval_entry_point);
}

// static
void SupervisedUserExtensionsMetricsRecorder::RecordEnablementUmaMetrics(
    EnablementState state) {
  base::UmaHistogramEnumeration(kEnablementHistogramName, state);
  switch (state) {
    case EnablementState::kEnabled:
      base::RecordAction(base::UserMetricsAction(kEnabledActionName));
      break;
    case EnablementState::kDisabled:
      base::RecordAction(base::UserMetricsAction(kDisabledActionName));
      break;
    case EnablementState::kFailedToEnable:
      base::RecordAction(base::UserMetricsAction(kFailedToEnableActionName));
      break;
  }
}
