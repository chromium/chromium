// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-pdf-documents' is the polymer element for showing the
 * settings for viewing PDF documents under Site Settings.
 */

import '../controls/settings_toggle_button.js';
import '../settings_shared_css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'settings-pdf-documents',

  _template: html`{__html_template__}`,

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },
  },
});
