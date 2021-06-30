#!/usr/bin/env vpython3
#
# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Install *_incremental.apk targets as well as their dependent files."""

import argparse
import collections
import functools
import glob
import json
import logging
import os
import posixpath
import shutil
import sys

sys.path.append(
    os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir)))
import devil_chromium
from devil.android import apk_helper
from devil.android import device_utils
from devil.utils import reraiser_thread
from devil.utils import run_tests_helper
from pylib import constants
from pylib.utils import time_profile

prev_sys_path = list(sys.path)
sys.path.insert(0, os.path.join(os.path.dirname(__file__), os.pardir, 'gyp'))
import dex
from util import build_utils
sys.path = prev_sys_path


_R8_PATH = os.path.join(build_utils.DIR_SOURCE_ROOT, 'third_party', 'r8', 'lib',
                        'r8.jar')


def _DeviceCachePath(device):
  file_name = 'device_cache_%s.json' % device.adb.GetDeviceSerial()
  return os.path.join(constants.GetOutDirectory(), file_name)


def _Execute(concurrently, *funcs):
  """Calls all functions in |funcs| concurrently or in sequence."""
  timer = time_profile.TimeProfile()
  if concurrently:
    reraiser_thread.RunAsync(funcs)
  else:
    for f in funcs:
      f()
  timer.Stop(log=False)
  return timer


def _GetDeviceIncrementalDir(package):
  """Returns the device path to put incremental files for the given package."""
  return '/data/local/tmp/incremental-app-%s' % package


def _IsStale(src_paths, dest):
  """Returns if |dest| is older than any of |src_paths|, or missing."""
  if not os.path.exists(dest):
    return True
  dest_time = os.path.getmtime(dest)
  for path in src_paths:
    if os.path.getmtime(path) > dest_time:
      return True
  return False


def _AllocateDexShards(dex_files):
  """Divides input dex files into buckets."""
  # Goals:
  # * Make shards small enough that they are fast to merge.
  # * Minimize the number of shards so they load quickly on device.
  # * Partition files into shards such that a change in one file results in only
  #   one shard having to be re-created.
  shards = collections.defaultdict(list)
  # As of Oct 2019, 10 shards results in a min/max size of 582K/2.6M.
  NUM_CORE_SHARDS = 10
  # As of Oct 2019, 17 dex files are larger than 1M.
  SHARD_THRESHOLD = 2**20
  for src_path in dex_files:
    if os.path.getsize(src_path) >= SHARD_THRESHOLD:
      # Use the path as the name rather than an incrementing number to ensure
      # that it shards to the same name every time.
      name = os.path.relpath(src_path, constants.GetOutDirectory()).replace(
          os.sep, '.')
      shards[name].append(src_path)
    else:
      name = 'shard{}.dex.jar'.format(hash(src_path) % NUM_CORE_SHARDS)
      shards[name].append(src_path)
  logging.info('Sharding %d dex files into %d buckets', len(dex_files),
               len(shards))
  return shards


def _CreateDexFiles(shards, dex_staging_dir, min_api, use_concurrency):
  """Creates dex files within |dex_staging_dir| defined by |shards|."""
  tasks = []
  for name, src_paths in shards.items():
    dest_path = os.path.join(dex_staging_dir, name)
    if _IsStale(src_paths, dest_path):
      tasks.append(
          functools.partial(dex.MergeDexForIncrementalInstall, _R8_PATH,
                            src_paths, dest_path, min_api))

  # TODO(agrieve): It would be more performant to write a custom d8.jar
  #     wrapper in java that would process these in bulk, rather than spinning
  #     up a new process for each one.
  _Execute(use_concurrency, *tasks)

  # Remove any stale shards.
  for name in os.listdir(dex_staging_dir):
    if name not in shards:
      os.unlink(os.path.join(dex_staging_dir, name))


def Uninstall(device, package, enable_device_cache=False):
  """Uninstalls and removes all incremental files for the given package."""
  main_timer = time_profile.TimeProfile()
  device.Uninstall(package)
  if enable_device_cache:
    # Uninstall is rare, so just wipe the cache in this case.
    cache_path = _DeviceCachePath(device)
    if os.path.exists(cache_path):
      os.unlink(cache_path)
  device.RunShellCommand(['rm', '-rf', _GetDeviceIncrementalDir(package)],
                         check_return=True)
  logging.info('Uninstall took %s seconds.', main_timer.GetDelta())


def Install(device, install_json, apk=None, enable_device_cache=False,
            use_concurrency=True, permissions=()):
  """Installs the given incremental apk and all required supporting files.

  Args:
    device: A DeviceUtils instance (to install to).
    install_json: Path to .json file or already parsed .json object.
    apk: An existing ApkHelper instance for the apk (optional).
    enable_device_cache: Whether to enable on-device caching of checksums.
    use_concurrency: Whether to speed things up using multiple threads.
    permissions: A list of the permissions to grant, or None to grant all
                 non-denylisted permissions in the manifest.
  """
  if isinstance(install_json, str):
    with open(install_json) as f:
      install_dict = json.load(f)
  else:
    install_dict = install_json

  main_timer = time_profile.TimeProfile()
  install_timer = time_profile.TimeProfile()
  push_native_timer = time_profile.TimeProfile()
  merge_dex_timer = time_profile.TimeProfile()
  push_dex_timer = time_profile.TimeProfile()

  def fix_path(p):
    return os.path.normpath(os.path.join(constants.GetOutDirectory(), p))

  if not apk:
    apk = apk_helper.ToHelper(fix_path(install_dict['apk_path']))
  split_globs = [fix_path(p) for p in install_dict['split_globs']]
  native_libs = [fix_path(p) for p in install_dict['native_libs']]
  dex_files = [fix_path(p) for p in install_dict['dex_files']]
  show_proguard_warning = install_dict.get('show_proguard_warning')

  apk_package = apk.GetPackageName()
  device_incremental_dir = _GetDeviceIncrementalDir(apk_package)
  dex_staging_dir = os.path.join(constants.GetOutDirectory(),
                                 'incremental-install',
                                 install_dict['apk_path'])
  device_dex_dir = posixpath.join(device_incremental_dir, 'dex')

  # Install .apk(s) if any of them have changed.
  def do_install():
    install_timer.Start()
    if split_globs:
      splits = []
      for split_glob in split_globs:
        splits.extend((f for f in glob.glob(split_glob)))
      device.InstallSplitApk(
          apk,
          splits,
          allow_downgrade=True,
          reinstall=True,
          allow_cached_props=True,
          permissions=permissions)
    else:
      device.Install(
          apk, allow_downgrade=True, reinstall=True, permissions=permissions)
    install_timer.Stop(log=False)

  # Push .so and .dex files to the device (if they have changed).
  def do_push_files():

    def do_push_native():
      push_native_timer.Start()
      if native_libs:
        with build_utils.TempDir() as temp_dir:
          device_lib_dir = posixpath.join(device_incremental_dir, 'lib')
          for path in native_libs:
            # Note: Can't use symlinks as they don't work when
            # "adb push parent_dir" is used (like we do here).
            shutil.copy(path, os.path.join(temp_dir, os.path.basename(path)))
          device.PushChangedFiles([(temp_dir, device_lib_dir)],
                                  delete_device_stale=True)
      push_native_timer.Stop(log=False)

    def do_merge_dex():
      merge_dex_timer.Start()
      shards = _AllocateDexShards(dex_files)
      build_utils.MakeDirectory(dex_staging_dir)
      _CreateDexFiles(shards, dex_staging_dir, apk.GetMinSdkVersion(),
                      use_concurrency)
      merge_dex_timer.Stop(log=False)

    def do_push_dex():
      push_dex_timer.Start()
      device.PushChangedFiles([(dex_staging_dir, device_dex_dir)],
                              delete_device_stale=True)
      push_dex_timer.Stop(log=False)

    _Execute(use_concurrency, do_push_native, do_merge_dex)
    do_push_dex()

  def check_device_configured():
    target_sdk_version = int(apk.GetTargetSdkVersion())
    # Beta Q builds apply allowlist to targetSdk=28 as well.
    if target_sdk_version >= 28 and device.build_version_sdk >= 28:
      # In P, there are two settings:
      #  * hidden_api_policy_p_apps
      #  * hidden_api_policy_pre_p_apps
      # In Q, there is just one:
      #  * hidden_api_policy
      if device.build_version_sdk == 28:
        setting_name = 'hidden_api_policy_p_apps'
      else:
        setting_name = 'hidden_api_policy'
      apis_allowed = ''.join(
          device.RunShellCommand(['settings', 'get', 'global', setting_name],
                                 check_return=True))
      if apis_allowed.strip() not in '01':
        msg = """\
Cannot use incremental installs on Android P+ without first enabling access to
non-SDK interfaces (https://developer.android.com/preview/non-sdk-q).

To enable access:
   adb -s {0} shell settings put global {1} 0
To restore back to default:
   adb -s {0} shell settings delete global {1}"""
        raise Exception(msg.format(device.serial, setting_name))

  cache_path = _DeviceCachePath(device)
  def restore_cache():
    if not enable_device_cache:
      return
    if os.path.exists(cache_path):
      logging.info('Using device cache: %s', cache_path)
      with open(cache_path) as f:
        device.LoadCacheData(f.read())
      # Delete the cached file so that any exceptions cause it to be cleared.
      os.unlink(cache_path)
    else:
      logging.info('No device cache present: %s', cache_path)

  def save_cache():
    if not enable_device_cache:
      return
    with open(cache_path, 'w') as f:
      f.write(device.DumpCacheData())
      logging.info('Wrote device cache: %s', cache_path)

  # Create 2 lock files:
  # * install.lock tells the app to pause on start-up (until we release it).
  # * firstrun.lock is used by the app to pause all secondary processes until
  #   the primary process finishes loading the .dex / .so files.
  def create_lock_files():
    # Creates or zeros out lock files.
    cmd = ('D="%s";'
           'mkdir -p $D &&'
           'echo -n >$D/install.lock 2>$D/firstrun.lock')
    device.RunShellCommand(
        cmd % device_incremental_dir, shell=True, check_return=True)

  # The firstrun.lock is released by the app itself.
  def release_installer_lock():
    device.RunShellCommand('echo > %s/install.lock' % device_incremental_dir,
                           check_return=True, shell=True)

  # Concurrency here speeds things up quite a bit, but DeviceUtils hasn't
  # been designed for multi-threading. Enabling only because this is a
  # developer-only tool.
  setup_timer = _Execute(use_concurrency, create_lock_files, restore_cache,
                         check_device_configured)

  _Execute(use_concurrency, do_install, do_push_files)

  finalize_timer = _Execute(use_concurrency, release_installer_lock, save_cache)

  logging.info(
      'Install of %s took %s seconds (setup=%s, install=%s, lib_push=%s, '
      'dex_merge=%s dex_push=%s, finalize=%s)', os.path.basename(apk.path),
      main_timer.GetDelta(), setup_timer.GetDelta(), install_timer.GetDelta(),
      push_native_timer.GetDelta(), merge_dex_timer.GetDelta(),
      push_dex_timer.GetDelta(), finalize_timer.GetDelta())
  if show_proguard_warning:
    logging.warning('Target had proguard enabled, but incremental install uses '
                    'non-proguarded .dex files. Performance characteristics '
                    'may differ.')


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('json_path',
                      help='The path to the generated incremental apk .json.')
  parser.add_argument('-d', '--device', dest='device',
                      help='Target device for apk to install on.')
  parser.add_argument('--uninstall',
                      action='store_true',
                      default=False,
                      help='Remove the app and all side-loaded files.')
  parser.add_argument('--output-directory',
                      help='Path to the root build directory.')
  parser.add_argument('--no-threading',
                      action='store_false',
                      default=True,
                      dest='threading',
                      help='Do not install and push concurrently')
  parser.add_argument('--no-cache',
                      action='store_false',
                      default=True,
                      dest='cache',
                      help='Do not use cached information about what files are '
                           'currently on the target device.')
  parser.add_argument('-v',
                      '--verbose',
                      dest='verbose_count',
                      default=0,
                      action='count',
                      help='Verbose level (multiple times for more)')

  args = parser.parse_args()

  run_tests_helper.SetLogLevel(args.verbose_count)
  if args.output_directory:
    constants.SetOutputDirectory(args.output_directory)

  devil_chromium.Initialize(output_directory=constants.GetOutDirectory())

  # Retries are annoying when commands fail for legitimate reasons. Might want
  # to enable them if this is ever used on bots though.
  device = device_utils.DeviceUtils.HealthyDevices(
      device_arg=args.device,
      default_retries=0,
      enable_device_files_cache=True)[0]

  if args.uninstall:
    with open(args.json_path) as f:
      install_dict = json.load(f)
    apk = apk_helper.ToHelper(install_dict['apk_path'])
    Uninstall(device, apk.GetPackageName(), enable_device_cache=args.cache)
  else:
    Install(device, args.json_path, enable_device_cache=args.cache,
            use_concurrency=args.threading)


if __name__ == '__main__':
  sys.exit(main())
