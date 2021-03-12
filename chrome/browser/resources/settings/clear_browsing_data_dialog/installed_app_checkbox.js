// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * `installed-app-checkbox` is a checkbox that displays an installed app.
 * An installed app could be a domain with data that the user might want
 * to protect from being deleted.
 */
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InstalledApp} from './clear_browsing_data_browser_proxy.js';

Polymer({
  is: 'installed-app-checkbox',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {InstalledApp} */
    installed_app: Object,
    disabled: {
      type: Boolean,
      value: false,
    },
  },
});
