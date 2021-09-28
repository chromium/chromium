// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/1234307): Delete this file once
// settings_radio_group.js has been migrated to TypeScript.

import {PrefControlMixinInterface} from './pref_control_behavior_ts.js';

export interface SettingsRadioGroupElement extends PrefControlMixinInterface,
                                                   HTMLElement {
  selected: string;
  sendPrefChange(): void;
  resetToPrefValue(): void;
}
