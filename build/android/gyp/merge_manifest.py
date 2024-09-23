#!/usr/bin/env python3

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges dependency Android manifests into a root manifest."""

import argparse
import collections
import contextlib
import os
import sys
import tempfile
import xml.etree.ElementTree as ElementTree

from util import build_utils
from util import manifest_utils
import action_helpers  # build_utils adds //build to sys.path.

_MANIFEST_MERGER_MAIN_CLASS = 'com.android.manifmerger.Merger'


@contextlib.contextmanager
def _ProcessMainManifest(manifest_path, min_sdk_version, target_sdk_version,
                         max_sdk_version, manifest_package):
  """Patches the main Android manifest"""
  doc, manifest, _ = manifest_utils.ParseManifest(manifest_path)
  manifest_utils.SetUsesSdk(manifest, target_sdk_version, min_sdk_version,
                            max_sdk_version)
  assert manifest_utils.GetPackage(manifest) or manifest_package, \
            'Must set manifest package in GN or in AndroidManifest.xml'
  if manifest_package:
    manifest.set('package', manifest_package)
  tmp_prefix = manifest_path.replace(os.path.sep, '-')
  with tempfile.NamedTemporaryFile(prefix=tmp_prefix) as patched_manifest:
    manifest_utils.SaveManifest(doc, patched_manifest.name)
    yield patched_manifest.name, manifest_utils.GetPackage(manifest)


@contextlib.contextmanager
def _ProcessOtherManifest(manifest_path, min_sdk_version, target_sdk_version,
                          seen_package_names):
  """Patches non-main AndroidManifest.xml if necessary."""
  # 1. Ensure targetSdkVersion is set to the expected value to avoid
  #    spurious permissions being added (b/222331337).
  # 2. Ensure all manifests have a unique package name so that the merger
  #    does not fail when this happens.
  doc, manifest, _ = manifest_utils.ParseManifest(manifest_path)

  changed_api = manifest_utils.SetTargetApiIfUnset(manifest, target_sdk_version)

  package_name = manifest_utils.GetPackage(manifest)
  # Ignore minSdkVersion from androidx.pdf library. The client code will ensure
  # not to call into the library API on older Android versions.
  if package_name in ('androidx.pdf', 'androidx.pdf.viewer.fragment'):
    manifest_utils.OverrideMinSdkVersionIfPresent(manifest, min_sdk_version)
    changed_api = True
  package_count = seen_package_names[package_name]
  seen_package_names[package_name] += 1
  if package_count > 0:
    manifest.set('package', f'{package_name}_{package_count}')

  if package_count > 0 or changed_api:
    tmp_prefix = manifest_path.replace(os.path.sep, '-')
    with tempfile.NamedTemporaryFile(prefix=tmp_prefix) as patched_manifest:
      manifest_utils.SaveManifest(doc, patched_manifest.name)
      yield patched_manifest.name
  else:
    yield manifest_path


def main(argv):
  argv = build_utils.ExpandFileArgs(argv)
  parser = argparse.ArgumentParser(description=__doc__)
  action_helpers.add_depfile_arg(parser)
  parser.add_argument('--manifest-merger-jar',
                      help='Path to SDK\'s manifest merger jar.',
                      required=True)
  parser.add_argument('--root-manifest',
                      help='Root manifest which to merge into',
                      required=True)
  parser.add_argument('--output', help='Output manifest path', required=True)
  parser.add_argument('--extras',
                      help='GN list of additional manifest to merge')
  parser.add_argument(
      '--min-sdk-version',
      required=True,
      help='android:minSdkVersion for merging.')
  parser.add_argument(
      '--target-sdk-version',
      required=True,
      help='android:targetSdkVersion for merging.')
  parser.add_argument(
      '--max-sdk-version', help='android:maxSdkVersion for merging.')
  parser.add_argument(
      '--manifest-package',
      help='Package name of the merged AndroidManifest.xml.')
  parser.add_argument('--warnings-as-errors',
                      action='store_true',
                      help='Treat all warnings as errors.')
  args = parser.parse_args(argv)

  with action_helpers.atomic_output(args.output) as output:
    cmd = build_utils.JavaCmd() + [
        '-cp',
        args.manifest_merger_jar,
        _MANIFEST_MERGER_MAIN_CLASS,
        '--out',
        output.name,
        '--property',
        'MIN_SDK_VERSION=' + args.min_sdk_version,
        '--property',
        'TARGET_SDK_VERSION=' + args.target_sdk_version,
    ]

    if args.max_sdk_version:
      cmd += [
          '--property',
          'MAX_SDK_VERSION=' + args.max_sdk_version,
      ]

    extras = action_helpers.parse_gn_list(args.extras)

    with contextlib.ExitStack() as stack:
      root_manifest, package = stack.enter_context(
          _ProcessMainManifest(args.root_manifest, args.min_sdk_version,
                               args.target_sdk_version, args.max_sdk_version,
                               args.manifest_package))
      if extras:
        seen_package_names = collections.Counter()
        extras_processed = [
            stack.enter_context(
                _ProcessOtherManifest(e, args.min_sdk_version,
                                      args.target_sdk_version,
                                      seen_package_names)) for e in extras
        ]
        cmd += ['--libs', ':'.join(extras_processed)]
      cmd += [
          '--main',
          root_manifest,
          '--property',
          'PACKAGE=' + package,
          '--remove-tools-declarations',
      ]
      build_utils.CheckOutput(
          cmd,
          # https://issuetracker.google.com/issues/63514300:
          # The merger doesn't set a nonzero exit code for failures.
          fail_func=lambda returncode, stderr: returncode != 0 or build_utils.
          IsTimeStale(output.name, [root_manifest] + extras),
          fail_on_output=args.warnings_as_errors)

  if args.depfile:
    action_helpers.write_depfile(args.depfile, args.output, inputs=extras)


if __name__ == '__main__':
  main(sys.argv[1:])
