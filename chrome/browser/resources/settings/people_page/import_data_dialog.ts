// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-import-data-dialog' is a component for importing
 * bookmarks and other data from other sources.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../controls/settings_checkbox.js';
import '../controls/settings_toggle_button.js';
import '../icons.html.js';
import '../settings_vars.css.js';
import '../i18n_setup.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SettingsCheckboxElement} from '../controls/settings_checkbox.js';

import type {BrowserProfile, ImportDataBrowserProxy} from './import_data_browser_proxy.js';
import {ImportDataBrowserProxyImpl, ImportDataStatus} from './import_data_browser_proxy.js';
import {getTemplate} from './import_data_dialog.html.js';

export interface SettingsImportDataDialogElement {
  $: {
    browserSelect: HTMLSelectElement,
    cancel: CrButtonElement,
    dialog: CrDialogElement,
    done: CrButtonElement,
    import: CrButtonElement,
    successIcon: HTMLElement,
  };
}

const SettingsImportDataDialogElementBase =
    WebUiListenerMixin(I18nMixin(PrefsMixin(PolymerElement)));

export class SettingsImportDataDialogElement extends
    SettingsImportDataDialogElementBase {
  static get is() {
    return 'settings-import-data-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      browserProfiles_: Array,

      selected_: {
        type: Object,
        observer: 'updateImportDataTypesSelected_',
      },

      /**
       * Whether none of the import data categories is selected.
       */
      noImportDataTypeSelected_: {
        type: Boolean,
        value: false,
      },

      importStatus_: {
        type: String,
        value: ImportDataStatus.INITIAL,
      },

      /**
       * Mirroring the enum so that it can be used from HTML bindings.
       */
      importStatusEnum_: {
        type: Object,
        value: ImportDataStatus,
      },

    };
  }

  private browserProfiles_: BrowserProfile[];
  private selected_: BrowserProfile;
  private noImportDataTypeSelected_: boolean;
  private importStatus_: ImportDataStatus;
  private browserProxy_: ImportDataBrowserProxy =
      ImportDataBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();
    this.addEventListener(
        'settings-boolean-control-change', this.updateImportDataTypesSelected_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.browserProxy_.initializeImportDialog().then(data => {
      this.browserProfiles_ = data;
      this.selected_ = this.browserProfiles_[0];

      // Show the dialog only after the browser profiles data is populated
      // to avoid UI flicker.
      this.$.dialog.showModal();
    });

    this.addWebUiListener(
        'import-data-status-changed', (importStatus: ImportDataStatus) => {
          this.importStatus_ = importStatus;
          if (this.hasImportStatus_(ImportDataStatus.FAILED)) {
            this.closeDialog_();
          }
        });
  }

  private getProfileDisplayName_(name: string, profileName: string): string {
    return profileName ? `${name} - ${profileName}` : name;
  }

  private updateImportDataTypesSelected_() {
    const checkboxes =
        this.shadowRoot!.querySelectorAll<SettingsCheckboxElement>(
            'settings-checkbox[checked]:not([hidden])');
    this.noImportDataTypeSelected_ = checkboxes.length === 0;
  }

  /**
   * @return Whether |status| is the current status.
   */
  private hasImportStatus_(status: ImportDataStatus): boolean {
    return this.importStatus_ === status;
  }

  private isImportFromFileSelected_() {
    // The last entry in |browserProfiles_| always refers to dummy profile for
    // importing from a bookmarks file.
    return this.selected_.index === this.browserProfiles_.length - 1;
  }

  private getActionButtonText_(): string {
    return this.i18n(
        this.isImportFromFileSelected_() ? 'importChooseFile' : 'importCommit');
  }

  private onBrowserProfileSelectionChange_() {
    this.selected_ = this.browserProfiles_[this.$.browserSelect.selectedIndex];
  }

  private onActionButtonClick_() {
    const checkboxes = this.shadowRoot!.querySelectorAll('settings-checkbox');
    if (this.isImportFromFileSelected_()) {
      this.browserProxy_.importFromBookmarksFile();
    } else {
      const types: {[type: string]: boolean} = {};
      checkboxes.forEach(checkbox => {
        types[checkbox.pref!.key] = checkbox.checked && !checkbox.hidden;
      });
      this.browserProxy_.importData(this.$.browserSelect.selectedIndex, types);
    }
    checkboxes.forEach(checkbox => checkbox.sendPrefChange());
  }

  private closeDialog_() {
    this.$.dialog.close();
  }

  /**
   * @return Whether the import button should be disabled.
   */
  private shouldDisableImport_(): boolean {
    return this.hasImportStatus_(ImportDataStatus.IN_PROGRESS) ||
        this.noImportDataTypeSelected_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-import-data-dialog': SettingsImportDataDialogElement;
  }
}

customElements.define(
    SettingsImportDataDialogElement.is, SettingsImportDataDialogElement);
