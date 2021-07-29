// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorConfig, AcceleratorInfo, AcceleratorKeys, AcceleratorSource, AcceleratorState, AcceleratorType,} from './shortcut_types.js'

const SHIFT = 1 << 1;
const CONTROL = 1 << 2;
const ALT = 1 << 3;
const COMMAND = 1 << 4;

/* @type {!Map<number, string>} */
export const fakeActionNames = new Map([
  [0, 'Snap Window Left'],
  [1, 'Snap Window Right'],
  [2, 'New Desk'],
  [3, 'Remove Desk'],
]);

/* @type {!AcceleratorConfig} */
export const fakeAcceleratorConfig = new Map([[
  AcceleratorSource.kAsh, new Map([
    // Snap Window Left
    [
      0, [{
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        accelerator: {
          modifiers: ALT,
          key: 219,
          key_display: '[',
        }
      }]
    ],
    // Snap Window Right
    [
      1, [{
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        accelerator: {
          modifiers: ALT,
          key: 221,
          key_display: ']',
        }
      }]
    ],
    // New Desk
    [
      2, [{
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        accelerator: {
          modifiers: COMMAND | SHIFT,
          key: 187,
          key_display: '+',
        }
      }]
    ],
    // Remove Desk
    [
      3, [{
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        accelerator: {
          modifiers: COMMAND | SHIFT,
          key: 189,
          key_display: '-',
        }
      }]
    ],
  ])
]]);
