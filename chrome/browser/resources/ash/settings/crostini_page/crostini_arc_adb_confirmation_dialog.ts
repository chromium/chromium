// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-arc-adb-confirmation-dialog' is a component
 * to confirm for enabling or disabling adb sideloading. After the confirmation,
 * reboot will happens.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {getTemplate} from './crostini_arc_adb_confirmation_dialog.html.js';
import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';

interface SettingsCrostiniArcAdbConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsCrostiniArcAdbConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-arc-adb-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** An attribute that indicates the action for the confirmation */
      action: {
        type: String,
      },
    };
  }

  action: string;
  private browserProxy_: CrostiniBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private isEnabling_(): boolean {
    return this.action === 'enable';
  }

  private isDisabling_(): boolean {
    return this.action === 'disable';
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onRestartClick_(): void {
    if (this.isEnabling_()) {
      this.browserProxy_.enableArcAdbSideload();
      recordSettingChange(Setting.kCrostiniAdbDebugging, {boolValue: true});
    } else if (this.isDisabling_()) {
      this.browserProxy_.disableArcAdbSideload();
      recordSettingChange(Setting.kCrostiniAdbDebugging, {boolValue: false});
    } else {
      assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-arc-adb-confirmation-dialog':
        SettingsCrostiniArcAdbConfirmationDialogElement;
  }
}

customElements.define(
    SettingsCrostiniArcAdbConfirmationDialogElement.is,
    SettingsCrostiniArcAdbConfirmationDialogElement);
