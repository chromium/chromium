// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorAction, ActionChoice, ChargeState, CompanionAppState, CustomizableButton, CustomizationRestriction, ExtendedFkeysModifier, GraphicsTablet, GraphicsTabletButtonConfig, Keyboard, MetaKey, ModifierKey, Mouse, MouseButtonConfig, PointingStick, SimulateRightClickModifier, SixPackKeyInfo, SixPackShortcutModifier, StaticShortcutAction, Stylus, TopRowActionKey, Touchpad, Vkey} from './input_device_settings_types.js';

const defaultSixPackKeyRemappings: SixPackKeyInfo = {
  pageDown: SixPackShortcutModifier.kSearch,
  pageUp: SixPackShortcutModifier.kSearch,
  del: SixPackShortcutModifier.kSearch,
  insert: SixPackShortcutModifier.kSearch,
  home: SixPackShortcutModifier.kSearch,
  end: SixPackShortcutModifier.kSearch,
};

export const fakeKeyboards: Keyboard[] = [
  {
    id: 0,
    deviceKey: 'test:key',
    name: 'ERGO K860',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: null,
      f12: null,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
  {
    id: 1,
    deviceKey: 'test:key',
    name: 'AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.kSearch,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    settings: {
      modifierRemappings: {},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: true,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: ExtendedFkeysModifier.kAlt,
      f12: ExtendedFkeysModifier.kShift,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
  },
  {
    id: 8,
    deviceKey: 'test:key',
    name: 'Logitech G713 Aurora',
    isExternal: true,
    metaKey: MetaKey.kLauncher,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {[ModifierKey.kAlt]: ModifierKey.kAssistant},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: null,
      f12: null,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
  {
    id: 9,
    deviceKey: 'test:key',
    name: 'Fake ERGO K861',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: {
        pageDown: SixPackShortcutModifier.kSearch,
        pageUp: SixPackShortcutModifier.kSearch,
        del: SixPackShortcutModifier.kAlt,
        insert: SixPackShortcutModifier.kSearch,
        home: SixPackShortcutModifier.kAlt,
        end: SixPackShortcutModifier.kAlt,
      },
      f11: null,
      f12: null,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
  {
    id: 17,
    deviceKey: 'test:key',
    name: 'Split Modifier keyboard',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
      ModifierKey.kRightAlt,
      ModifierKey.kFunction,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: ExtendedFkeysModifier.kAlt,
      f12: ExtendedFkeysModifier.kShift,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
];

export const fakeKeyboards2: Keyboard[] = [
  {
    id: 9,
    deviceKey: 'test:key',
    name: 'Fake ERGO K860',
    isExternal: true,
    metaKey: MetaKey.kCommand,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kBackspace,
      ModifierKey.kCapsLock,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {
        [ModifierKey.kControl]: ModifierKey.kCapsLock,
        [ModifierKey.kCapsLock]: ModifierKey.kAssistant,
      },
      topRowAreFkeys: false,
      suppressMetaFkeyRewrites: false,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: null,
      f12: null,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
  {
    id: 10,
    deviceKey: 'test:key',
    name: 'Fake AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.kSearch,
    modifierKeys: [
      ModifierKey.kAlt,
      ModifierKey.kAssistant,
      ModifierKey.kBackspace,
      ModifierKey.kControl,
      ModifierKey.kEscape,
      ModifierKey.kMeta,
    ],
    topRowActionKeys: [
      TopRowActionKey.kBack,
      TopRowActionKey.kForward,
      TopRowActionKey.kRefresh,
      TopRowActionKey.kFullscreen,
      TopRowActionKey.kOverview,
      TopRowActionKey.kScreenBrightnessDown,
      TopRowActionKey.kScreenBrightnessUp,
      TopRowActionKey.kVolumeMute,
      TopRowActionKey.kVolumeDown,
      TopRowActionKey.kVolumeUp,
    ],
    settings: {
      modifierRemappings: {},
      topRowAreFkeys: true,
      suppressMetaFkeyRewrites: true,
      sixPackKeyRemappings: defaultSixPackKeyRemappings,
      f11: null,
      f12: null,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
];

export const fakeTouchpads: Touchpad[] = [
  {
    id: 2,
    deviceKey: 'test:key',
    name: 'Default Touchpad',
    isExternal: false,
    isHaptic: true,
    settings: {
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      tapToClickEnabled: false,
      threeFingerClickEnabled: false,
      tapDraggingEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
      hapticSensitivity: 1,
      hapticEnabled: false,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
  {
    id: 3,
    deviceKey: 'test:key',
    name: 'Logitech T650',
    isExternal: true,
    isHaptic: false,
    settings: {
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      tapToClickEnabled: true,
      threeFingerClickEnabled: true,
      tapDraggingEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
      hapticSensitivity: 5,
      hapticEnabled: true,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
];

export const fakeTouchpads2: Touchpad[] = [
  {
    id: 11,
    deviceKey: 'test:key',
    name: 'Fake Default Touchpad',
    isExternal: false,
    isHaptic: true,
    settings: {
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      tapToClickEnabled: false,
      threeFingerClickEnabled: false,
      tapDraggingEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
      hapticSensitivity: 1,
      hapticEnabled: false,
      simulateRightClick: SimulateRightClickModifier.kNone,
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: null,
  },
];

export const fakeMice: Mouse[] = [
  {
    id: 4,
    deviceKey: 'test:key',
    name: 'Razer Basilisk V3',
    isExternal: true,
    customizationRestriction: CustomizationRestriction.kAllowCustomizations,
    mouseButtonConfig: MouseButtonConfig.kNoConfig,
    settings: {
      swapRight: true,
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
      buttonRemappings: [
        {
          name: 'Back Button',
          button: {
            customizableButton: CustomizableButton.kBack,
          },
          remappingAction: {
            staticShortcutAction: StaticShortcutAction.kDisable,
          },
        },
        {
          name: 'Forward Button',
          button: {
            customizableButton: CustomizableButton.kForward,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kCycleForwardMru,
          },
        },
        {
          name: 'Undo',
          button: {
            customizableButton: CustomizableButton.kExtra,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
              keyDisplay: 'z',
            },
          },
        },
        {
          name: 'Redo',
          button: {
            customizableButton: CustomizableButton.kSide,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 6,
              keyDisplay: 'z',
            },
          },
        },
      ],
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: {
      packageId: '',
      appName: '',
      actionLink: '',
      iconUrl: '',
      state: CompanionAppState.kAvailable,
    },
  },
  {
    id: 5,
    deviceKey: 'test:key',
    name: 'MX Anywhere 2S',
    isExternal: false,
    customizationRestriction: CustomizationRestriction.kDisableKeyEventRewrites,
    mouseButtonConfig: MouseButtonConfig.kFiveKey,
    settings: {
      swapRight: false,
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
      buttonRemappings: [
        {
          name: 'Chrome Vox',
          button: {
            customizableButton: CustomizableButton.kSide,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 10,
              keyDisplay: 'z',
            },
          },
        },
        {
          name: 'Open Clipboard',
          button: {
            customizableButton: CustomizableButton.kMiddle,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kToggleClipboardHistory,
          },
        },
      ],
    },
    batteryInfo: null,
    appInfo: {
      packageId: '',
      appName: '',
      actionLink: '',
      iconUrl: '',
      state: CompanionAppState.kInstalled,
    },
  },
];

export const fakeMice2: Mouse[] = [
  {
    id: 13,
    deviceKey: 'test:key',
    name: 'Fake Razer Basilisk V3',
    isExternal: true,
    customizationRestriction: CustomizationRestriction.kDisallowCustomizations,
    mouseButtonConfig: MouseButtonConfig.kNoConfig,
    settings: {
      swapRight: true,
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
      buttonRemappings: [],
    },
    batteryInfo: {
      chargeState: ChargeState.kCharging,
      batteryPercentage: 10,
    },
    appInfo: {
      packageId: '',
      appName: '',
      actionLink: '',
      iconUrl: '',
      state: CompanionAppState.kAvailable,
    },
  },
];

export const fakePointingSticks: PointingStick[] = [
  {
    id: 6,
    deviceKey: 'test:key',
    name: 'Default Pointing Stick',
    isExternal: false,
    settings: {
      swapRight: false,
      sensitivity: 1,
      accelerationEnabled: false,
    },
  },
  {
    id: 7,
    deviceKey: 'test:key',
    name: 'Lexmark-Unicomp FSR',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      accelerationEnabled: true,
    },
  },
];

export const fakePointingSticks2: PointingStick[] = [
  {
    id: 12,
    deviceKey: 'test:key',
    name: 'Fake Lexmark-Unicomp FSR',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      accelerationEnabled: true,
    },
  },
];

export const fakeStyluses: Stylus[] = [
  {
    id: 13,
    deviceKey: 'test:key',
    name: 'Apple Pencil 2nd generation',
  },
  {
    id: 14,
    deviceKey: 'test:key',
    name: 'Zebra ET8X',
  },
];

export const fakeGraphicsTablets: GraphicsTablet[] = [
  {
    id: 15,
    deviceKey: 'test:key',
    name: 'Wacom Cintiq 16',
    settings: {
      tabletButtonRemappings: [
        {
          name: 'Back Button',
          button: {
            vkey: Vkey.kNum0,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kCycleBackwardMru,
          },
        },
        {
          name: 'Forward Button',
          button: {
            vkey: Vkey.kNum1,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kCycleForwardMru,
          },
        },
      ],
      penButtonRemappings: [
        {
          name: 'Undo',
          button: {
            vkey: Vkey.kNum2,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
              keyDisplay: 'z',
            },
          },
        },
        {
          name: 'Redo',
          button: {
            vkey: Vkey.kNum3,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 6,
              keyDisplay: 'z',
            },
          },
        },
      ],
    },
    graphicsTabletButtonConfig: GraphicsTabletButtonConfig.kNoConfig,
    batteryInfo: null,
    appInfo: null,
    customizationRestriction: CustomizationRestriction.kAllowCustomizations,
  },
  {
    id: 16,
    deviceKey: 'test:key',
    name: 'XPPen Artist13.3 Pro',
    settings: {
      tabletButtonRemappings: [
        {
          name: 'Brightness Up',
          button: {
            vkey: Vkey.kNum0,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kBrightnessUp,
          },
        },
        {
          name: 'Brightness down',
          button: {
            vkey: Vkey.kNum1,
          },
          remappingAction: {
            acceleratorAction: AcceleratorAction.kBrightnessDown,
          },
        },
      ],
      penButtonRemappings: [
        {
          name: 'Copy',
          button: {
            vkey: Vkey.kNum2,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyC,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
              keyDisplay: 'c',
            },
          },
        },
        {
          name: 'Paste',
          button: {
            vkey: Vkey.kNum3,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyV,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
              keyDisplay: 'v',
            },
          },
        },
      ],
    },
    graphicsTabletButtonConfig:
        GraphicsTabletButtonConfig.kWacomStandardFourButtons,
    batteryInfo: null,
    appInfo: null,
    customizationRestriction: CustomizationRestriction.kAllowCustomizations,
  },
];

export const fakeGraphicsTablets2: GraphicsTablet[] = [
  {
    id: 15,
    deviceKey: 'test:key',
    name: 'Test device without tablet buttons',
    settings: {
      tabletButtonRemappings: [],
      penButtonRemappings: [
        {
          name: 'Undo',
          button: {
            vkey: Vkey.kNum2,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 4,
              keyDisplay: 'z',
            },
          },
        },
        {
          name: 'Redo',
          button: {
            vkey: Vkey.kNum3,
          },
          remappingAction: {
            keyEvent: {
              vkey: Vkey.kKeyZ,
              domCode: 0,
              domKey: 0,
              modifiers: 6,
              keyDisplay: 'z',
            },
          },
        },
      ],
    },
    graphicsTabletButtonConfig:
        GraphicsTabletButtonConfig.kWacomStandardFourButtons,
    batteryInfo: null,
    appInfo: null,
    customizationRestriction: CustomizationRestriction.kAllowCustomizations,
  },
];

export const fakeMouseButtonActions: ActionChoice[] = [
  {
    actionType: {
      staticShortcutAction: StaticShortcutAction.kCopy,
    },
    name: 'Copy',
  },
  {
    actionType: {
      staticShortcutAction: StaticShortcutAction.kPaste,
    },
    name: 'Paste',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kCycleForwardMru,
    },
    name: 'Forward',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kLockScreen,
    },
    name: 'Lock screen',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kToggleClipboardHistory,
    },
    name: 'Open clipboard',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kToggleFullscreen,
    },
    name: 'Fullscreen',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kVolumeMute,
    },
    name: 'Mute',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kWindowMinimize,
    },
    name: 'Minimize window',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kBrightnessDown,
    },
    name: 'Brightness down',
  },
];

export const fakeGraphicsTabletButtonActions: ActionChoice[] = [
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kBrightnessDown,
    },
    name: 'Brightness down',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kBrightnessUp,
    },
    name: 'Brightness up',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kCycleBackwardMru,
    },
    name: 'Back',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kCycleForwardMru,
    },
    name: 'Forward',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kMagnifierZoomIn,
    },
    name: 'Zoom in',
  },
  {
    actionType: {
      acceleratorAction: AcceleratorAction.kMagnifierZoomOut,
    },
    name: 'Zoom out',
  },
];
