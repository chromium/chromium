// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

Polymer({
  is: 'nearby-device',

  properties: {
    /**
     * Expected to start as null, then change to a valid object before this
     * component is shown.
     * @type {?nearbyShare.mojom.ShareTarget}
     */
    shareTarget: {
      type: Object,
      value: null,
    },

    /**
     * Whether this share target is selected.
     * @type {boolean}
     */
    isSelected: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },
});
