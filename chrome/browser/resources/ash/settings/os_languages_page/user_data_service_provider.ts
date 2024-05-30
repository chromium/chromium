// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Singleton that provides a remote for the UserDataService.
 */
import {InputMethodUserDataService, InputMethodUserDataServiceInterface} from '../mojom-webui/input_method_user_data.mojom-webui.js';

export class UserDataServiceProvider {
  private static instance_: InputMethodUserDataServiceInterface|null = null;

  static getRemote(): InputMethodUserDataServiceInterface {
    if (UserDataServiceProvider.instance_ === null) {
      UserDataServiceProvider.instance_ =
          InputMethodUserDataService.getRemote();
    }
    return UserDataServiceProvider.instance_;
  }
}
