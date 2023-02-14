# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Presubmit helpers for ios

See https://www.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

from . import update_bundle_filelist


def CheckBundleData(input_api, output_api, base, globroot='//'):
  root = input_api.change.RepositoryRoot()
  filelist = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                    base + '.filelist')
  globlist = input_api.os_path.join(input_api.PresubmitLocalPath(),
                                    base + '.globlist')
  if globroot.startswith('//'):
    globroot = input_api.os_path.join(input_api.change.RepositoryRoot(),
                                      globroot[2:])
  else:
    globroot = input_api.os_path.join(input_api.PresubmitLocalPath(), globroot)
  if update_bundle_filelist.process_filelist(filelist,
                                             globlist,
                                             globroot,
                                             check=True,
                                             verbose=input_api.verbose) == 0:
    return []
  else:
    script = input_api.os_path.join(input_api.change.RepositoryRoot(), 'build',
                                    'ios', 'update_bundle_filelist.py')

    return [
        output_api.PresubmitError(
            'Filelist needs to be re-generated. Please run \'python3 %s %s %s '
            '%s\' and include the changes in this CL' %
            (script, filelist, globlist, globroot))
    ]
