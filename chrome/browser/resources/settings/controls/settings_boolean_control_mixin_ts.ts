// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1234307): Delete this file once
// settings_boolean_control_mixin.js has been migrated to TypeScript.

import {PrefControlMixinInterface} from './pref_control_mixin_ts.js';

export interface SettingsBooleanControlMixinInterface extends
    PrefControlMixinInterface {
  checked: boolean
  label: string;

  controlDisabled(): boolean;
  notifyChangedByUserInteraction(): void;
  resetToPrefValue(): void;
  sendPrefChange(): void;
}
