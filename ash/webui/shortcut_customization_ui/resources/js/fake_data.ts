// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {keyToIconNameMap} from './input_key.js';
import {stringToMojoString16} from './mojo_utils.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutStyle, Modifier, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo} from './shortcut_types.js';

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
      layoutProperties: {
        defaultAccelerator: {
          keyDisplay: stringToMojoString16('['),
          accelerator: {
            modifiers: Modifier.ALT,
            keyCode: 219,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },
    }],
    // Snap Window Right
    [1]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      layoutProperties: {
        defaultAccelerator: {
          keyDisplay: stringToMojoString16(']'),
          accelerator: {
            modifiers: Modifier.ALT,
            keyCode: 221,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },

    }],
    // New Desk
    [2]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      layoutProperties: {
        defaultAccelerator: {
          keyDisplay: stringToMojoString16('+'),
          accelerator: {
            modifiers: Modifier.COMMAND | Modifier.SHIFT,
            keyCode: 187,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,

      },
    }],
    // Remove Desk
    [3]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      layoutProperties: {
        defaultAccelerator: {
          keyDisplay: stringToMojoString16('-'),
          accelerator: {
            modifiers: Modifier.COMMAND | Modifier.SHIFT,
            keyCode: 189,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,

      },
    }],
  },
  [AcceleratorSource.kBrowser]: {
    // New Tab
    [1001]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      layoutProperties: {
        defaultAccelerator: {
          keyDisplay: stringToMojoString16('t'),
          accelerator: {
            modifiers: Modifier.CONTROL,
            keyCode: 84,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,

      },
    }],
  },
};

export const fakeLayoutInfo: MojoLayoutInfo[] = [
  {
    category: AcceleratorCategory.kTabsAndWindows,
    subCategory: AcceleratorSubcategory.kGeneral,
    description: stringToMojoString16('Snap Window Left'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 0,
  },
  {
    category: AcceleratorCategory.kTabsAndWindows,
    subCategory: AcceleratorSubcategory.kGeneral,
    description: stringToMojoString16('Snap Window Right'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  {
    category: AcceleratorCategory.kTabsAndWindows,
    subCategory: AcceleratorSubcategory.kSystemApps,
    description: stringToMojoString16('Create Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  {
    category: AcceleratorCategory.kTabsAndWindows,
    subCategory: AcceleratorSubcategory.kSystemApps,
    description: stringToMojoString16('Remove Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 3,
  },
  {
    category: AcceleratorCategory.kPageAndWebBrowser,
    subCategory: AcceleratorSubcategory.kSystemControls,
    description: stringToMojoString16('New Tab'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kBrowser,
    action: 1001,
  },
];

// The following code is used to add fake accelerator entries for each icon.
// When useFakeProvider is true, this will display all available icons for
// the purposes of debugging.
const createFakeMojoAccelInfo = (keyDisplay: string): MojoAcceleratorInfo => {
  return {
    type: AcceleratorType.kDefault,
    state: AcceleratorState.kEnabled,
    locked: true,
    layoutProperties: {
      defaultAccelerator: {
        keyDisplay: stringToMojoString16(keyDisplay),
        accelerator: {
          modifiers: 0,
          keyCode: 0,
          keyState: 0,
          timeStamp: fakeTimestamp,
        },
      },
      textAccelerator: undefined,
    },
  };
};

const createFakeMojoLayoutInfo =
    (description: string, action: number): MojoLayoutInfo => {
      return {
        category: AcceleratorCategory.kPageAndWebBrowser,
        subCategory: AcceleratorSubcategory.kSystemControls,
        description: stringToMojoString16(description),
        style: LayoutStyle.kDefault,
        source: AcceleratorSource.kBrowser,
        action,
      };
    };

const icons = Object.keys(keyToIconNameMap);

for (const [index, iconName] of icons.entries()) {
  const actionId = 10000 + index;
  fakeAcceleratorConfig[AcceleratorSource.kBrowser] = {
    ...fakeAcceleratorConfig[AcceleratorSource.kBrowser],
    [actionId]: [createFakeMojoAccelInfo(iconName)],
  };
  fakeLayoutInfo.push(createFakeMojoLayoutInfo(`Icon: ${iconName}`, actionId));
}