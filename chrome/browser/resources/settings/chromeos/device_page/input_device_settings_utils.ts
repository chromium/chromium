// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceSettings, InputDeviceSettingsPolicy, PolicyStatus} from './input_device_settings_types.js';

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

export function settingsAreEqual(
    settings1: DeviceSettings, settings2: DeviceSettings): boolean {
  return objectsAreEqual(settings1, settings2);
}

interface PrefPolicyFields {
  controlledBy?: chrome.settingsPrivate.ControlledBy;
  enforcement?: chrome.settingsPrivate.Enforcement;
  recommendedValue?: boolean;
}

export function getPrefPolicyFields(policy?: InputDeviceSettingsPolicy):
    PrefPolicyFields {
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
