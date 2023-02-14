// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Keyboard, MetaKey, ModifierKey, Mouse, PointingStick, Touchpad} from './input_device_settings_types.js';

export const fakeKeyboards: Keyboard[] = [
  {
    id: 0,
    name: 'ERGO K860',
    isExternal: true,
    metaKey: MetaKey.COMMAND,
    modifierKeys: [
      ModifierKey.ALT,
      ModifierKey.BACKSPACE,
      ModifierKey.CAPS_LOCK,
      ModifierKey.CONTROL,
      ModifierKey.ESC,
      ModifierKey.META,
    ],
    settings: {
      modifierRemappings: new Map<ModifierKey, ModifierKey>([
        [ModifierKey.CONTROL, ModifierKey.CAPS_LOCK],
        [ModifierKey.CAPS_LOCK, ModifierKey.ASSISTANT],
      ]),
      topRowAreFKeys: false,
      suppressMetaFKeyRewrites: false,
      autoRepeatEnabled: false,
      autoRepeatDelay: 2000,
      autoRepeatInterval: 2000,
    },
  },
  {
    id: 1,
    name: 'AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.SEARCH,
    modifierKeys: [
      ModifierKey.ALT,
      ModifierKey.ASSISTANT,
      ModifierKey.BACKSPACE,
      ModifierKey.CONTROL,
      ModifierKey.ESC,
      ModifierKey.META,
    ],
    settings: {
      modifierRemappings: new Map<ModifierKey, ModifierKey>(),
      topRowAreFKeys: true,
      suppressMetaFKeyRewrites: true,
      autoRepeatEnabled: true,
      autoRepeatDelay: 150,
      autoRepeatInterval: 20,
    },
  },
  {
    id: 8,
    name: 'Logitech G713 Aurora',
    isExternal: true,
    metaKey: MetaKey.LAUNCHER,
    modifierKeys: [
      ModifierKey.ALT,
      ModifierKey.BACKSPACE,
      ModifierKey.CAPS_LOCK,
      ModifierKey.CONTROL,
      ModifierKey.ESC,
      ModifierKey.META,
    ],
    settings: {
      modifierRemappings: new Map<ModifierKey, ModifierKey>([
        [ModifierKey.ALT, ModifierKey.ASSISTANT],
      ]),
      topRowAreFKeys: true,
      suppressMetaFKeyRewrites: false,
      autoRepeatEnabled: true,
      autoRepeatDelay: 500,
      autoRepeatInterval: 100,
    },
  },
];

export const fakeTouchpads: Touchpad[] = [
  {
    id: 2,
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
    },
  },
  {
    id: 3,
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
    },
  },
];

export const fakeMice: Mouse[] = [
  {
    id: 4,
    name: 'Razer Basilisk V3',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      reverseScrolling: true,
      accelerationEnabled: true,
      scrollSensitivity: 5,
      scrollAcceleration: true,
    },
  },
  {
    id: 5,
    name: 'MX Anywhere 2S',
    isExternal: false,
    settings: {
      swapRight: false,
      sensitivity: 1,
      reverseScrolling: false,
      accelerationEnabled: false,
      scrollSensitivity: 1,
      scrollAcceleration: false,
    },
  },
];

export const fakePointingSticks: PointingStick[] = [
  {
    id: 6,
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
    name: 'Lexmark-Unicomp FSR',
    isExternal: true,
    settings: {
      swapRight: true,
      sensitivity: 5,
      accelerationEnabled: true,
    },
  },
];
