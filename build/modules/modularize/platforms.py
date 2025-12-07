# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import enum


class Os(str, enum.Enum):
  Android = 'android'
  Fuchsia = 'fuchsia'
  Ios = 'ios'
  Linux = 'linux'
  Mac = 'mac'
  Win = 'win'

  @property
  def is_apple(self):
    return self == Os.Mac or self == Os.Ios


class Cpu(str, enum.Enum):
  x86 = 'x86'
  x64 = 'x64'
  arm = 'arm'
  arm64 = 'arm64'
