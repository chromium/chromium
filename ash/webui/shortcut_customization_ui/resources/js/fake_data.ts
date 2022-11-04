// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorConfig, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfoList, LayoutStyle, Modifier} from './shortcut_types.js';

export const fakeSubCategories: Map<number, string> = new Map([
  [0, 'Window Management'],
  [1, 'Virtual Desks'],
  [2, 'Tabs'],
]);

export const fakeAcceleratorConfig: AcceleratorConfig = {
  [AcceleratorSource.kAsh]: {
    // Snap Window Left
    [0]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      keyDisplay: '[',
      hasKeyEvent: true,
      accelerator: {
        modifiers: Modifier.ALT,
        keyCode: 219,
      },
    }],
    // Snap Window Right
    [1]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      keyDisplay: ']',
      hasKeyEvent: true,
      accelerator: {
        modifiers: Modifier.ALT,
        keyCode: 221,
      },
    }],
    // New Desk
    [2]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      keyDisplay: '+',
      hasKeyEvent: true,
      accelerator: {
        modifiers: Modifier.COMMAND | Modifier.SHIFT,
        keyCode: 187,
      },
    }],
    // Remove Desk
    [3]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      keyDisplay: '-',
      hasKeyEvent: true,
      accelerator: {
        modifiers: Modifier.COMMAND | Modifier.SHIFT,
        keyCode: 189,
      },
    }],
  },
  [AcceleratorSource.kBrowser]: {
    // New Tab
    [1001]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      keyDisplay: 't',
      hasKeyEvent: true,
      accelerator: {
        modifiers: Modifier.CONTROL,
        keyCode: 84,
      },
    }],
  },
};

export const fakeLayoutInfo: LayoutInfoList = [
  {
    category: 0,     // Chrome OS.
    subCategory: 0,  // Window Management.
    description: 'Snap Window Left',
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 0,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 0,  // Window Management.
    description: 'Snap Window Right',
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 1,  // Virtual Desks.
    description: 'Create Desk',
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  {
    category: 0,     // Chrome OS.
    subCategory: 1,  // Virtual Desks.
    description: 'Remove Desk',
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 3,
  },
  {
    category: 1,     // Browser.
    subCategory: 2,  // Tabs.
    description: 'New Tab',
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kBrowser,
    action: 1001,
  },
];
