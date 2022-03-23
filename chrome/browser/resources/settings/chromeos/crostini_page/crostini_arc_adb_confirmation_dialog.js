// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-arc-adb-confirmation-dialog' is a component
 * to confirm for enabling or disabling adb sideloading. After the confirmation,
 * reboot will happens.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';
import '../../settings_shared_css.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl, CrostiniDiskInfo, CrostiniPortActiveSetting, CrostiniPortProtocol, CrostiniPortSetting, DEFAULT_CROSTINI_CONTAINER, DEFAULT_CROSTINI_VM, MAX_VALID_PORT_NUMBER, MIN_VALID_PORT_NUMBER, PortState} from './crostini_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-crostini-arc-adb-confirmation-dialog',

  properties: {
    /** An attribute that indicates the action for the confirmation */
    action: {
      type: String,
    },
  },

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /**
   * @private
   * @return {boolean}
   */
  isEnabling_() {
    return this.action === 'enable';
  },

  /**
   * @private
   * @return {boolean}
   */
  isDisabling_() {
    return this.action === 'disable';
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  },

  /** @private */
  onRestartTap_() {
    if (this.isEnabling_()) {
      CrostiniBrowserProxyImpl.getInstance().enableArcAdbSideload();
      recordSettingChange();
    } else if (this.isDisabling_()) {
      CrostiniBrowserProxyImpl.getInstance().disableArcAdbSideload();
      recordSettingChange();
    } else {
      assertNotReached();
    }
  },
});
