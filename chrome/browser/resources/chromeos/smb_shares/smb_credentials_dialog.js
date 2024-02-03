// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './strings.m.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/ash/common/assert.js';
import {I18nBehavior} from 'chrome://resources/ash/common/i18n_behavior.js';
import {SmbBrowserProxy, SmbBrowserProxyImpl} from 'chrome://resources/ash/common/smb_shares/smb_browser_proxy.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview
 * 'smb-credentials-dialog' is used to update the credentials for a mounted
 * smb share.
 */
Polymer({
  is: 'smb-credentials-dialog',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @private {string} */
    sharePath_: String,

    /** @private {string} */
    username_: String,

    /** @private {string} */
    password_: String,

    /** @private {string} */
    mountId_: String,
  },

  /** @private {?SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = SmbBrowserProxyImpl.getInstance();

    /** @suppress {checkTypes} */
    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  },

  /** @override */
  attached() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    assert(dialogArgs);
    const args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.path);
    assert(args.mid);
    this.sharePath_ = args.path;
    this.mountId_ = args.mid;

    this.$.dialog.showModal();
  },

  /** @private */
  onCancelButtonClick_() {
    chrome.send('dialogClose');
  },

  /** @private */
  onSaveButtonClick_() {
    this.browserProxy_.updateCredentials(
        this.mountId_, this.username_, this.password_);
    chrome.send('dialogClose');
  },
});
