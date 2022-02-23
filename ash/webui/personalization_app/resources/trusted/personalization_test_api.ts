// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {PersonalizationStore} from './personalization_store.js';
import {setFullscreenEnabledAction} from './wallpaper/wallpaper_actions.js';

/**
 * @fileoverview provides useful functions for e2e browsertests.
 */

function enterFullscreen() {
  const store = PersonalizationStore.getInstance();
  assert(!!store);
  store.dispatch(setFullscreenEnabledAction(true));
}

declare global {
  interface Window {
    personalizationTestApi: {enterFullscreen: () => void;}
  }
}

window.personalizationTestApi = {enterFullscreen};
