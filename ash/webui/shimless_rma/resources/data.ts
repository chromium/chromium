// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ComponentType} from './shimless_rma.mojom-webui.js';

export const ComponentTypeToId: {[key in ComponentType]: string} = {
  [ComponentType.kComponentUnknown]: '',
  [ComponentType.kAudioCodec]: 'componentAudio',
  [ComponentType.kBattery]: 'componentBattery',
  [ComponentType.kStorage]: 'componentStorage',
  [ComponentType.kVpdCached]: 'componentVpdCached',
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
  [ComponentType.kPowerButton]: 'componentPowerButton',
};
