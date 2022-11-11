// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of cellular
 * APNs
 */

import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './network_shared.css.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/ash/common/network/apn_list_item.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './apn_list.html.js';

Polymer({
  _template: getTemplate(),
  is: 'apn-list',

  behaviors: [I18nBehavior],

  properties: {
    // TODO(b/162365553): Populate with real data and remove dummy values.
    apns: {
      type: Array,
      value: [{name: 'apn1'}, {name: 'apn2'}, {name: 'apn3'}],
    },
  },

  /**
   * Redirects to "Lean more about APN" page.
   * TODO(b/162365553): Implement.
   * @private
   */
  onLearnMoreClicked_() {},
});