// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import './shared_style.css.js';
import './browser_tabs_metadata_form.js';

import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './browser_tabs_model_form.html.js';
import {MultidevicePhoneHubBrowserProxy} from './multidevice_phonehub_browser_proxy.js';
import {BrowserTabsMetadataModel, BrowserTabsModel} from './types.js';

Polymer({
  is: 'browser-tabs-model-form',

  _template: getTemplate(),

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

    /** @type{number} */
    nValidTabs_: {
      type: Number,
      computed:
          'computeNValidTabs_(isTabSyncEnabled_, browserTabOneMetadata_, ' +
          'browserTabTwoMetadata_)',
    },
  },

  /** @private{?MultidevicePhoneHubBrowserProxy}*/
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = MultidevicePhoneHubBrowserProxy.getInstance();
  },

  /**
   * @return {Array<!BrowserTabsMetadataModel>}
   * @private
   */
  getAllBrowserTabMetadatas_() {
    return [this.browserTabOneMetadata_, this.browserTabTwoMetadata_];
  },

  /**
   * @return {number}
   * @private
   */
  computeNValidTabs_() {
    if (!this.isTabSyncEnabled_) {
      return 0;
    }

    return this.getAllBrowserTabMetadatas_().reduce((acc, metadata) => {
      return acc + (!!metadata && metadata.isValid);
    }, 0);
  },

  /** @private */
  setFakeBrowserTabModel_() {
    if (!this.isTabSyncEnabled_) {
      const syncDisabledBrowserTabsModel = {
        isTabSyncEnabled: false,
        browserTabOneMetadata: null,
        browserTabTwoMetadata: null,
      };
      this.browserProxy_.setBrowserTabs(syncDisabledBrowserTabsModel);
      return;
    }

    const browserTabsModel = {
      isTabSyncEnabled: this.isTabSyncEnabled_,
      browserTabOneMetadata: this.browserTabOneMetadata_,
      browserTabTwoMetadata: this.browserTabTwoMetadata_,
    };
    this.browserProxy_.setBrowserTabs(browserTabsModel);
  },
});
