// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Actions} from '../personalization_actions.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {AmbientActionName} from './ambient_actions.js';
import {AmbientState} from './ambient_state.js';

export function ambientModeEnabledReducer(
    state: AmbientState['ambientModeEnabled'], action: Actions,
    _: PersonalizationState): AmbientState['ambientModeEnabled'] {
  switch (action.name) {
    case AmbientActionName.SET_AMBIENT_MODE_ENABLED:
      return action.enabled;
    default:
      return state;
  }
}

export function topicSourceReducer(
    state: AmbientState['topicSource'], action: Actions,
    _: PersonalizationState): AmbientState['topicSource'] {
  switch (action.name) {
    case AmbientActionName.SET_TOPIC_SOURCE:
      return action.topicSource;
    default:
      return state;
  }
}

export const ambientReducers:
    {[K in keyof AmbientState]: ReducerFunction<AmbientState[K]>} = {
      ambientModeEnabled: ambientModeEnabledReducer,
      topicSource: topicSourceReducer,
    };
