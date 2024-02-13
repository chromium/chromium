// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-import-confirmation-dialog' is a component
 * warning the user that importing a container overrides the existing container.
 * By clicking 'Continue', the user agrees to start the import.
 */
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '../settings_shared.css.js';

import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GuestId} from '../guest_os/guest_os_browser_proxy.js';
import {recordSettingChange} from '../metrics_recorder.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';

import {CrostiniBrowserProxy, CrostiniBrowserProxyImpl} from './crostini_browser_proxy.js';
import {getTemplate} from './crostini_import_confirmation_dialog.html.js';

interface SettingsCrostiniImportConfirmationDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

class SettingsCrostiniImportConfirmationDialogElement extends PolymerElement {
  static get is() {
    return 'settings-crostini-import-confirmation-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      importContainerId: {
        type: Object,
      },
    };
  }

  importContainerId: GuestId;
  private browserProxy_: CrostiniBrowserProxy;

  constructor() {
    super();

    this.browserProxy_ = CrostiniBrowserProxyImpl.getInstance();
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onCancelClick_(): void {
    this.$.dialog.close();
  }

  private onContinueClick_(): void {
    this.browserProxy_.importCrostiniContainer(this.importContainerId);
    recordSettingChange(Setting.kRestoreLinuxAppsAndFiles);
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-crostini-import-confirmation-dialog':
        SettingsCrostiniImportConfirmationDialogElement;
  }
}

customElements.define(
    SettingsCrostiniImportConfirmationDialogElement.is,
    SettingsCrostiniImportConfirmationDialogElement);
