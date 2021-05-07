// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-powerwash-dialog' is a dialog shown to request confirmation
 * from the user for a device reset (aka powerwash).
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '//resources/polymer/v3_0/iron-list/iron-list.js';
import '../localized_link/localized_link.m.js';
import '../../settings_shared_css.js';
import './os_powerwash_dialog_esim_item.js';

import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LifetimeBrowserProxy, LifetimeBrowserProxyImpl} from '../../lifetime_browser_proxy.js';
import {recordClick, recordNavigation, recordPageBlur, recordPageFocus, recordSearch, recordSettingChange, setUserActionRecorderForTesting} from '../metrics_recorder.m.js';

import {OsResetBrowserProxy, OsResetBrowserProxyImpl} from './os_reset_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-powerwash-dialog',

  properties: {
    requestTpmFirmwareUpdate: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {!Array<!chromeos.cellularSetup.mojom.ESimProfileRemote>}
     * @private
     */
    installedESimProfiles: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @private */
    shouldShowESimWarning_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowESimWarning_(installedESimProfiles)',
    },
  },

  /** @override */
  attached() {
    OsResetBrowserProxyImpl.getInstance().onPowerwashDialogShow();
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onRestartTap_() {
    recordSettingChange();
    LifetimeBrowserProxyImpl.getInstance().factoryReset(
        this.requestTpmFirmwareUpdate);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowESimWarning_() {
    return !!this.installedESimProfiles.length;
  },
});
