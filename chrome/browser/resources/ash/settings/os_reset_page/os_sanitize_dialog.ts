// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-sanitize-dialog' is a dialog shown to request confirmation
 * from the user for reverting to safe settings (aka sanitize).
 */
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {OsResetBrowserProxy, OsResetBrowserProxyImpl} from './os_reset_browser_proxy.js';
import {getTemplate} from './os_sanitize_dialog.html.js';

export interface OsSettingsSanitizeDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class OsSettingsSanitizeDialogElement extends PolymerElement {
  static get is() {
    return 'os-settings-sanitize-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {};
  }

  private osResetBrowserProxy_: OsResetBrowserProxy;

  constructor() {
    super();
    this.osResetBrowserProxy_ = OsResetBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.osResetBrowserProxy_.onShowSanitizeDialog();
    this.$.dialog.showModal();
  }

  private onAbortSanitize(): void {
    this.$.dialog.close();
  }

  private onPerformSanitize(): void {
    this.osResetBrowserProxy_.performSanitizeSettings();
    recordSettingChange(Setting.kSanitizeCrosSettings);
    if (this.$.dialog.open) {
      this.$.dialog.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsSanitizeDialogElement.is]: OsSettingsSanitizeDialogElement;
  }
}

customElements.define(
    OsSettingsSanitizeDialogElement.is, OsSettingsSanitizeDialogElement);
