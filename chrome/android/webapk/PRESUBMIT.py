# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for changes affecting chrome/android/webapk/shell_apk:webapk

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.

This presubmit checks for three rules:
1. If anything in the webapk/libs/common or the webapk/shell_apk directories
has changed (excluding test files), $CURRENT_VERSION_VARIABLE should be updated.
2. If $REQUEST_UPDATE_FOR_VERSION_VARIABLE in
$REQUEST_UPDATE_FOR_VERSION_LOCAL_PATH is changed, the variable change should
be the only change in the CL.
3. If a file in a res/ directory has been added, its' file name should be
unique to the res/ directory.
  res/values/dimens.xml and res/values-v17/dimens.xml -> OK
  res/values/dimens.xml and libs/common/res_splash/values/dimens.xml -> BAD
This requirement is needed to upload the resources to the Google storage
build bucket.
"""


CURRENT_VERSION_VARIABLE = 'current_shell_apk_version'
CURRENT_VERSION_LOCAL_PATH = 'shell_apk/current_version/current_version.gni'

REQUEST_UPDATE_FOR_VERSION_VARIABLE = 'request_update_for_shell_apk_version'
REQUEST_UPDATE_FOR_VERSION_LOCAL_PATH = (
    'shell_apk/request_update_for_version.gni')

TRIGGER_CURRENT_VERSION_UPDATE_LOCAL_PATHS = [
    'libs/common/src/',
    'libs/common/res_splash/',
    'shell_apk/AndroidManifest.xml',
    'shell_apk/res/',
    'shell_apk/res_template',
    'shell_apk/src/',
]

RES_DIR_LOCAL_PATHS = [
    'shell_apk/res',
    'shell_apk/res_template',
    'libs/common/res_splash'
]

def _DoChangedContentsContain(changed_contents, key):
  for _, line in changed_contents:
    if key in line:
      return True
  return False


def _FindFileNamesInDirectory(input_api, dir_path, search_file_names):
  """
  Searches the directory recursively for files with the passed-in file name
  (not file path) set. Returns the file names of any matches.
  """
  matches = []
  for _, _, file_names in input_api.os_walk(dir_path):
    for file_name in file_names:
      if file_name in search_file_names:
        matches.append(file_name)
  return matches


def _CheckVersionVariableChanged(input_api, version_file_local_path,
                                 variable_name):
  for f in input_api.AffectedFiles():
    local_path = input_api.os_path.relpath(
        f.AbsoluteLocalPath(),
        input_api.PresubmitLocalPath()).replace('\\', '/')
    if local_path == version_file_local_path:
      return _DoChangedContentsContain(f.ChangedContents(), variable_name)

  return False


def _CheckChromeUpdateTriggerRule(input_api, output_api):
  """
  Check that if |request_update_for_shell_apk_version| is updated it is the
  only change in the CL.
  """
  if _CheckVersionVariableChanged(input_api,
                                  REQUEST_UPDATE_FOR_VERSION_LOCAL_PATH,
                                  REQUEST_UPDATE_FOR_VERSION_VARIABLE):
    if (len(input_api.AffectedFiles()) != 1 or
        len(input_api.AffectedFiles()[0].ChangedContents()) != 1):
      return [
        output_api.PresubmitError(
            '{} in {} must be updated in a standalone CL.'.format(
                REQUEST_UPDATE_FOR_VERSION_VARIABLE,
                REQUEST_UPDATE_FOR_VERSION_LOCAL_PATH))
        ]
  return []


def _CheckCurrentVersionIncreaseRule(input_api, output_api):
  """
  Check that if a file in $WAM_MINT_TRIGGER_LOCAL_PATHS is updated that
  |template_shell_apk_version| is updated as well.
  """
  files_requiring_version_increase = []
  for f in input_api.AffectedFiles():
    if f.ChangedContents():
      local_path = input_api.os_path.relpath(
          f.AbsoluteLocalPath(),
          input_api.PresubmitLocalPath()).replace('\\', '/')
      for trigger_local_path in TRIGGER_CURRENT_VERSION_UPDATE_LOCAL_PATHS:
        if local_path.startswith(trigger_local_path):
          files_requiring_version_increase.append(local_path)

  if not files_requiring_version_increase:
    return []

  if not _CheckVersionVariableChanged(input_api, CURRENT_VERSION_LOCAL_PATH,
                                      CURRENT_VERSION_VARIABLE):
    return [output_api.PresubmitError(
        '{} in {} needs to updated due to changes in:'.format(
            CURRENT_VERSION_VARIABLE, CURRENT_VERSION_LOCAL_PATH),
        items=files_requiring_version_increase)]

  return []


def _CheckNoOverlappingFileNamesInResourceDirsRule(input_api, output_api):
  """
  Checks that if a file has been added to a res/ directory that its file name
  is unique to the res/ directory.
    res/values/dimens.xml and res/values-v17/dimens.xml -> OK
    res/values/dimens.xml and libs/common/res_splash/values/dimens.xml -> BAD
  """
  res_dir_file_names_map = {}
  for f in input_api.AffectedFiles():
    local_path = input_api.os_path.relpath(
        f.AbsoluteLocalPath(),
        input_api.PresubmitLocalPath()).replace('\\', '/')
    for res_dir_local_path in RES_DIR_LOCAL_PATHS:
      if local_path.startswith(res_dir_local_path):
        file_name = input_api.os_path.basename(local_path)
        res_dir_file_names_map.setdefault(res_dir_local_path, set()).add(
            file_name)
        break

  if len(res_dir_file_names_map) == 0:
    return []

  overlapping_file_names = set()
  for res_dir, file_names in res_dir_file_names_map.items():
    for other_res_dir, other_file_names in res_dir_file_names_map.items():
      if res_dir == other_res_dir:
        continue

      # Check for affected files with identical name in |other_res_dir|.
      overlapping_file_names |= (file_names & other_file_names)

      # Check for existing files with identical name in |other_res_dir|.
      overlapping_file_names.update(
          _FindFileNamesInDirectory(input_api, other_res_dir, file_names))

  if len(overlapping_file_names) > 0:
    error_msg = ('Resources in different top level res/ directories {} should '
                 'have different names:').format(RES_DIR_LOCAL_PATHS)
    return [output_api.PresubmitError(error_msg,
                                      items=list(overlapping_file_names))]
  return []

def _CommonChecks(input_api, output_api):
  """Checks common to both upload and commit."""
  result = []
  result.extend(_CheckChromeUpdateTriggerRule(input_api, output_api))
  result.extend(_CheckCurrentVersionIncreaseRule(input_api, output_api))
  result.extend(_CheckNoOverlappingFileNamesInResourceDirsRule(input_api,
                                                               output_api))

  return result


def CheckChangeOnUpload(input_api, output_api):
  return _CommonChecks(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return _CommonChecks(input_api, output_api)
