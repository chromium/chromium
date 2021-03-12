// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '../i18n_setup.js';
import '../settings_shared_css.js';

import {assert} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionControlBrowserProxyImpl} from '../extension_control_browser_proxy.js';

Polymer({
  is: 'extension-controlled-indicator',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    extensionCanBeDisabled: Boolean,
    extensionId: String,
    extensionName: String,
  },

  /**
   * @param {string} extensionId
   * @param {string} extensionName
   * @return {string}
   * @private
   */
  getLabel_(extensionId, extensionName) {
    if (this.extensionId === undefined || this.extensionName === undefined) {
      return '';
    }

    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    return this.i18nAdvanced('controlledByExtension', {
      substitutions:
          ['<a href="' + manageUrl + '" target="_blank">' + this.extensionName +
           '</a>'],
    });
  },

  /** @private */
  onDisableTap_() {
    assert(this.extensionCanBeDisabled);
    ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        assert(this.extensionId));
    this.fire('extension-disable');
  },
});
