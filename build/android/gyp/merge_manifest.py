#!/usr/bin/env python3

# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Merges dependency Android manifests into a root manifest."""

import argparse
import contextlib
import os
import sys
import tempfile
import xml.etree.ElementTree as ElementTree

from util import build_utils
from util import manifest_utils

_MANIFEST_MERGER_MAIN_CLASS = 'com.android.manifmerger.Merger'


@contextlib.contextmanager
def _ProcessManifest(manifest_path, min_sdk_version, target_sdk_version,
                     max_sdk_version, manifest_package):
  """Patches an Android manifest's package and performs assertions to ensure
  correctness for the manifest.
  """
  doc, manifest, _ = manifest_utils.ParseManifest(manifest_path)
  manifest_utils.AssertUsesSdk(manifest, min_sdk_version, target_sdk_version,
                               max_sdk_version)
  assert manifest_utils.GetPackage(manifest) or manifest_package, \
            'Must set manifest package in GN or in AndroidManifest.xml'
  manifest_utils.AssertPackage(manifest, manifest_package)
  if manifest_package:
    manifest.set('package', manifest_package)
  tmp_prefix = manifest_path.replace(os.path.sep, '-')
  with tempfile.NamedTemporaryFile(prefix=tmp_prefix) as patched_manifest:
    manifest_utils.SaveManifest(doc, patched_manifest.name)
    yield patched_manifest.name, manifest_utils.GetPackage(manifest)


@contextlib.contextmanager
def _SetTargetApi(manifest_path, target_sdk_version):
  """Patches an Android manifest's TargetApi if not set.

  We do this to avoid the manifest merger assuming we have a targetSdkVersion
  of 1 and inserting unnecessary permission requests into our merged manifests.
  See b/222331337 for more details.
  """
  doc, manifest, _ = manifest_utils.ParseManifest(manifest_path)
  manifest_utils.SetTargetApiIfUnset(manifest, target_sdk_version)
  tmp_prefix = manifest_path.replace(os.path.sep, '-')
  with tempfile.NamedTemporaryFile(prefix=tmp_prefix) as patched_manifest:
    manifest_utils.SaveManifest(doc, patched_manifest.name)
    yield patched_manifest.name


def main(argv):
  argv = build_utils.ExpandFileArgs(argv)
  parser = argparse.ArgumentParser(description=__doc__)
  build_utils.AddDepfileOption(parser)
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

  with build_utils.AtomicOutput(args.output) as output:
    cmd = build_utils.JavaCmd(args.warnings_as_errors) + [
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

    extras = build_utils.ParseGnList(args.extras)

    with contextlib.ExitStack() as stack:
      root_manifest, package = stack.enter_context(
          _ProcessManifest(args.root_manifest, args.min_sdk_version,
                           args.target_sdk_version, args.max_sdk_version,
                           args.manifest_package))
      if extras:
        extras_processed = [
            stack.enter_context(_SetTargetApi(e, args.target_sdk_version))
            for e in extras
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

    # Check for correct output.
    _, manifest, _ = manifest_utils.ParseManifest(output.name)
    manifest_utils.AssertUsesSdk(manifest, args.min_sdk_version,
                                 args.target_sdk_version)
    manifest_utils.AssertPackage(manifest, package)

  if args.depfile:
    build_utils.WriteDepfile(args.depfile, args.output, inputs=extras)


if __name__ == '__main__':
  main(sys.argv[1:])
