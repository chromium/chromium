// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './multidevice_setup_shared.css.js';

import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './button_bar.html.js';

/**
 * DOM Element containing (page-dependent) navigation buttons for the
 * MultiDevice Setup WebUI.
 */
Polymer({
  _template: getTemplate(),
  is: 'button-bar',

  properties: {
    /** Whether the forward button should be hidden. */
    forwardButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether the cancel button should be hidden. */
    cancelButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether the backward button should be hidden. */
    backwardButtonHidden: {
      type: Boolean,
      value: true,
    },

    /** Whether a shadow should appear over the button bar. */
    shouldShowShadow: {
      type: Boolean,
      value: false,
    },
  },

  /** @private */
  onForwardButtonClicked_() {
    this.fire('forward-navigation-requested');
  },

  /** @private */
  onCancelButtonClicked_() {
    this.fire('cancel-requested');
  },

  /** @private */
  onBackwardButtonClicked_() {
    this.fire('backward-navigation-requested');
  },
});
