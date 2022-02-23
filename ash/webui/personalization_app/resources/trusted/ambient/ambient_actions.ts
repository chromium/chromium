// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {TemperatureUnit, TopicSource} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change ambient state.
 */

export enum AmbientActionName {
  SET_AMBIENT_MODE_ENABLED = 'set_ambient_mode_enabled',
  SET_TOPIC_SOURCE = 'set_topic_source',
  SET_TEMPERATURE_UNIT = 'set_temperature_unit',
}

export type AmbientActions =
    SetAmbientModeEnabledAction|SetTopicSourceAction|SetTemperatureUnitAction;

export type SetAmbientModeEnabledAction = Action&{
  name: AmbientActionName.SET_AMBIENT_MODE_ENABLED;
  enabled: boolean;
};

export type SetTopicSourceAction = Action&{
  name: AmbientActionName.SET_TOPIC_SOURCE;
  topicSource: TopicSource;
};

export type SetTemperatureUnitAction = Action&{
  name: AmbientActionName.SET_TEMPERATURE_UNIT;
  temperatureUnit: TemperatureUnit;
};

/**
 * Sets the current value of the ambient mode pref.
 */
export function setAmbientModeEnabledAction(enabled: boolean):
    SetAmbientModeEnabledAction {
  return {name: AmbientActionName.SET_AMBIENT_MODE_ENABLED, enabled};
}

/**
 * Sets the current value of the topic source.
 */
export function setTopicSourceAction(topicSource: TopicSource):
    SetTopicSourceAction {
  return {name: AmbientActionName.SET_TOPIC_SOURCE, topicSource};
}

/**
 * Sets the current value of the temperature unit.
 */
export function setTemperatureUnitAction(temperatureUnit: TemperatureUnit):
    SetTemperatureUnitAction {
  return {name: AmbientActionName.SET_TEMPERATURE_UNIT, temperatureUnit};
}
