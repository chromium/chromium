// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './multidevice_setup_shared.css.js';
import './ui_page.js';
import '//resources/ash/common/cr.m.js';
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxy, BrowserProxyImpl} from './multidevice_setup_browser_proxy.js';
import {getTemplate} from './setup_succeeded_page.html.js';
import {UiPageContainerBehavior} from './ui_page_container_behavior.js';

Polymer({
  _template: getTemplate(),
  is: 'setup-succeeded-page',

  properties: {
    /** Overridden from UiPageContainerBehavior. */
    forwardButtonTextId: {
      type: String,
      value: 'done',
    },
  },

  behaviors: [UiPageContainerBehavior],

  /** @private {?BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  },

  /** @private */
  openSettings_() {
    this.browserProxy_.openMultiDeviceSettings();
  },

  /** @private */
  onSettingsLinkClicked_() {
    this.openSettings_();
    this.fire('setup-exited');
  },

  /** @private */
  getMessageHtml_() {
    return this.i18nAdvanced('setupSucceededPageMessage', {attrs: ['id']});
  },

  /** @override */
  ready() {
    const linkElement = this.$$('#settings-link');
    linkElement.setAttribute('href', '#');
    linkElement.addEventListener('click', () => this.onSettingsLinkClicked_());
  },
});
