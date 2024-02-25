// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * An element that encapsulates the structure common to all pages in the WebUI.
 */
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.html.js';
import './multidevice_setup_shared.css.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './ui_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'ui-page',

  properties: {
    /**
     * Main heading for the page.
     *
     * @type {string}
     */
    headerText: String,

    /**
     * Name of icon within icon set.
     *
     * @type {string}
     */
    iconName: String,
  },

  /**
   * @return {string}
   * @private
   */
  computeIconIdentifier_() {
    return 'multidevice-setup-icons-32:' + this.iconName;
  },
});
