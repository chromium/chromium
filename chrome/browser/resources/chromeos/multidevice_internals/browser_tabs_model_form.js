// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import './shared_style.js';
import './browser_tabs_metadata_form.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {BrowserTabsMetadataModel, BrowserTabsModel} from './types.js';

Polymer({
  is: 'browser-tabs-model-form',

  _template: html`{__html_template__}`,

  properties: {
    /** @private */
    isTabSyncEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private{BrowserTabsMetadataModel} */
    browserTabOneMetadata_: {
      type: Object,
    },

    /** @private{BrowserTabsMetadataModel} */
    browserTabTwoMetadata_: {
      type: Object,
    },
  },

  /** @private{?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /** @private */
  setFakeBrowserTabModel_() {
    const browserTabsModel = {
      isTabSyncEnabled: this.isTabSyncEnabled_,
      browserTabOneMetadata:
          this.isTabSyncEnabled_ ? this.browserTabOneMetadata_ : null,
      browserTabTwoMetadata:
          this.isTabSyncEnabled_ ? this.browserTabTwoMetadata_ : null,
    };
    this.browserProxy_.setBrowserTabs(browserTabsModel);
  },
});
