// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SwitchAccessCommand, SwitchAccessDeviceType} from './switch_access_constants.js';

export interface KeyAssignment {
  key: string;
  device: SwitchAccessDeviceType;
}

export type SwitchAccessAssignmentsChangedValue = {
  [key in SwitchAccessCommand]: KeyAssignment[];
};
