// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Actions} from '../personalization_actions.js';
import {ReducerFunction} from '../personalization_reducers.js';
import {PersonalizationState} from '../personalization_state.js';

import {UserActionName} from './user_actions.js';
import {UserState} from './user_state.js';

export function infoReducer(
    state: UserState['info'], action: Actions,
    _: PersonalizationState): UserState['info'] {
  switch (action.name) {
    case UserActionName.SET_USER_INFO:
      return action.user_info;
    default:
      return state;
  }
}

export const userReducers:
    {[K in keyof UserState]: ReducerFunction<UserState[K]>} = {
      info: infoReducer,
    };
