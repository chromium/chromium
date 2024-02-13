// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './screens/common/user_creation.js';

//TODO(b/324392321) Move type definition to oobe_types after its TS migration
export interface OobeScreen {
  tag: string;
  id: string;
  condition?: string;
  extra_classes?: string[];
}
export interface ScreensList extends Array<OobeScreen>{}

export const priorityCommonScreenList: ScreensList =
    [{tag: 'user-creation-element', id: 'user-creation'}];
