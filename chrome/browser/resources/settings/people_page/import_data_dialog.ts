// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-import-data-dialog' is a component for importing
 * bookmarks and other data from other sources.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../controls/settings_checkbox.js';
import '../controls/settings_toggle_button.js';
import '../i18n_setup.js';

import {PrefService} from '/shared/settings/prefs2/pref_service.js';
import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {BrowserProfile, ImportDataBrowserProxy} from './import_data_browser_proxy.js';
import {ImportDataBrowserProxyImpl, ImportDataStatus} from './import_data_browser_proxy.js';
import {getCss} from './import_data_dialog.css.js';
import {getHtml} from './import_data_dialog.html.js';

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

const SettingsImportDataDialogElementBase = WebUiListenerMixinLit(
    I18nMixinLit(PrefServiceObserverMixinLit(CrLitElement)));

export type ImportDataDialogElement = SettingsImportDataDialogElement;

export class SettingsImportDataDialogElement extends
    SettingsImportDataDialogElementBase {
  static get is() {
    return 'settings-import-data-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      browserProfiles_: {type: Array},
      importDialogBookmarksPref_: {type: Object},
      selected_: {type: Object},
      noImportDataTypeSelected_: {type: Boolean},
      importStatus_: {type: String},
    };
  }

  protected accessor browserProfiles_: BrowserProfile[] = [];
  protected accessor importDialogBookmarksPref_:
      chrome.settingsPrivate.PrefObject<boolean>|undefined;
  protected accessor selected_: BrowserProfile = {
    name: '',
    index: 0,
    profileName: '',
    history: false,
    favorites: false,
    passwords: false,
    search: false,
    autofillFormData: false,
  };
  protected accessor noImportDataTypeSelected_: boolean = false;
  protected accessor importStatus_: ImportDataStatus = ImportDataStatus.INITIAL;
  private browserProxy_: ImportDataBrowserProxy =
      ImportDataBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.mirrorPref('import_dialog_bookmarks', 'importDialogBookmarksPref_');

    PrefService.getInstance().whenInitialized().then(() => {
      if (this.isConnected) {
        this.updateImportDataTypesSelected_();
      }
    });

    this.browserProxy_.initializeImportDialog().then(data => {
      if (!this.isConnected) {
        // Element was disconnected while waiting for the backend call to
        // return. Do nothing.
        return;
      }

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
            this.$.dialog.close();
          }
        });
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);

    this.addEventListener(
        'settings-boolean-control-change',
        () => this.updateImportDataTypesSelected_());
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selected_')) {
      this.updateImportDataTypesSelected_();
    }
  }

  protected getProfileDisplayName_(name: string, profileName: string): string {
    return profileName ? `${name} - ${profileName}` : name;
  }

  protected updateImportDataTypesSelected_() {
    const checkboxes = this.shadowRoot.querySelectorAll(
        'settings-checkbox[checked]:not([hidden])');
    this.noImportDataTypeSelected_ = checkboxes.length === 0;
  }

  /**
   * @return Whether |status| is the current status.
   */
  protected hasImportStatus_(status: ImportDataStatus): boolean {
    return this.importStatus_ === status;
  }

  private isImportFromFileSelected_(): boolean {
    // The last entry in |browserProfiles_| always refers to dummy profile for
    // importing from a bookmarks file.
    return this.selected_.index === this.browserProfiles_.length - 1;
  }

  protected getActionButtonText_(): string {
    return this.i18n(
        this.isImportFromFileSelected_() ? 'importChooseFile' : 'importCommit');
  }

  protected onBrowserProfileSelectionChange_() {
    this.selected_ = this.browserProfiles_[this.$.browserSelect.selectedIndex];
  }

  protected onActionButtonClick_() {
    const checkboxes = this.shadowRoot.querySelectorAll('settings-checkbox');
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

  protected onCloseClick_() {
    this.$.dialog.close();
  }

  /**
   * @return Whether the import button should be disabled.
   */
  protected shouldDisableImport_(): boolean {
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
