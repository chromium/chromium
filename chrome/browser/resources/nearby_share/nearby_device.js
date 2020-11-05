// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'nearby-device' component shows details of a remote device.
 */

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './nearby_device_icon.js';
import './nearby_share_target_types.mojom-lite.js';
import './nearby_share.mojom-lite.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'nearby-device',

  _template: html`{__html_template__}`,

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
