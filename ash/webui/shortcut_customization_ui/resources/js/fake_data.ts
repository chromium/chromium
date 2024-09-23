// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import {TimeTicks} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {Accelerator, AcceleratorCategory, AcceleratorKeyState, AcceleratorSource, AcceleratorState, AcceleratorSubcategory, AcceleratorType, LayoutStyle, Modifier, MojoAcceleratorConfig, MojoAcceleratorInfo, MojoLayoutInfo, MojoSearchResult, TextAcceleratorPartType} from './shortcut_types.js';
import {keyToIconNameMap} from './shortcut_utils.js';

const fakeTimestamp: TimeTicks = {
  internalValue: BigInt(0),
};

const newTabAcceleratorInfo: MojoAcceleratorInfo = {
  type: AcceleratorType.kDefault,
  state: AcceleratorState.kEnabled,
  acceleratorLocked: false,
  locked: true,
  layoutProperties: {
    standardAccelerator: {
      originalAccelerator: null,
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

const cycleTabsAcceleratorInfo: MojoAcceleratorInfo = {
  type: AcceleratorType.kDefault,
  state: AcceleratorState.kEnabled,
  acceleratorLocked: false,
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

export const fakeAcceleratorConfig: MojoAcceleratorConfig = {
  [AcceleratorSource.kAsh]: {
    // Snap Window Left: alt + [.
    [0]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: true,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
    // Snap Window Right: alt + ].
    [1]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
    // New Desk: search + shift + '+'.
    [2]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
    // Remove Desk: search + shift + '-'.
    [3]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
    // Open Calculator app: 'LaunchApplication2' key.
    [4]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kDisabledByUnavailableKeys,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
          keyDisplay: stringToMojoString16('LaunchApplication2'),
          accelerator: {
            modifiers: Modifier.NONE,
            keyCode: 183,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },
    }],
    // Open Diagnostics app: search + ctrl + esc.
    [5]: [{
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
          keyDisplay: stringToMojoString16('esc'),
          accelerator: {
            modifiers: Modifier.COMMAND | Modifier.CONTROL,
            keyCode: 27,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },
    }],
    // Open/close Google assistant: search + a or 'LaunchAssistant' key.
    [6]: [
      {
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        acceleratorLocked: false,
        locked: false,
        layoutProperties: {
          standardAccelerator: {
            originalAccelerator: null,
            keyDisplay: stringToMojoString16('a'),
            accelerator: {
              modifiers: Modifier.COMMAND,
              keyCode: 65,
              keyState: 0,
              timeStamp: fakeTimestamp,
            },
          },
          textAccelerator: undefined,
        },
      },
      {
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kDisabledByUnavailableKeys,
        acceleratorLocked: false,
        locked: false,
        layoutProperties: {
          standardAccelerator: {
            originalAccelerator: null,
            keyDisplay: stringToMojoString16('LaunchAssistant'),
            accelerator: {
              modifiers: Modifier.NONE,
              keyCode: 153,
              keyState: 0,
              timeStamp: fakeTimestamp,
            },
          },
          textAccelerator: undefined,
        },
      },
    ],
  },
  // TODO(michaelcheco): Separate Browser and Ambient accelerators.
  [AcceleratorSource.kAmbient]: {
    // New Tab
    [0]: [newTabAcceleratorInfo],
    [1]: [cycleTabsAcceleratorInfo],
  },
};

export const fakeAmbientConfig: MojoAcceleratorConfig = {
  [AcceleratorSource.kAmbient]: {
    [0]: [newTabAcceleratorInfo],
    [1]: [cycleTabsAcceleratorInfo],
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
    category: AcceleratorCategory.kGeneral,
    subCategory: AcceleratorSubcategory.kApps,
    description: stringToMojoString16('Open Calculator app'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 4,
  },
  {
    category: AcceleratorCategory.kGeneral,
    subCategory: AcceleratorSubcategory.kApps,
    description: stringToMojoString16('Open Diagnostic app'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 5,
  },
  {
    category: AcceleratorCategory.kGeneral,
    subCategory: AcceleratorSubcategory.kGeneralControls,
    description: stringToMojoString16('Open/close Google assistant'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 6,
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
      acceleratorLocked: false,
      locked: true,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
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

export const TakeScreenshotSearchResult: MojoSearchResult = {
  acceleratorLayoutInfo: {
    category: AcceleratorCategory.kWindowsAndDesks,
    subCategory: AcceleratorSubcategory.kDesks,
    description:
        stringToMojoString16('Take full screenshot or screen recording'),
    style: LayoutStyle.kDefault,
    source: AcceleratorSource.kAsh,
    action: 2,
  },
  acceleratorInfos: [
    {
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
          keyDisplay: stringToMojoString16('LaunchApplication1'),  // overview
          accelerator: {
            modifiers: Modifier.CONTROL,
            keyCode: 0,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },
    },
    {
      type: AcceleratorType.kDefault,
      state: AcceleratorState.kEnabled,
      acceleratorLocked: false,
      locked: false,
      layoutProperties: {
        standardAccelerator: {
          originalAccelerator: null,
          keyDisplay: stringToMojoString16('PrintScreen'),  // screenshot
          accelerator: {
            modifiers: 0,
            keyCode: 0,
            keyState: 0,
            timeStamp: fakeTimestamp,
          },
        },
        textAccelerator: undefined,
      },
    },
  ],
  relevanceScore: 0.95,
};

export const CycleTabsTextSearchResult: MojoSearchResult = {
  acceleratorLayoutInfo: {
    category: AcceleratorCategory.kGeneral,
    subCategory: AcceleratorSubcategory.kApps,
    description: stringToMojoString16('Click or tap shelf icons 1-8'),
    style: LayoutStyle.kText,
    source: AcceleratorSource.kAsh,
    action: 1,
  },
  acceleratorInfos: [cycleTabsAcceleratorInfo],
  relevanceScore: 0.95,
};

export const fakeDefaultAccelerators: Accelerator[] = [
  {
    modifiers: Modifier.COMMAND | Modifier.SHIFT,
    keyCode: 187,
    keyState: AcceleratorKeyState.PRESSED,
  },
  {
    modifiers: Modifier.CONTROL,
    keyCode: 84,
    keyState: AcceleratorKeyState.PRESSED,
  },
];

export const createFakeMojoAccelInfo =
    (keyDisplay: string = 'a'): MojoAcceleratorInfo => {
      return {
        type: AcceleratorType.kDefault,
        state: AcceleratorState.kEnabled,
        acceleratorLocked: false,
        locked: true,
        layoutProperties: {
          standardAccelerator: {
            originalAccelerator: null,
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

export const createFakeMojoLayoutInfo =
    (description: string, action: number,
     category: AcceleratorCategory = AcceleratorCategory.kBrowser,
     source: AcceleratorSource = AcceleratorSource.kAmbient):
        MojoLayoutInfo => {
          return {
            category,
            subCategory: AcceleratorSubcategory.kTabs,
            description: stringToMojoString16(description),
            style: LayoutStyle.kDefault,
            source,
            action,
          };
        };

// The following code is used to add fake accelerator entries for each icon.
// When useFakeProvider is true, this will display all available icons for
// the purposes of debugging.
const icons = Object.keys(keyToIconNameMap);

for (const [index, iconName] of icons.entries()) {
  const actionId = 10000 + index;
  fakeAcceleratorConfig[AcceleratorSource.kAmbient] = {
    ...fakeAcceleratorConfig[AcceleratorSource.kAmbient],
    [actionId]: [createFakeMojoAccelInfo(iconName)],
  };
  fakeLayoutInfo.push(createFakeMojoLayoutInfo(`Icon: ${iconName}`, actionId));
}
