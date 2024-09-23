# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for Android Java code.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.

This presubmit checks for the following:
  - No new calls to Notification.Builder or NotificationCompat.Builder
    constructors. Callers should use NotificationWrapperBuilder instead.
  - No new calls to AlertDialog.Builder. Callers should use ModalDialogView
    instead.
"""

import re

NEW_NOTIFICATION_BUILDER_RE = re.compile(
    r'\bnew\sNotification(Compat)?\.Builder\b')

IMPORT_APP_COMPAT_ALERTDIALOG_RE = re.compile(
    r'\bimport\sandroid\.support\.v7\.app\.AlertDialog;')

NEW_COMPATIBLE_ALERTDIALOG_BUILDER_RE = re.compile(
    r'\bnew\s+(UiUtils\s*\.)?CompatibleAlertDialogBuilder\b')

NEW_ALERTDIALOG_BUILDER_RE = re.compile(
    r'\bnew\sAlertDialog\.Builder\b')

SPLIT_COMPAT_UTILS_IMPL_NAME_RE = re.compile(
    r'\bBundleUtils\.getIdentifierName\(\s*[^\s"]')

COMMENT_RE = re.compile(r'^\s*(//|/\*|\*)')

BROWSER_ROOT = 'chrome/android/java/src/org/chromium/chrome/browser/'


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckNotificationConstructors(input_api, output_api))
  result.extend(_CheckAlertDialogBuilder(input_api, output_api))
  result.extend(_CheckCompatibleAlertDialogBuilder(input_api, output_api))
  result.extend(_CheckBundleUtilsIdentifierName(input_api, output_api))
  # Add more checks here
  return result


def _CheckNotificationConstructors(input_api, output_api):
  files_to_skip = (
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
      'ChromeNotificationWrapperBuilder.java',
      'chrome/android/java/src/org/chromium/chrome/browser/notifications/'
      'ChromeNotificationWrapperCompatBuilder.java'
  )
  error_msg = '''
  Android Notification Construction Check failed:
  Your new code added one or more calls to the Notification.Builder and/or
  NotificationCompat.Builder constructors, listed below.

  This is banned, please construct notifications using
  NotificationWrapperBuilderFactory.createNotificationWrapperBuilder instead,
  specifying a channel for use on Android O.

  See https://crbug.com/678670 for more information.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, files_to_skip,
                               NEW_NOTIFICATION_BUILDER_RE)


def _CheckAlertDialogBuilder(input_api, output_api):
  # In general, preference and FRE related UIs are not relevant to VR mode.
  files_to_skip = (
      BROWSER_ROOT + 'autofill/prefeditor/EditorDialog.java',
      BROWSER_ROOT + 'browserservices/ClearDataDialogActivity.java',
      BROWSER_ROOT + 'browsing_data/ConfirmImportantSitesDialogFragment.java',
      BROWSER_ROOT + 'browsing_data/OtherFormsOfHistoryDialogFragment.java',
      BROWSER_ROOT + 'dom_distiller/DistilledPagePrefsView.java',
      BROWSER_ROOT + 'dom_distiller/DomDistillerUIUtils.java',
      BROWSER_ROOT + 'download/OMADownloadHandler.java',
      BROWSER_ROOT + 'init/LaunchFailedActivity.java',
      BROWSER_ROOT + 'password_manager/AccountChooserDialog.java',
      BROWSER_ROOT + 'password_manager/AutoSigninFirstRunDialog.java',
      # TODO(crbug.com/40945893): Tentatively suppressed.
      (BROWSER_ROOT +
       'password_manager/settings/ExportErrorDialogFragment.java'),
      (BROWSER_ROOT +
       'password_manager/settings/ExportWarningDialogFragment.java'),
      (BROWSER_ROOT +
       'password_manager/settings/ProgressBarDialogFragment.java'),
      # end of https://crbug.com/1505284
      BROWSER_ROOT + r'settings[\\\/].*',
      BROWSER_ROOT + 'site_settings/AddExceptionPreference.java',
      BROWSER_ROOT + 'site_settings/ChosenObjectSettings.java',
      BROWSER_ROOT + 'site_settings/ManageSpaceActivity.java',
      BROWSER_ROOT + 'site_settings/ManageSpaceActivity.java',
      BROWSER_ROOT + 'site_settings/SingleCategorySettings.java',
      BROWSER_ROOT + 'site_settings/SingleWebsiteSettings.java',
      BROWSER_ROOT + 'sync/settings/ManageSyncSettings.java',
      BROWSER_ROOT + 'sync/settings/SyncAndServicesSettings.java',
      BROWSER_ROOT + 'sync/ui/PassphraseCreationDialogFragment.java',
      BROWSER_ROOT + 'sync/ui/PassphraseDialogFragment.java',
      BROWSER_ROOT + 'sync/ui/PassphraseTypeDialogFragment.java',
      BROWSER_ROOT + 'webapps/WebApkOfflineDialog.java',
  )
  error_msg = '''
  AlertDialog.Builder Check failed:
  Your new code added one or more calls to the AlertDialog.Builder, listed
  below.

  We recommend you use ModalDialogProperties to show a dialog whenever possible
  to support VR mode. You could only keep the AlertDialog if you are certain
  that your new AlertDialog is not used in VR mode (e.g. pereference, FRE)

  If you are in doubt, contact
  //src/chrome/android/java/src/org/chromium/chrome/browser/vr/VR_JAVA_OWNERS
  '''
  error_files = []
  result = _CheckReIgnoreComment(input_api, output_api, error_msg,
                                 files_to_skip, NEW_ALERTDIALOG_BUILDER_RE,
                                 error_files)

  wrong_builder_errors = []
  wrong_builder_error_msg = '''
  Android Use of AppCompat AlertDialog.Builder Check failed:
  Your new code added one or more calls to the AppCompat AlertDialog.Builder,
  file listed below.

  If you are keeping the new AppCompat AlertDialog.Builder, please use
  CompatibleAlertDialogBuilder instead to work around support library issues.

  See https://crbug.com/966101 for more information.
  '''
  for f in error_files:
    contents = input_api.ReadFile(f)
    if IMPORT_APP_COMPAT_ALERTDIALOG_RE.search(contents):
      wrong_builder_errors.append('  %s' % (f.LocalPath()))
  if wrong_builder_errors:
    result.extend([output_api.PresubmitError(
        wrong_builder_error_msg, wrong_builder_errors)])
  return result


def _CheckCompatibleAlertDialogBuilder(input_api, output_api):
  files_to_skip = (
      BROWSER_ROOT + 'autofill/keyboard_accessory/'
                     'AutofillKeyboardAccessoryBridge.java',
      BROWSER_ROOT + 'dom_distiller/DistilledPagePrefsView.java',
      BROWSER_ROOT + 'dom_distiller/DomDistillerUIUtils.java',
      BROWSER_ROOT + 'download/DownloadController.java',
      BROWSER_ROOT + 'download/OMADownloadHandler.java',
      BROWSER_ROOT + 'externalnav/ExternalNavigationDelegateImpl.java',
      BROWSER_ROOT + 'payments/AndroidPaymentApp.java',
      BROWSER_ROOT + 'permissions/AndroidPermissionRequester.java',
      BROWSER_ROOT + 'share/ShareDelegateImpl.java',
      BROWSER_ROOT + 'util/AccessibilityUtil.java',
      BROWSER_ROOT + 'webapps/AddToHomescreenDialog.java',
      BROWSER_ROOT + 'webapps/WebappOfflineDialog.java',
  )
  error_msg = '''
  Android Use of CompatibleAlertDialogBuilder Check failed:
  Your new code added one or more calls to the CompatibleAlertDialogBuilder
  constructors, listed below.

  We recommend you use ModalDialogProperties to show a dialog whenever possible
  to support VR mode. You could only keep the AlertDialog if you are certain
  that your new AlertDialog is not used in VR mode (e.g. pereference, FRE)

  If you are in doubt, contact
  //src/chrome/android/java/src/org/chromium/chrome/browser/vr/VR_JAVA_OWNERS
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, files_to_skip,
                               NEW_COMPATIBLE_ALERTDIALOG_BUILDER_RE)


def _CheckBundleUtilsIdentifierName(input_api, output_api):
  error_msg = '''
  BundleUtils.getIdentifierName() not check failed:
  BundleUtils.getIdentifierName() must be called with a String literal,
  otherwise R8 may not correctly obfuscate the class name passed in.
  '''
  return _CheckReIgnoreComment(input_api, output_api, error_msg, [],
                               SPLIT_COMPAT_UTILS_IMPL_NAME_RE)


def _CheckReIgnoreComment(input_api, output_api, error_msg, files_to_skip,
                          regular_expression, error_files=None):

  def CheckLine(current_file, line_number, line, problems, error_files):
    """Returns a boolean whether the line contains an error."""
    if (regular_expression.search(line) and not COMMENT_RE.search(line)):
      if error_files is not None:
        error_files.append(current_file)
      problems.append(
          '  %s:%d\n    \t%s' %
          (current_file.LocalPath(), line_number, line.strip()))
      return True
    return False

  problems = []
  sources = lambda x: input_api.FilterSourceFile(
      x, files_to_check=(r'.*\.java$',), files_to_skip=files_to_skip)
  for f in input_api.AffectedFiles(include_deletes=False,
                                   file_filter=sources):
    previous_line = ''
    for line_number, line in enumerate(f.NewContents(), start=1):
      if not CheckLine(f, line_number, line, problems, error_files):
        if previous_line:
          two_lines = '\n'.join([previous_line, line])
          CheckLine(f, line_number, two_lines, problems, error_files)
        previous_line = line
      else:
        previous_line = ''
  if problems:
    return [output_api.PresubmitError(error_msg, problems)]
  return []
