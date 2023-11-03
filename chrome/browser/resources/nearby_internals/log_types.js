// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-location/iron-location.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './log_types.html.js';
import {FeatureValues} from './types.js';

Polymer({
  is: 'log-types',

  _template: getTemplate(),


  properties: {
    /** @private {!Array<FeatureValues>} */
    currentLogTypes: {
      type: FeatureValues,
      value: [
        FeatureValues.NEARBY_SHARE,
        FeatureValues.NEARBY_CONNECTIONS,
        FeatureValues.NEARBY_PRESENCE,
        FeatureValues.FAST_PAIR,
      ],
    },
  },

  nearbyPresenceCheckboxClicked() {
    if (this.$.nearbyPresenceCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NEARBY_PRESENCE)) {
      this.currentLogTypes.push(FeatureValues.NEARBY_PRESENCE);
    }
    if (!this.$.nearbyPresenceCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NEARBY_PRESENCE)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NEARBY_PRESENCE), 1);
    }
  },

  nearbyShareCheckboxClicked() {
    if (this.$.nearbyShareCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NEARBY_SHARE)) {
      this.currentLogTypes.push(FeatureValues.NEARBY_SHARE);
    }
    if (!this.$.nearbyShareCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NEARBY_SHARE)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NEARBY_SHARE), 1);
    }
  },

  nearbyConnectionsCheckboxClicked() {
    if (this.$.nearbyConnectionsCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NEARBY_CONNECTIONS)) {
      this.currentLogTypes.push(FeatureValues.NEARBY_CONNECTIONS);
    }
    if (!this.$.nearbyConnectionsCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NEARBY_CONNECTIONS)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NEARBY_CONNECTIONS),
          1);
    }
  },

  fastPairCheckboxClicked() {
    if (this.$.fastPairCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.FAST_PAIR)) {
      this.currentLogTypes.push(FeatureValues.FAST_PAIR);
    }
    if (!this.$.fastPairCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.FAST_PAIR)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.FAST_PAIR), 1);
    }
  },
});
