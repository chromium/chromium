// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-arc-adb-confirmation-dialog' is a component
 * to confirm for enabling or disabling adb sideloading. After the confirmation,
 * reboot will happens.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import '../../settings_shared.css.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';


/** @polymer */
class SettingsCrostiniArcAdbConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-arc-adb-confirmation-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** An attribute that indicates the action for the confirmation */
      action: {
        type: String,
      },
    };
  }

  constructor() {
    super();

    /** @private {!CrostiniBrowserProxy} */
    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  /**
   * @private
   * @return {boolean}
   */
  isEnabling_() {
    return this.action === 'enable';
  }

  /**
   * @private
   * @return {boolean}
   */
  isDisabling_() {
    return this.action === 'disable';
  }

  /** @private */
  onCancelTap_() {
    this.$.dialog.close();
  }

  /** @private */
  onRestartTap_() {
    if (this.isEnabling_()) {
      this.browserProxy_.enableArcAdbSideload();
      recordSettingChange();
    } else if (this.isDisabling_()) {
      this.browserProxy_.disableArcAdbSideload();
      recordSettingChange();
    } else {
      assertNotReached();
    }
  }
}

customElements.define(
    SettingsCrostiniArcAdbConfirmationDialogElement.is,
    SettingsCrostiniArcAdbConfirmationDialogElement);
