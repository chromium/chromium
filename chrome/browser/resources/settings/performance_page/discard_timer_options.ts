// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import type {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';

export function getDiscardTimerOptions(): DropdownMenuOptionList {
  return [
    {
      value: 5,
      name: loadTimeData.getString('tabDiscardTimerFiveMinutes'),
    },
    {
      value: 15,
      name: loadTimeData.getString('tabDiscardTimerFifteenMinutes'),
    },
    {
      value: 30,
      name: loadTimeData.getString('tabDiscardTimerThirtyMinutes'),
    },
    {
      value: 60,
      name: loadTimeData.getString('tabDiscardTimerOneHour'),
    },
    {
      value: 2 * 60,
      name: loadTimeData.getString('tabDiscardTimerTwoHours'),
    },
    {
      value: 4 * 60,
      name: loadTimeData.getString('tabDiscardTimerFourHours'),
    },
    {
      value: 8 * 60,
      name: loadTimeData.getString('tabDiscardTimerEightHours'),
    },
    {
      value: 16 * 60,
      name: loadTimeData.getString('tabDiscardTimerSixteenHours'),
    },
    {
      value: 24 * 60,
      name: loadTimeData.getString('tabDiscardTimerTwentyFourHours'),
    },
  ];
}
