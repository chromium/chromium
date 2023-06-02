# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import sys


def IsNewer(old_version, new_version):
  return (old_version and new_version and
          (old_version.major < new_version.major or
           (old_version.major == new_version.major and
            old_version.minor < new_version.minor)))


def CheckVersionAndAssetParity(input_api, output_api):
  """Checks that
  - the version was upraded if assets files were changed,
  - the version was not downgraded,
  - both the google_chrome and the chromium assets have the same files.
  """
  sys.path.append(input_api.PresubmitLocalPath())
  import parse_version

  old_version = None
  new_version = None
  changed_assets = False
  changed_version = False
  changed_component_list = False
  changed_asset_files = {'google_chrome': [], 'chromium': []}
  for file in input_api.AffectedFiles():
    basename = input_api.os_path.basename(file.LocalPath())
    extension = input_api.os_path.splitext(basename)[1][1:].strip().lower()
    basename_without_extension = input_api.os_path.splitext(basename)[
        0].strip().lower()
    if extension == 'sha1':
      basename_without_extension = input_api.os_path.splitext(
          basename_without_extension)[0]
    dirname = input_api.os_path.basename(
        input_api.os_path.dirname(file.LocalPath()))
    action = file.Action()
    if (dirname in changed_asset_files and
        extension in {'sha1', 'png', 'wav'} and action in {'A', 'D'}):
      changed_asset_files[dirname].append((action, basename_without_extension))
    if (extension == 'sha1' or basename == 'vr_assets_component_files.json'):
      # See if there are actually changes or if it's just --files or --all:
      if file.ChangedContents():
        changed_assets = True
    if basename == 'vr_assets_component_files.json':
      changed_component_list = True
    if basename == 'VERSION':
      old_version = parse_version.ParseVersion(file.OldContents())
      new_version = parse_version.ParseVersion(file.NewContents())
      if new_version != old_version:
        changed_version = True

  local_version_filename = input_api.os_path.join(
      input_api.os_path.dirname(input_api.AffectedFiles()[0].LocalPath()),
      'VERSION')
  local_component_list_filename = input_api.os_path.join(
      input_api.os_path.dirname(input_api.AffectedFiles()[0].LocalPath()),
      'vr_assets_component_files.json')

  if changed_asset_files['google_chrome'] != changed_asset_files['chromium']:
    return [
        output_api.PresubmitError(
            'Must have same asset files for %s in \'%s\'.' %
            (changed_asset_files.keys(),
             input_api.os_path.dirname(
                 input_api.AffectedFiles()[0].LocalPath())))
    ]

  if changed_asset_files['google_chrome'] and not changed_component_list:
    return [
        output_api.PresubmitError(
            'Must update \'%s\' if adding/removing assets.' %
            local_component_list_filename)
    ]

  if changed_version and (not old_version or not new_version):
    return [
        output_api.PresubmitError(
            'Cannot parse version in \'%s\'.' % local_version_filename)
    ]

  version_upgraded = IsNewer(old_version, new_version)
  if changed_assets and not version_upgraded:
    return [
        output_api.PresubmitError(
            'Must increment version in \'%s\' when '
            'updating VR assets.' % local_version_filename)
    ]
  if changed_version and not version_upgraded:
    return [
        output_api.PresubmitError(
            'Must not downgrade version in \'%s\'.' % local_version_filename)
    ]

  return []


def CheckChangeOnUpload(input_api, output_api):
  return CheckVersionAndAssetParity(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckVersionAndAssetParity(input_api, output_api)
