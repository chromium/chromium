// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data-entry' handles showing the local storage summary for a site.
 */

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl, LocalDataItem} from './local_data_browser_proxy.js';

Polymer({
  is: 'site-data-entry',

  _template: html`{__html_template__}`,

  behaviors: [
    FocusRowBehavior,
    I18nBehavior,
  ],

  properties: {
    /** @type {!LocalDataItem} */
    model: Object,
  },

  /** @private {LocalDataBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = LocalDataBrowserProxyImpl.getInstance();
  },

  /**
   * Deletes all site data for this site.
   * @param {!Event} e
   * @private
   */
  onRemove_(e) {
    e.stopPropagation();
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.SITE_DATA_REMOVE_SITE);
    this.browserProxy_.removeSite(this.model.site);
  },
});
