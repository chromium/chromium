// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mojoString16ToString} from '//resources/js/mojo_type_util.js';
import {BluetoothDeviceProperties, PairedBluetoothDeviceProperties} from '//resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {BatteryType} from './bluetooth_types.js';


/**
 *  WARNING: The returned string may contain malicious HTML and should not be
 *  used for Polymer bindings in CSS code. For additional information see
 *  b/298724102.
 */
export function getDeviceNameUnsafe(device: PairedBluetoothDeviceProperties|
                                    null): string {
  if (!device || (!device.nickname && !device.deviceProperties?.publicName)) {
    return '';
  }

  return device.nickname ||
      mojoString16ToString(device.deviceProperties.publicName);
}

/**
 * Returns the battery percentage of the battery type of the device, or
 * undefined if device does not exist, has no battery information describing
 * the battery type, or the battery percentage is out of bounds. Clients that
 * call this method should explicitly check if the return value is undefined to
 * differentiate it from a return value of 0.
 */
export function getBatteryPercentage(device: BluetoothDeviceProperties,
      batteryType: BatteryType) : number|undefined {
  if (!device) {
    return undefined;
  }

  const batteryInfo = device.batteryInfo;
  if (!batteryInfo) {
    return undefined;
  }

  let batteryProperties;
  switch (batteryType) {
    case BatteryType.DEFAULT:
      batteryProperties = batteryInfo.defaultProperties;
      break;
    case BatteryType.LEFT_BUD:
      batteryProperties = batteryInfo.leftBudInfo;
      break;
    case BatteryType.CASE:
      batteryProperties = batteryInfo.caseInfo;
      break;
    case BatteryType.RIGHT_BUD:
      batteryProperties = batteryInfo.rightBudInfo;
      break;
  }

  if (!batteryProperties) {
    return undefined;
  }

  const batteryPercentage = batteryProperties.batteryPercentage;
  if (batteryPercentage < 0 || batteryPercentage > 100) {
    return undefined;
  }

  return batteryPercentage;
}

/**
 * Returns true if the the device contains any multiple battery information.
 */
export function hasAnyDetailedBatteryInfo(device: BluetoothDeviceProperties): boolean {
  return getBatteryPercentage(device, BatteryType.LEFT_BUD) !== undefined ||
      getBatteryPercentage(device, BatteryType.CASE) !== undefined ||
      getBatteryPercentage(device, BatteryType.RIGHT_BUD) !== undefined;
}

/**
 * Returns true if the device contains the default image URL.
 */
export function hasDefaultImage(device: BluetoothDeviceProperties): boolean {
  return !!device && !!device.imageInfo && !!device.imageInfo.defaultImageUrl &&
      !!device.imageInfo.defaultImageUrl.url;
}

/**
 * Returns true if the device contains True Wireless Images.
 */
export function hasTrueWirelessImages(device: BluetoothDeviceProperties): boolean {
  const imageInfo = device.imageInfo;
  if (!imageInfo) {
    return false;
  }

  const trueWirelessImages = imageInfo.trueWirelessImages;
  if (!trueWirelessImages) {
    return false;
  }

  // Only return true if all True Wireless Images are present.
  const leftBudImageUrl = trueWirelessImages.leftBudImageUrl;
  const rightBudImageUrl = trueWirelessImages.rightBudImageUrl;
  const caseImageUrl = trueWirelessImages.caseImageUrl;
  if (!leftBudImageUrl || !rightBudImageUrl || !caseImageUrl) {
    return false;
  }

  return !!leftBudImageUrl.url && !!rightBudImageUrl.url && !!caseImageUrl.url;
}
