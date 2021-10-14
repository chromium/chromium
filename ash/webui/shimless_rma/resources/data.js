// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComponentType} from './shimless_rma_types.js';

// TODO(gavindodd): i18n strings
/**
 * @type {!Object<!ComponentType, string>}
 */
export const ComponentTypeToName = {
  [ComponentType.kAudioCodec]: 'Audio',
  [ComponentType.kBattery]: 'Battery',
  [ComponentType.kStorage]: 'Storage',
  [ComponentType.kVpdCached]: 'Vpd Cached',
  [ComponentType.kNetwork]: 'Network',
  [ComponentType.kCamera]: 'Camera',
  [ComponentType.kStylus]: 'Stylus',
  [ComponentType.kTouchpad]: 'Touchpad',
  [ComponentType.kTouchsreen]: 'Touchscreen',
  [ComponentType.kDram]: 'Memory',
  [ComponentType.kDisplayPanel]: 'Display',
  [ComponentType.kCellular]: 'Cellular',
  [ComponentType.kEthernet]: 'Ethernet',
  [ComponentType.kWireless]: 'Wireless',
  [ComponentType.kBaseAccelerometer]: 'Base Accelerometer',
  [ComponentType.kLidAccelerometer]: 'Lid Accelerometer',
  [ComponentType.kBaseGyroscope]: 'Base Gyroscope',
  [ComponentType.kLidGyroscope]: 'Lid Gyroscope',
  [ComponentType.kScreen]: 'Screen',
  [ComponentType.kKeyboard]: 'Keyboard',
  [ComponentType.kPowerButton]: 'Power Button'
};

/**
 * @type {!Object<!ComponentType, string>}
 */
export const ComponentTypeToId = {
  [ComponentType.kAudioCodec]: 'componentAudio',
  [ComponentType.kBattery]: 'componentBattery',
  [ComponentType.kStorage]: 'componentStorage',
  [ComponentType.kVpdCached]: 'componentVpd Cached',
  [ComponentType.kNetwork]: 'componentNetwork',
  [ComponentType.kCamera]: 'componentCamera',
  [ComponentType.kStylus]: 'componentStylus',
  [ComponentType.kTouchpad]: 'componentTouchpad',
  [ComponentType.kTouchsreen]: 'componentTouchscreen',
  [ComponentType.kDram]: 'componentDram',
  [ComponentType.kDisplayPanel]: 'componentDisplayPanel',
  [ComponentType.kCellular]: 'componentCellular',
  [ComponentType.kEthernet]: 'componentEthernet',
  [ComponentType.kWireless]: 'componentWireless',
  [ComponentType.kBaseAccelerometer]: 'componentBaseAccelerometer',
  [ComponentType.kLidAccelerometer]: 'componentLidAccelerometer',
  [ComponentType.kBaseGyroscope]: 'componentBaseGyroscope',
  [ComponentType.kLidGyroscope]: 'componentLidGyroscope',
  [ComponentType.kScreen]: 'componentScreen',
  [ComponentType.kKeyboard]: 'componentKeyboard',
  [ComponentType.kPowerButton]: 'componentPowerButton'
};
