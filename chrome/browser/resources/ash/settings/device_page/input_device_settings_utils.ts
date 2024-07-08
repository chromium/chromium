// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {AudioOutputCapability, BluetoothDeviceProperties, DeviceConnectionState, DeviceType} from 'chrome://resources/mojo/chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom-webui.js';

import {Button, DeviceSettings, InputDeviceSettingsPolicy, InputDeviceType, KeyEvent, PolicyStatus} from './input_device_settings_types.js';

function objectsAreEqual(
    obj1: {[key: string]: any}, obj2: {[key: string]: any}): boolean {
  const keys1 = Object.keys(obj1);
  const keys2 = Object.keys(obj2);
  if (keys1.length !== keys2.length) {
    return false;
  }

  for (let i = 0; i < keys1.length; i++) {
    const key = keys1[i];
    const val1 = obj1[key];
    const val2 = obj2[key];
    if (val1 instanceof Object || val2 instanceof Object) {
      if (!(val1 instanceof Object) || !(val2 instanceof Object) ||
          !objectsAreEqual(val1, val2)) {
        return false;
      }
    } else if (val1 !== val2) {
      return false;
    }
  }

  return true;
}

function deviceInList(
    deviceId: number, deviceList: InputDeviceType[]): boolean {
  for (const device of deviceList) {
    if (device.id === deviceId) {
      return true;
    }
  }

  return false;
}

export function settingsAreEqual(
    settings1: DeviceSettings, settings2: DeviceSettings): boolean {
  return objectsAreEqual(settings1, settings2);
}

export function buttonsAreEqual(button1: Button, button2: Button): boolean {
  return objectsAreEqual(button1, button2);
}

export function keyEventsAreEqual(
    keyEvent1: KeyEvent, keyEvent2: KeyEvent): boolean {
  return objectsAreEqual(keyEvent1, keyEvent2);
}

interface PrefPolicyFields {
  controlledBy?: chrome.settingsPrivate.ControlledBy;
  enforcement?: chrome.settingsPrivate.Enforcement;
  recommendedValue?: boolean;
}

export function getPrefPolicyFields(policy: InputDeviceSettingsPolicy|
                                    null): PrefPolicyFields {
  if (policy) {
    const enforcement = policy.policyStatus === PolicyStatus.kManaged ?
        chrome.settingsPrivate.Enforcement.ENFORCED :
        chrome.settingsPrivate.Enforcement.RECOMMENDED;
    return {
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
      enforcement,
      recommendedValue: policy.value,
    };
  }

  // These fields must be set back to undefined so the html badge is properly
  // removed from the UI.
  return {
    controlledBy: undefined,
    enforcement: undefined,
    recommendedValue: undefined,
  };
}

export function getDeviceStateChangesToAnnounce(
    newDeviceList: InputDeviceType[],
    prevDeviceList: InputDeviceType[]): {msgId: string, deviceNames: string[]} {
  let msgId: string;
  let devices: InputDeviceType[];
  if (newDeviceList.length > prevDeviceList.length) {
    devices = newDeviceList.filter(
        (device) => !deviceInList(device.id, prevDeviceList));
    msgId = 'deviceConnectedA11yLabel';
  } else {
    msgId = 'deviceDisconnectedA11yLabel';
    devices = prevDeviceList.filter(
        (device) => !deviceInList(device.id, newDeviceList));
  }

  return {msgId, deviceNames: devices.map(device => device.name)};
}

export function createBluetoothDeviceProperties(
    id: string,
    publicName: string,
    batteryPercentage: number,
    ): BluetoothDeviceProperties {
  return {
    id: id,
    address: id,
    publicName: stringToMojoString16(publicName),
    deviceType: DeviceType.kMouse,
    audioCapability: AudioOutputCapability.kNotCapableOfAudioOutput,
    connectionState: DeviceConnectionState.kConnected,
    isBlockedByPolicy: false,
    batteryInfo: {
      defaultProperties: {batteryPercentage},
      leftBudInfo: undefined,
      rightBudInfo: undefined,
      caseInfo: undefined,
    },
    imageInfo: undefined,
  };
}
