// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Page in eSIM Setup flow that requests user consent to scan for profiles.
 */

import 'chrome://resources/ash/common/cellular_setup/cellular_setup_icons.html.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/cr_elements/cr_shared_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './base_page.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './profile_discovery_consent_page.html.js';

Polymer({
  _template: getTemplate(),
  is: 'profile-discovery-consent-page',

  behaviors: [I18nBehavior],

  properties: {
    shouldSkipDiscovery: {
      type: Boolean,
      notify: true,
    },
  },

  shouldSkipDiscoveryClicked_(e) {
    // A place holder href with the value "#" is used to have a compliant link.
    // This prevents the browser from navigating the window to "#"
    e.detail.event.preventDefault();
    e.stopPropagation();
    this.shouldSkipDiscovery = true;
    this.fire('forward-navigation-requested');
  },
});
