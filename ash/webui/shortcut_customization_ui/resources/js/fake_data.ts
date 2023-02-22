// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {keyToIconNameMap} from './input_key.js';
import {stringToMojoString16} from './mojo_utils.js';
import {AcceleratorCategory, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutStyle, Modifier, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo, MojoSearchResult, TextAcceleratorPartType} from './shortcut_types.js';

const fakeTimestamp: TimeTicks = {
  internalValue: BigInt(0),
};

const newTabAccelerator: MojoAcceleratorInfo = {
  type: AcceleratorType.kDefault,
  state: AcceleratorState.kEnabled,
  locked: true,
  layoutProperties: {
    standardAccelerator: {
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
};

const cycleTabsAccelerator: MojoAcceleratorInfo = {
  type: AcceleratorType.kDefault,
  state: AcceleratorState.kEnabled,
  locked: true,
  layoutProperties: {
    textAccelerator: {
      parts: [
        {
          text: stringToMojoString16('ctrl'),
          type: TextAcceleratorPartType.kModifier,
        },
        {
          text: stringToMojoString16(' + '),
          type: TextAcceleratorPartType.kDelimiter,
        },
        {
          text: stringToMojoString16('1 '),
          type: TextAcceleratorPartType.kKey,
        },
        {
          text: stringToMojoString16('through '),
          type: TextAcceleratorPartType.kPlainText,
        },
        {
          text: stringToMojoString16('8'),
          type: TextAcceleratorPartType.kKey,
        },
      ],
    },
    standardAccelerator: undefined,
  },
};

const sixPackDeleteAccelerator: MojoAcceleratorInfo = {
  type: AcceleratorType.kDefault,
  state: AcceleratorState.kEnabled,
  locked: true,
  layoutProperties: {
    standardAccelerator: {
      keyDisplay: stringToMojoString16('backspace'),
      accelerator: {
        modifiers: Modifier.COMMAND,
        keyCode: 8,
        keyState: 0,
        timeStamp: fakeTimestamp,
      },
    },
    textAccelerator: undefined,

  },
};

export const fakeAcceleratorConfig: MojoAcceleratorConfig = {
  [AcceleratorSource.kAsh]: {
    // Snap Window Left
    [0]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      layoutProperties: {
        standardAccelerator: {
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
        standardAccelerator: {
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
        standardAccelerator: {
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
        standardAccelerator: {
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
  // TODO(michaelcheco): Separate Browser and Ambient accelerators.
  [AcceleratorSource.kAmbient]: {
    // New Tab
    [0]: [newTabAccelerator],
    [1]: [cycleTabsAccelerator],
    [2]: [sixPackDeleteAccelerator],
  },
};

export const fakeAmbientConfig: MojoAcceleratorConfig = {
  [AcceleratorSource.kAmbient]: {
    [0]: [newTabAccelerator],
    [1]: [cycleTabsAccelerator],
    [2]: [sixPackDeleteAccelerator],
  },
};

export const fakeLayoutInfo: MojoLayoutInfo[] = [
  {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kWindows,
    description: stringToMojoString16('Snap Window Left'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 0,
  },
  {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kWindows,
    description: stringToMojoString16('Snap Window Right'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kDesks,
    description: stringToMojoString16('Create Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kDesks,
    description: stringToMojoString16('Remove Desk'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 3,
  },
  {
    category: AcceleratorCategory.kBrowser,
    subCategory: AcceleratorSubcategory.kTabs,
    description: stringToMojoString16('New Tab'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAmbient,
    action: 0,
  },
  {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kWindows,
    description: stringToMojoString16('Go to windows 1 through 8'),
    style: LayoutStyle.kText,
    source: AcceleratorSource.kAmbient,
    action: 1,
  },
  {
    category: AcceleratorCategory.kEventRewriter,
    subCategory: AcceleratorSubcategory.kSixPackKeys,
    description: stringToMojoString16('Delete'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAmbient,
    action: 2,
  },
];

export const fakeSearchResults: MojoSearchResult[] = [
  {
    acceleratorLayoutInfo: {
      category: AcceleratorCategory.kWindowsAndDesks,
      subCategory: AcceleratorSubcategory.kWindows,
      description: stringToMojoString16('Snap Window Left'),
      style: LayoutStyle.kDefault,
      source: AcceleratorSource.kAsh,
      action: 0,
    },
    acceleratorInfos: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: true,
      layoutProperties: {
        standardAccelerator: {
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
    relevanceScore: 0.8,
  },
  {
    acceleratorLayoutInfo: {
      category: AcceleratorCategory.kWindowsAndDesks,
      subCategory: AcceleratorSubcategory.kWindows,
      description: stringToMojoString16('Snap Window Right'),
      style: LayoutStyle.kDefault,
      source: AcceleratorSource.kAsh,
      action: 1,
    },
    acceleratorInfos: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
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
    relevanceScore: 0.6,
  },
  {
    acceleratorLayoutInfo: {
      category: AcceleratorCategory.kWindowsAndDesks,
      subCategory: AcceleratorSubcategory.kDesks,
      description: stringToMojoString16('Create Desk'),
      style: LayoutStyle.kDefault,
      source: AcceleratorSource.kAsh,
      action: 2,
    },
    acceleratorInfos: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
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
    relevanceScore: 0.4,
  },
];

export const SnapWindowLeftSearchResult: MojoSearchResult =
    fakeSearchResults[0];

// The following code is used to add fake accelerator entries for each icon.
// When useFakeProvider is true, this will display all available icons for
// the purposes of debugging.
const createFakeMojoAccelInfo = (keyDisplay: string): MojoAcceleratorInfo => {
  return {
    type: AcceleratorType.kDefault,
    state: AcceleratorState.kEnabled,
    locked: true,
    layoutProperties: {
      standardAccelerator: {
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
        category: AcceleratorCategory.kBrowser,
        subCategory: AcceleratorSubcategory.kTabs,
        description: stringToMojoString16(description),
        style: LayoutStyle.kDefault,
        source: AcceleratorSource.kAmbient,
        action,
      };
    };

const icons = Object.keys(keyToIconNameMap);

for (const [index, iconName] of icons.entries()) {
  const actionId = 10000 + index;
  fakeAcceleratorConfig[AcceleratorSource.kAmbient] = {
    ...fakeAcceleratorConfig[AcceleratorSource.kAmbient],
    [actionId]: [createFakeMojoAccelInfo(iconName)],
  };
  fakeLayoutInfo.push(createFakeMojoLayoutInfo(`Icon: ${iconName}`, actionId));
}
