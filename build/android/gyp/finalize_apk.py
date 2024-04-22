# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Signs and aligns an APK."""

import argparse
import logging
import shutil
import subprocess
import sys
import tempfile

from util import build_utils


def FinalizeApk(apksigner_path,
                zipalign_path,
                unsigned_apk_path,
                final_apk_path,
                key_path,
                key_passwd,
                key_name,
                min_sdk_version,
                warnings_as_errors=False):
  # Use a tempfile so that Ctrl-C does not leave the file with a fresh mtime
  # and a corrupted state.
  with tempfile.NamedTemporaryFile() as staging_file:
    if zipalign_path:
      # v2 signing requires that zipalign happen first.
      logging.debug('Running zipalign')
      zipalign_cmd = [
          zipalign_path, '-p', '-f', '4', unsigned_apk_path, staging_file.name
      ]
      build_utils.CheckOutput(zipalign_cmd,
                              print_stdout=True,
                              fail_on_output=warnings_as_errors)
      signer_input_path = staging_file.name
    else:
      signer_input_path = unsigned_apk_path

    sign_cmd = build_utils.JavaCmd() + [
        '-jar',
        apksigner_path,
        'sign',
        '--in',
        signer_input_path,
        '--out',
        staging_file.name,
        '--ks',
        key_path,
        '--ks-key-alias',
        key_name,
        '--ks-pass',
        'pass:' + key_passwd,
    ]
    # V3 signing adds security niceties, which are irrelevant for local builds.
    sign_cmd += ['--v3-signing-enabled', 'false']

    if min_sdk_version >= 24:
      # Disable v1 signatures when v2 signing can be used (it's much faster).
      # By default, both v1 and v2 signing happen.
      sign_cmd += ['--v1-signing-enabled', 'false']
    else:
      # Force SHA-1 (makes signing faster; insecure is fine for local builds).
      # Leave v2 signing enabled since it verifies faster on device when
      # supported.
      sign_cmd += ['--min-sdk-version', '1']

    logging.debug('Signing apk')
    build_utils.CheckOutput(sign_cmd,
                            print_stdout=True,
                            fail_on_output=warnings_as_errors)
    shutil.move(staging_file.name, final_apk_path)
    # TODO(crbug.com/40167754): Remove this once Python2 is obsoleted.
    if sys.version_info.major == 2:
      staging_file.delete = False
    else:
      staging_file._closer.delete = False
