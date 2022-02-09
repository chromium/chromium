// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {TopicSource} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change ambient state.
 */

export enum AmbientActionName {
  SET_AMBIENT_MODE_ENABLED = 'set_ambient_mode_enabled',
  SET_TOPIC_SOURCE = 'set_topic_source',
}

export type AmbientActions = SetAmbientModeEnabledAction|SetTopicSourceAction;

export type SetAmbientModeEnabledAction = Action&{
  name: AmbientActionName.SET_AMBIENT_MODE_ENABLED;
  enabled: boolean;
};

export type SetTopicSourceAction = Action&{
  name: AmbientActionName.SET_TOPIC_SOURCE;
  topicSource: TopicSource;
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
