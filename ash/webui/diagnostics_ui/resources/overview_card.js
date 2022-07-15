// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SystemDataProviderInterface, SystemInfo} from './diagnostics_types.js';
import {getSystemDataProvider} from './mojo_interface_provider.js';


/**
 * @fileoverview
 * 'overview-card' shows an overview of system information such
 * as CPU type, version, board name, and memory.
 */
Polymer({
  is: 'overview-card',

  _template: html`{__html_template__}`,

  /**
   * @private {?SystemDataProviderInterface}
   */
  systemDataProvider_: null,

  properties: {
    /** @private {!SystemInfo} */
    systemInfo_: {
      type: Object,
    },

    /** @private {string} */
    deviceInfo_: {
      type: String,
      value: '',
      computed: 'getDeviceInfo_(systemInfo_.versionInfo.fullVersionString,' +
          'systemInfo_.boardName)',
    },
  },

  /** @override */
  created() {
    this.systemDataProvider_ = getSystemDataProvider();
    this.fetchSystemInfo_();
  },

  /** @private */
  fetchSystemInfo_() {
    this.systemDataProvider_.getSystemInfo().then((result) => {
      this.onSystemInfoReceived_(result.systemInfo);
    });
  },

  /**
   * @param {!SystemInfo} systemInfo
   * @private
   */
  onSystemInfoReceived_(systemInfo) {
    this.systemInfo_ = systemInfo;
  },

  /** @private */
  getDeviceInfo_() {
    const marketingNameValid = !this.shouldHideMarketingName_();
    const boardName = this.systemInfo_.boardName;
    const version = this.systemInfo_.versionInfo.fullVersionString;

    if (!boardName && !marketingNameValid) {
      const versionInfo =
          loadTimeData.getStringF('versionInfo', version);
      // Capitalize "v" in "version" if board and marketing name are missing.
      return versionInfo[0].toUpperCase() + versionInfo.slice(1);
    }

    const deviceInfo = this.systemInfo_.boardName ?
        loadTimeData.getStringF(
            'boardAndVersionInfo', this.systemInfo_.boardName, version) :
        loadTimeData.getStringF('versionInfo', version);
    return marketingNameValid ? `(${deviceInfo})` : deviceInfo;
  },

  /**
   * @protected
   * @return {boolean}
   */
  shouldHideMarketingName_() {
    return this.systemInfo_.marketingName === 'TBD' ||
        this.systemInfo_.marketingName === '';
  },
});
