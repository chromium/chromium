# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Functions used to provision Fuchsia boot images."""

import common
import logging
import os
import subprocess
import tempfile
import time
import uuid

_SSH_CONFIG_TEMPLATE = """
Host *
  CheckHostIP no
  StrictHostKeyChecking no
  ForwardAgent no
  ForwardX11 no
  User fuchsia
  IdentitiesOnly yes
  IdentityFile {identity}
  ServerAliveInterval 2
  ServerAliveCountMax 5
  ControlMaster auto
  ControlPersist 1m
  ControlPath /tmp/ssh-%r@%h:%p
  ConnectTimeout 5
  """

# Specifies boot files intended for use by an emulator.
TARGET_TYPE_QEMU = 'qemu'

# Specifies boot files intended for use by anything (incl. physical devices).
TARGET_TYPE_GENERIC = 'generic'

# Defaults used by Fuchsia SDK
_SSH_DIR = os.path.expanduser('~/.ssh')
_SSH_CONFIG_DIR = os.path.expanduser('~/.fuchsia')


def _GetAuthorizedKeysPath():
  """Returns a path to the authorized keys which get copied to your Fuchsia
  device during paving"""

  return os.path.join(_SSH_DIR, 'fuchsia_authorized_keys')


def ProvisionSSH():
  """Generates a key pair and config file for SSH using the GN SDK."""

  returncode, out, err = common.RunGnSdkFunction('fuchsia-common.sh',
                                                 'check-fuchsia-ssh-config')
  if returncode != 0:
    logging.error('Command exited with error code %d' % (returncode))
    logging.error('Stdout: %s' % out)
    logging.error('Stderr: %s' % err)
    raise Exception('Failed to provision ssh keys')


def GetTargetFile(filename, image_path):
  """Computes a path to |filename| in the Fuchsia boot image directory specific
  to |image_path|."""

  return os.path.join(common.IMAGES_ROOT, image_path, filename)


def GetSSHConfigPath():
  return os.path.join(_SSH_CONFIG_DIR, 'sshconfig')


def GetBootImage(output_dir, image_path, image_name):
  """"Gets a path to the Zircon boot image, with the SSH client public key
  added."""
  ProvisionSSH()
  authkeys_path = _GetAuthorizedKeysPath()
  zbi_tool = common.GetHostToolPathFromPlatform('zbi')
  image_source_path = GetTargetFile(image_name, image_path)
  image_dest_path = os.path.join(output_dir, 'gen', 'fuchsia-with-keys.zbi')

  cmd = [
      zbi_tool, '-o', image_dest_path, image_source_path, '-e',
      'data/ssh/authorized_keys=' + authkeys_path
  ]
  subprocess.check_call(cmd)

  return image_dest_path


def GetKernelArgs():
  """Returns a list of Zircon commandline arguments to use when booting a
  system."""
  return [
      'devmgr.epoch=%d' % time.time(),
      'blobfs.write-compression-algorithm=UNCOMPRESSED'
  ]


def AssertBootImagesExist(image_path):
  assert os.path.exists(GetTargetFile('fuchsia.zbi', image_path)), \
      'This checkout is missing the files necessary for\n' \
      'booting this configuration of Fuchsia.\n' \
      'To check out the files, add this entry to the "custom_vars"\n' \
      'section of your .gclient file:\n\n' \
      '    "checkout_fuchsia_boot_images": "%s"\n\n' % \
           image_path
