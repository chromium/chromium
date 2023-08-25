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
        FeatureValues.NearbyShare,
        FeatureValues.NearbyConnections,
        FeatureValues.NearbyPresence,
        FeatureValues.FastPair,
      ],
    },
  },

  nearbyPresenceCheckboxClicked() {
    if (this.$.nearbyPresenceCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NearbyPresence)) {
      this.currentLogTypes.push(FeatureValues.NearbyPresence);
    }
    if (!this.$.nearbyPresenceCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NearbyPresence)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NearbyPresence), 1);
    }
  },

  nearbyShareCheckboxClicked() {
    if (this.$.nearbyShareCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NearbyShare)) {
      this.currentLogTypes.push(FeatureValues.NearbyShare);
    }
    if (!this.$.nearbyShareCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NearbyShare)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NearbyShare), 1);
    }
  },

  nearbyConnectionsCheckboxClicked() {
    if (this.$.nearbyConnectionsCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.NearbyConnections)) {
      this.currentLogTypes.push(FeatureValues.NearbyConnections);
    }
    if (!this.$.nearbyConnectionsCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.NearbyConnections)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.NearbyConnections), 1);
    }
  },

  fastPairCheckboxClicked() {
    if (this.$.fastPairCheckbox.checked &&
        !this.currentLogTypes.includes(FeatureValues.FastPair)) {
      this.currentLogTypes.push(FeatureValues.FastPair);
    }
    if (!this.$.fastPairCheckbox.checked &&
        this.currentLogTypes.includes(FeatureValues.FastPair)) {
      this.currentLogTypes.splice(
          this.currentLogTypes.lastIndexOf(FeatureValues.FastPair), 1);
    }
  },
});
