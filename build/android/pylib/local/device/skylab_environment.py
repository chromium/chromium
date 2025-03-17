# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from pylib.local.device import local_device_ethernet_environment

SWARMING_SERVER = 'chromeos-swarming.appspot.com'


class SkylabEnvironment(
    local_device_ethernet_environment.LocalDeviceEthernetEnvironment):
  """
  A subclass of LocalDeviceEthernetEnvironment for Skylab devices.
  """

  def GetDeviceHostname(self):
    """Return the hostname based on the bot id.

    Strips the first component of the bot id, e.g. 'cros-clank1' -> 'clank1'.

    Gets the bot id from the SWARMING_BOT_ID envvar, see
    https://chromium.googlesource.com/infra/luci/luci-py/+/HEAD/appengine/swarming/doc/Magic-Values.md#bot-environment-variables.
    """
    bot_id = os.environ.get('SWARMING_BOT_ID')
    if not bot_id:
      raise ValueError(
          "device_arg is 'swarming' but SWARMING_BOT_ID is not set")

    return bot_id[bot_id.index("-") + 1:]
