// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-update-warning-dialog' is a component warning the
 * user about update over mobile data. By clicking 'Continue', the user
 * agrees to download update using mobile data.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo} from './about_page_browser_proxy.js';
import {getTemplate} from './update_warning_dialog.html.js';

interface SettingsUpdateWarningDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SettingsUpdateWarningDialogElementBase = I18nMixin(PolymerElement);

class SettingsUpdateWarningDialogElement extends
    SettingsUpdateWarningDialogElementBase {
  static get is() {
    return 'settings-update-warning-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      updateInfo: {
        type: Object,
        observer: 'onUpdateInfoChanged_',
      },

      warningMessage_: {
        type: String,
        value: '',
      },
    };
  }

  updateInfo?: AboutPageUpdateInfo;

  private browserProxy_: AboutPageBrowserProxy;
  private warningMessage_: string;

  constructor() {
    super();

    this.browserProxy_ = AboutPageBrowserProxyImpl.getInstance();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onContinueClick_(): void {
    if (!this.updateInfo || !this.updateInfo.version || !this.updateInfo.size) {
      console.warn('ERROR: requestUpdateOverCellular arguments are undefined');
      return;
    }
    this.browserProxy_.requestUpdateOverCellular(
        this.updateInfo.version, this.updateInfo.size);
    this.$.dialog.close();
  }

  private onUpdateInfoChanged_(): void {
    if (!this.updateInfo || this.updateInfo.size === undefined) {
      console.warn('ERROR: Update size is undefined');
      return;
    }

    this.warningMessage_ = this.i18n(
        'aboutUpdateWarningMessage',
        // Convert bytes to megabytes
        Math.floor(Number(this.updateInfo.size) / (1024 * 1024)));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-update-warning-dialog': SettingsUpdateWarningDialogElement;
  }
}

customElements.define(
    SettingsUpdateWarningDialogElement.is, SettingsUpdateWarningDialogElement);
