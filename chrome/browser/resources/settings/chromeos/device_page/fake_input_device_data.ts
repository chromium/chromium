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
    modifierKeys: [],
    settings: {},
  },
  {
    id: 1,
    name: 'AT Translated Set 2 ',
    isExternal: false,
    metaKey: MetaKey.SEARCH,
    modifierKeys: [ModifierKey.BACKSPACE],
    settings: {},
  },
];

export const fakeTouchpads: Touchpad[] = [{
  id: 1,
  name: 'fake-touchpad',
  isExternal: false,
  isHaptic: false,
}];

export const fakeMice: Mouse[] = [{
  id: 2,
  name: 'fake-mouse',
  isExternal: false,
}];

export const fakePointingSticks: PointingStick[] = [{
  id: 3,
  name: 'fake-pointing-stick',
  isExternal: false,
}];
