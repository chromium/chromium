// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-update-warning-dialog' is a component warning the
 * user about update over mobile data. By clicking 'Continue', the user
 * agrees to download update using mobile data.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo, BrowserChannel, browserChannelToI18nId, ChannelInfo, isTargetChannelMoreStable, RegulatoryInfo, TPMFirmwareUpdateStatusChangedEvent, UpdateStatus, UpdateStatusChangedEvent, VersionInfo} from './about_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-update-warning-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!AboutPageUpdateInfo|undefined} */
    updateInfo: {
      type: Object,
      observer: 'updateInfoChanged_',
    },
  },

  /** @private {?AboutPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = AboutPageBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onContinueTap_() {
    if (!this.updateInfo || !this.updateInfo.version || !this.updateInfo.size){
      console.log('ERROR: requestUpdateOverCellular arguments are undefined');
      return;
    }
    this.browserProxy_.requestUpdateOverCellular(
        /** @type {!string} */ (this.updateInfo.version),
        /** @type {!string} */ (this.updateInfo.size));
    this.$.dialog.close();
  },

  /** @private */
  updateInfoChanged_() {
    this.$$('#update-warning-message').innerHTML = this.i18n(
        'aboutUpdateWarningMessage',
        // Convert bytes to megabytes
        Math.floor(Number(this.updateInfo.size) / (1024 * 1024)));
  },
});
