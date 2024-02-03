// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-cellular-setup-dialog' embeds the <cellular-setup>
 * that is shared with OOBE in a dialog with OS Settings stylizations.
 */
import 'chrome://resources/ash/common/cellular_setup/cellular_setup.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {CellularSetupDelegate} from 'chrome://resources/ash/common/cellular_setup/cellular_setup_delegate.js';
import {CellularSetupPageName} from 'chrome://resources/ash/common/cellular_setup/cellular_types.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cellular_setup_dialog.html.js';
import {CellularSetupSettingsDelegate} from './cellular_setup_settings_delegate.js';

export interface OsSettingsCellularSetupDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const OsSettingsCellularSetupDialogElementBase = I18nMixin(PolymerElement);

export class OsSettingsCellularSetupDialogElement extends
    OsSettingsCellularSetupDialogElementBase {
  static get is() {
    return 'os-settings-cellular-setup-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Name of cellular dialog page to be selected.
       */
      pageName: String,

      delegate_: Object,

      psimBanner_: {
        type: String,
      },

      dialogHeader_: {
        type: String,
      },
    };
  }

  pageName: CellularSetupPageName;
  private delegate_: CellularSetupDelegate;
  private dialogHeader_: string;
  private psimBanner_: string;

  constructor() {
    super();

    this.delegate_ = new CellularSetupSettingsDelegate();
  }

  override ready(): void {
    super.ready();

    this.addEventListener('exit-cellular-setup', this.onExitCellularSetup_);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.$.dialog.showModal();
  }

  private onExitCellularSetup_(): void {
    this.$.dialog.close();
  }

  private shouldShowPsimBanner_(): boolean {
    return !!this.psimBanner_;
  }

  private getDialogHeader_(): string {
    if (this.dialogHeader_) {
      return this.dialogHeader_;
    }

    return this.i18n('cellularSetupDialogTitle');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsCellularSetupDialogElement.is]:
        OsSettingsCellularSetupDialogElement;
  }
}

customElements.define(
    OsSettingsCellularSetupDialogElement.is,
    OsSettingsCellularSetupDialogElement);
