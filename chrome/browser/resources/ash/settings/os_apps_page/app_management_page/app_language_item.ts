// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import '../../common/app_language_selection_dialog/app_language_selection_dialog.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppLanguageSelectionDialogEntryPoint} from '../../common/app_language_selection_dialog/app_language_selection_dialog.js';
import {PrefsState} from '../../common/types.js';

import {getTemplate} from './app_language_item.html.js';

const AppManagementAppLanguageItemElementBase = I18nMixin(PolymerElement);

export class AppManagementAppLanguageItemElement extends
    AppManagementAppLanguageItemElementBase {
  static get is() {
    return 'app-management-app-language-item' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },
      app: Object,
      hidden: {
        type: Boolean,
        computed: 'isHidden_(app)',
        reflectToAttribute: true,
      },
      showSelectLanguageDialog_: {
        type: Boolean,
        value: false,
      },
    };
  }

  // Public API: Bidirectional data flow.
  /** Passed down to children. Do not access without using PrefsMixin. */
  prefs: PrefsState;

  app?: App = undefined;
  // Polymer-overridden property to hide this component.
  override hidden: boolean;

  private showSelectLanguageDialog_: boolean;

  private onClick_(): void {
    this.showSelectLanguageDialog_ = true;
  }

  private onSelectLanguageDialogClose_(): void {
    this.showSelectLanguageDialog_ = false;
  }

  /**
   * Returns true if the app does not support per-app-language settings.
   */
  private isHidden_(): boolean {
    return !this.app?.supportedLocales?.length;
  }

  /**
   * Returns display name of the selected locale if exists.
   */
  private getSelectedLocale_(): string {
    if (this.app?.selectedLocale?.localeTag) {
      const displayName = this.app.selectedLocale.displayName;
      return displayName === '' ? this.app.selectedLocale.localeTag :
                                  displayName;
    }
    return this.i18n('appLanguageDeviceLanguageLabel');
  }

  private getDialogEntryPoint_(): AppLanguageSelectionDialogEntryPoint {
    return AppLanguageSelectionDialogEntryPoint.APPS_MANAGEMENT_PAGE;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppManagementAppLanguageItemElement.is]:
        AppManagementAppLanguageItemElement;
  }
}

customElements.define(
    AppManagementAppLanguageItemElement.is,
    AppManagementAppLanguageItemElement);
