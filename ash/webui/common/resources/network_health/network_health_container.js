// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/cr_elements/icons.html.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_health_container.html.js';

/**
 * @fileoverview Polymer element for a container used in displaying network
 * health info.
 */

Polymer({
  _template: getTemplate(),
  is: 'network-health-container',

  properties: {
    /**
     * Boolean flag if the container is expanded.
     */
    expanded: {
      type: Boolean,
      value: false,
    },

    /**
     * Container label.
     */
    label: {
      type: String,
      value: '',
    },
  },

  /**
   * Returns the correct arrow icon depending on if the container is expanded.
   * @param {boolean} expanded
   */
  getArrowIcon_(expanded) {
    return expanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * Helper function to fire the toggle event when clicked.
   * @private
   */
  onClick_() {
    this.fire('toggle-expanded');
  },
});
