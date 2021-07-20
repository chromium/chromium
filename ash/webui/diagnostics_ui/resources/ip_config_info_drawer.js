// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_fonts_css.js';
import './diagnostics_shared_css.js';

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'ip-config-info-drawer' displays standard IP related configuration data in a
 * collapsible drawer.
 */
Polymer({
  is: 'ip-config-info-drawer',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @protected
     * @type {boolean}
     */
    expanded_: {
      type: Boolean,
      value: false,
    },
  },
});
