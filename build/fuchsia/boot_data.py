# Copyright 2018 The Chromium Authors. All rights reserved.
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
  UserKnownHostsFile {known_hosts}
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

FVM_TYPE_QCOW = 'qcow'
FVM_TYPE_SPARSE = 'sparse'

# Specifies boot files intended for use by an emulator.
TARGET_TYPE_QEMU = 'qemu'

# Specifies boot files intended for use by anything (incl. physical devices).
TARGET_TYPE_GENERIC = 'generic'

def _GetPubKeyPath(output_dir):
  """Returns a path to the generated SSH public key."""

  return os.path.join(output_dir, 'id_ed25519.pub')


def ProvisionSSH(output_dir):
  """Generates a keypair and config file for SSH."""

  host_key_path = os.path.join(output_dir, 'ssh_key')
  host_pubkey_path = host_key_path + '.pub'
  id_key_path = os.path.join(output_dir, 'id_ed25519')
  id_pubkey_path = _GetPubKeyPath(output_dir)
  known_hosts_path = os.path.join(output_dir, 'known_hosts')
  ssh_config_path = os.path.join(output_dir, 'ssh_config')

  logging.debug('Generating SSH credentials.')
  if not os.path.isfile(host_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-h', '-f',
                           host_key_path, '-P', '', '-N', ''],
                          stdout=open(os.devnull))
  if not os.path.isfile(id_key_path):
    subprocess.check_call(['ssh-keygen', '-t', 'ed25519', '-f', id_key_path,
                           '-P', '', '-N', ''], stdout=open(os.devnull))

  with open(ssh_config_path, "w") as ssh_config:
    ssh_config.write(
        _SSH_CONFIG_TEMPLATE.format(identity=id_key_path,
                                    known_hosts=known_hosts_path))

  if os.path.exists(known_hosts_path):
    os.remove(known_hosts_path)


def GetTargetFile(filename, target_arch, target_type):
  """Computes a path to |filename| in the Fuchsia boot image directory specific
  to |target_type| and |target_arch|."""

  assert target_type == TARGET_TYPE_QEMU or target_type == TARGET_TYPE_GENERIC

  return os.path.join(common.IMAGES_ROOT, target_arch, target_type, filename)


def GetSSHConfigPath(output_dir):
  return output_dir + '/ssh_config'


def GetBootImage(output_dir, target_arch, target_type):
  """"Gets a path to the Zircon boot image, with the SSH client public key
  added."""

  ProvisionSSH(output_dir)
  pubkey_path = _GetPubKeyPath(output_dir)
  zbi_tool = common.GetHostToolPathFromPlatform('zbi')
  image_source_path = GetTargetFile('zircon-a.zbi', target_arch, target_type)
  image_dest_path = os.path.join(output_dir, 'gen', 'fuchsia-with-keys.zbi')

  cmd = [ zbi_tool, '-o', image_dest_path, image_source_path,
          '-e', 'data/ssh/authorized_keys=' + pubkey_path ]
  subprocess.check_call(cmd)

  return image_dest_path


def GetKernelArgs(output_dir):
  return ['devmgr.epoch=%d' % time.time()]


def AssertBootImagesExist(arch, platform):
  assert os.path.exists(GetTargetFile('zircon-a.zbi', arch, platform)), \
      'This checkout is missing the files necessary for\n' \
      'booting this configuration of Fuchsia.\n' \
      'To check out the files, add this entry to the "custom_vars"\n' \
      'section of your .gclient file:\n\n' \
      '    "checkout_fuchsia_boot_images": "%s.%s"\n\n' % \
           (platform, arch)
