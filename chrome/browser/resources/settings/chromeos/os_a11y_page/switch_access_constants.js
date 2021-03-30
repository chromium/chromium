// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Available commands.
 * @enum {string}
 */
/* #export */ const SwitchAccessCommand = {
  NEXT: 'next',
  PREVIOUS: 'previous',
  SELECT: 'select'
};

/**
 * Possible device types for Switch Access.
 * @enum {string}
 */
/* #export */ const SwitchAccessDeviceType = {
  INTERNAL: 'internal',
  USB: 'usb',
  BLUETOOTH: 'bluetooth',
  UNKNOWN: 'unknown'
};