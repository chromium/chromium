// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
#define CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_

#include "chrome/browser/extensions/extension_install_prompt.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

// Records UMA metrics for supervised users using extensions.
class SupervisedUserExtensionsMetricsRecorder
    : public ExtensionInstallPrompt::Observer {
 public:
  // These enum values represent the state that the child user has attained
  // while trying to install an extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(UmaExtensionState)
  enum class UmaExtensionState {
    // Recorded when custodian grants child approval to install an extension.
    kApprovalGranted = 0,
    // Recorded when the child approves a new version of an existing extension
    // with increased permissions.
    kPermissionsIncreaseGranted = 1,
    // Recorded when the child removes an extension.
    kApprovalRemoved = 2,
    // Recorded when an extension receives automatic parent approval, when
    // it is installed under the `SkipParentApprovalToInstallExtensions` mode
    // with the corresponding preference enabled.
    kApprovalGrantedByDefault = 3,
    // Recorded when an extension receives local parent approval, when
    // the `SkipParentApprovalToInstallExtensions` feature is first
    // released on Desktop (Windows/Linux/Mac).
    kLocalApprovalGranted = 4,
    // Add future entries above this comment, updating kMaxValue to the last
    // value.
    kMaxValue = kLocalApprovalGranted
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserExtension2)

  // These enum values represent the state of the Extension Install Dialog for
  // installing and enabling extensions for supervised users.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ExtensionInstallDialogState)
  enum class ExtensionInstallDialogState {
    // Recorded when the extension install dialog opens.
    kOpened = 0,
    // Recorded when the child clicks "Ask a parent".
    // Deprecated, the Extension install dialog does not display this button
    // anymore.
    // It was used in ChromeOS v1 extension installation flow.
    kAskedParentDeprecated = 1,
    // Recorded when the child cancels the extension installation.
    kChildCanceled = 2,
    // Recorded when the child proceeds with the extension installation dialog.
    // Under the Skip parent approval move, the `Accept` action is the
    // installation of the extension without parent intervention.
    kChildAccepted = 3,
    // Add future entries above this comment, updating kMaxValue to the last
    // value.
    kMaxValue = kChildAccepted
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserExtensionInstallDialog)

  // These enum values represent the state of the Parent Permission Dialog for
  // installing and enabling extensions for supervised users.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserParentPermissionDialog" in
  // src/tools/metrics/histograms/enums.xml.
  enum class ParentPermissionDialogState {
    // Recorded when the parent permission dialog opens.
    kOpened = 0,
    // Recorded when the parent enters their password and successfully approves
    // the extension install.
    kParentApproved = 1,
    // Recorded when the parent cancels the extension installation, denying the
    // supervised user's attempt.
    kParentCanceled = 2,
    // Recorded when there was some sort of failure in the Parent Permission
    // Dialog.
    kFailed = 3,
    // Recorded when the supervised user has no parents, an error. Note that
    // this error triggers the kFailed metric as well.
    kNoParentError = 4,
    // Recorded when the parent provides a wrong password.
    kIncorrectParentPasswordProvided = 5,
    // Add future entries above this comment, in sync with
    // "SupervisedUserParentPermissionDialog" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kIncorrectParentPasswordProvided
  };

  // These enum values represent supervised user actions to enable or disable an
  // extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused. Please keep in sync with
  // "SupervisedUserExtensionEnablement" in
  // src/tools/metrics/histograms/enums.xml.
  enum class EnablementState {
    // Recorded when the child successfully enables an approved extension.
    kEnabled = 0,
    // Recorded when the child successfully disables an approved extension.
    // Note that a disable attempt can't fail, because there are no
    // force-enabled extensions for supervised users.
    kDisabled = 1,
    // Recorded when the child tries to enable a force-disabled extension and
    // fails.
    kFailedToEnable = 2,
    // Add future entries above this comment, in sync with
    // "SupervisedUserExtensionEnablement" in
    // src/tools/metrics/histograms/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = kFailedToEnable
  };

  // These enum values represent the supervised user flows that grant
  // automatic parent approval to an extension.
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(ImplicitExtensionApprovalEntryPoint)
  enum class ImplicitExtensionApprovalEntryPoint : int {
    // Recorded when an extension is granted it,
    // when the Family Link "Extensions" toggle if flipped to On.
    kOnExtensionsSwitchFlippedToEnabled = 0,
    // Recorded when an extension is granted parent approval at the
    // time of its installation, when the "Extensions" toggle set to On.
    OnExtensionInstallationWithExtensionsSwitchEnabled = 1,
    // Add future entries above this comment, in sync with
    // "SupervisedUserAutomaticParentApprovalGrantEntryPoint" in
    // src/tools/metrics/histograms/metadata/families/enums.xml.
    // Update kMaxValue to the last value.
    kMaxValue = OnExtensionInstallationWithExtensionsSwitchEnabled
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserImplicitParentApprovalGrantEntryPoint)

  // UMA metrics for adding to or removing from the set of approved extension
  // ids in the kSupervisedUserApprovedExtensions synced pref.
  // They should be kept in sync with entries on
  // tools/metrics/actions/actions.xml.
  static const char kExtensionsHistogramName[];
  static const char kApprovalGrantedActionName[];
  static const char kPermissionsIncreaseGrantedActionName[];
  static const char kApprovalRemovedActionName[];
  static const char kApprovalGrantedByDefaultName[];
  static const char kLocalApprovalGrantedName[];
  static const char kIncorrectParentPasswordProvidedActionName[];
  // UMA metrics for the Extension Install Dialog.
  static const char kExtensionInstallDialogHistogramName[];
  static const char kExtensionInstallDialogOpenedActionName[];
  static const char kExtensionInstallDialogChildCanceledActionName[];
  static const char kExtensionInstallDialogChildAcceptedActionName[];
  // UMA metrics for the Parent Permission Dialog.
  static const char kParentPermissionDialogHistogramName[];
  static const char kParentPermissionDialogOpenedActionName[];
  static const char kParentPermissionDialogParentApprovedActionName[];
  static const char kParentPermissionDialogParentCanceledActionName[];

  // UMA metrics for enabling or disabling extensions.
  static const char kEnablementHistogramName[];
  static const char kEnabledActionName[];
  static const char kDisabledActionName[];
  static const char kFailedToEnableActionName[];

  // UMA metrics for tracking approval entry points.
  static const char kImplicitParentApprovalGrantEntryPointHistogramName[];
  static const char kExtensionParentApprovalEntryPointHistogramName[];

  SupervisedUserExtensionsMetricsRecorder();
  ~SupervisedUserExtensionsMetricsRecorder() override = default;
  SupervisedUserExtensionsMetricsRecorder(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;
  SupervisedUserExtensionsMetricsRecorder& operator=(
      const SupervisedUserExtensionsMetricsRecorder&) = delete;

  // ExtensionInstallPrompt::Observer:
  void OnDialogOpened() override;
  void OnDialogAccepted() override;
  void OnDialogCanceled() override;

  // Record UMA metrics related to adding or removing extension approvals.
  static void RecordExtensionsUmaMetrics(UmaExtensionState state);

  // Record UMA metrics related to the Extension Install Dialog.
  void RecordExtensionInstallDialogUmaMetrics(
      ExtensionInstallDialogState state);

  // Record UMA metrics related to the Parent Permission Dialog.
  void RecordParentPermissionDialogUmaMetrics(
      ParentPermissionDialogState state);

  // Record UMA metrics related to the entry point that grants implicit parent
  // approval to an extension.
  static void RecordImplicitParentApprovalGrantEntryPointEntryPointUmaMetrics(
      ImplicitExtensionApprovalEntryPoint extension_approval_entry_point);

  // Record UMA metrics related to the entry point leading to the display of the
  // Parent Approval Dialog.
  static void RecordExtensionParentApprovalDialogEntryPointUmaMetrics(
      SupervisedUserExtensionParentApprovalEntryPoint
          extension_approval_entry_point);

  // Records when the supervised user enables or disables an approved extension.
  static void RecordEnablementUmaMetrics(EnablementState state);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_SUPERVISED_USER_EXTENSIONS_METRICS_RECORDER_H_
