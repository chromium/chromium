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
        [ModifierKey.META, ModifierKey.VOID],
        [ModifierKey.CAPS_LOCK, ModifierKey.ASSISTANT],
      ]),
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
    },
  },
];

export const fakeTouchpads: Touchpad[] = [
  {
    id: 2,
    name: 'Default Touchpad',
    isExternal: false,
    isHaptic: true,
  },
  {
    id: 3,
    name: 'Logitech T650',
    isExternal: true,
    isHaptic: false,
  },
];

export const fakeMice: Mouse[] = [
  {
    id: 4,
    name: 'Razer Basilisk V3',
    isExternal: true,
  },
  {
    id: 5,
    name: 'MX Anywhere 2S',
    isExternal: false,
  },
];

export const fakePointingSticks: PointingStick[] = [
  {
    id: 6,
    name: 'Default Pointing Stick',
    isExternal: false,
  },
  {
    id: 7,
    name: 'Lexmark-Unicomp FSR',
    isExternal: true,
  },
];
