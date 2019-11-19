// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icons_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-selector/iron-selector.js';
import 'chrome://resources/polymer/v3_0/paper-ripple/paper-ripple.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from './navigation_helper.js';

Polymer({
  is: 'extensions-sidebar',

  _template: html`{__html_template__}`,

  properties: {
    isSupervised: Boolean,
  },

  hostAttributes: {
    role: 'navigation',
  },

  /** @override */
  attached: function() {
    this.$.sectionMenu.select(
        navigation.getCurrentPage().page == Page.SHORTCUTS ? 1 : 0);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onLinkTap_: function(e) {
    e.preventDefault();
    navigation.navigateTo({page: e.target.dataset.path});
    this.fire('close-drawer');
  },

  /** @private */
  onMoreExtensionsTap_: function() {
    assert(!this.isSupervised);
    chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
  },
});
