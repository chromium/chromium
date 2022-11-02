// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorConfig, AcceleratorSource, AcceleratorState, AcceleratorType, LayoutInfoList, LayoutStyle, Modifier} from './shortcut_types.js';

export const fakeActionNames: Map<number, string> = new Map([
  [0, 'Snap Window Left'],
  [1, 'Snap Window Right'],
  [2, 'New Desk'],
  [3, 'Remove Desk'],
  [1001, 'New Tab'],
]);

export const fakeCategories: Map<number, string> = new Map([
  [0, 'Chrome OS'],
  [1, 'Browser'],
]);

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
    category: 0,      // Chrome OS.
    sub_category: 0,  // Window Management.
    description: 0,   // Snap Window Left.
    layout_style: LayoutStyle.DEFAULT,
    source: AcceleratorSource.kAsh,
    action: 0,
  },
  {
    category: 0,      // Chrome OS.
    sub_category: 0,  // Window Management.
    description: 1,   // Snap Window Right.
    layout_style: LayoutStyle.DEFAULT,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  {
    category: 0,      // Chrome OS.
    sub_category: 1,  // Virtual Desks.
    description: 2,   // Create Desk.
    layout_style: LayoutStyle.DEFAULT,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  {
    category: 0,      // Chrome OS.
    sub_category: 1,  // Virtual Desks.
    description: 3,   // Remove Desk.
    layout_style: LayoutStyle.DEFAULT,
    source: AcceleratorSource.kAsh,
    action: 3,
  },
  {
    category: 1,        // Browser.
    sub_category: 2,    // Tabs.
    description: 1001,  // New tab.
    layout_style: LayoutStyle.DEFAULT,
    source: AcceleratorSource.kBrowser,
    action: 1001,
  },
];
