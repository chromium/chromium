// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Action} from 'chrome://resources/js/cr/ui/store.js';

import {UserInfo} from '../personalization_app.mojom-webui.js';

/**
 * @fileoverview Defines the actions to change user state.
 */

export enum UserActionName {
  SET_USER_INFO = 'set_user_info',
}

export type UserActions = SetUserInfoAction;

export type SetUserInfoAction = Action&{
  name: UserActionName.SET_USER_INFO;
  user_info: UserInfo;
};

/**
 * Notify that the app has finished loading user info. Will be called with null
 * on error.
 */
export function setUserInfoAction(user_info: UserInfo): SetUserInfoAction {
  return {
    name: UserActionName.SET_USER_INFO,
    user_info,
  };
}