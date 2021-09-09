// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './data_point.js';
import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LockType, Network, RoamingState} from './diagnostics_types.js';
import {getLockType} from './diagnostics_utils.js';

/**
 * @fileoverview
 * 'cellular-info' is responsible for displaying data points related
 * to a Cellular network.
 */
Polymer({
  is: 'cellular-info',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!Network} */
    network: {
      type: Object,
    },
  },

  /**
   * @protected
   * @return {string}
   */
  computeRoamingText_() {
    if (!this.network.typeProperties) {
      return '';
    }

    if (!this.network.typeProperties.cellular.roaming) {
      return this.i18n('networkRoamingOff');
    }

    const state = this.network.typeProperties.cellular.roamingState;
    switch (state) {
      case RoamingState.kNone:
        return '';
      case RoamingState.kRoaming:
        return this.i18n('networkRoamingStateRoaming');
      case RoamingState.kHome:
        return this.i18n('networkRoamingStateHome');
    }

    assertNotReached();
    return '';
  },

  /**
   * @protected
   * @return {string}
   */
  computeSimLockedText_() {
    if (!this.network.typeProperties) {
      return '';
    }

    const {simLocked, lockType} = this.network.typeProperties.cellular;
    return (simLocked && lockType !== LockType.kNone) ?
        this.i18n('networkSimLockedText', getLockType(lockType)) :
        this.i18n('networkSimUnlockedText');
  },
});
