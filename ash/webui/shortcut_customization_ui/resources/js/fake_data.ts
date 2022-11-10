// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {stringToMojoString16} from './mojo_utils.js';
import {AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutStyle, Modifier, MojoAcceleratorConfig, MojoLayoutInfo} from './shortcut_types.js';

export const fakeSubCategories: Map<AcceleratorSubcategory, string> = new Map([
  [0, 'Window Management'],
  [1, 'Virtual Desks'],
  [2, 'Tabs'],
]);

const fakeTimestamp: TimeTicks = {
  internalValue: BigInt(0),
};

export const fakeAcceleratorConfig: MojoAcceleratorConfig = {
  [AcceleratorSource.kAsh]: {
    // Snap Window Left
    [0]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      hasKeyEvent: true,
      keyDisplay: stringToMojoString16('['),
      accelerator: {
        modifiers: Modifier.ALT,
        keyCode: 219,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    }],
    // Snap Window Right
    [1]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      hasKeyEvent: true,
      keyDisplay: stringToMojoString16(']'),
      accelerator: {
        modifiers: Modifier.ALT,
        keyCode: 221,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    }],
    // New Desk
    [2]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      hasKeyEvent: true,
      keyDisplay: stringToMojoString16('+'),
      accelerator: {
        modifiers: Modifier.COMMAND | Modifier.SHIFT,
        keyCode: 187,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    }],
    // Remove Desk
    [3]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      hasKeyEvent: true,
      keyDisplay: stringToMojoString16('-'),
      accelerator: {
        modifiers: Modifier.COMMAND | Modifier.SHIFT,
        keyCode: 189,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    }],
  },
  [AcceleratorSource.kBrowser]: {
    // New Tab
    [1001]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      hasKeyEvent: true,
      keyDisplay: stringToMojoString16('t'),
      accelerator: {
        modifiers: Modifier.CONTROL,
        keyCode: 84,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    }],
  },
};

export const fakeLayoutInfo: MojoLayoutInfo[] = [
  {
    category: 0,     // Chrome OS.
    subCategory: 0,  // Window Management.
    description: stringToMojoString16('Snap Window Left'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 0,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 0,  // Window Management.
    description: stringToMojoString16('Snap Window Right'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 1,  // Virtual Desks.
    description: stringToMojoString16('Create Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 1,  // Virtual Desks.
    description: stringToMojoString16('Remove Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 3,
  },
  {
    category: 1,     // Browser.
    subCategory: 2,  // Tabs.
    description: stringToMojoString16('New Tab'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kBrowser,
    action: 1001,
  },
];
