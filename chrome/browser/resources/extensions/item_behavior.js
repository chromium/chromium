// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.m.js';

/** @polymerBehavior */
export const ItemBehavior = {
  /**
   * @param {chrome.developerPrivate.ExtensionType} type
   * @param {string} appLabel
   * @param {string} extensionLabel
   * @return {string} The app or extension label depending on |type|.
   */
  appOrExtension: function(type, appLabel, extensionLabel) {
    const ExtensionType = chrome.developerPrivate.ExtensionType;
    switch (type) {
      case ExtensionType.HOSTED_APP:
      case ExtensionType.LEGACY_PACKAGED_APP:
      case ExtensionType.PLATFORM_APP:
        return appLabel;
      case ExtensionType.EXTENSION:
      case ExtensionType.SHARED_MODULE:
        return extensionLabel;
    }
    assertNotReached('Item type is not App or Extension.');
  },
};
