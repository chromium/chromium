// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-manage-user-dictionary-page' is a
 * sub-page for editing the dictionary of words used for Japanese input
 * methods.
 */

import './os_japanese_dictionary_expand.js';
import '../settings_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';

import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../common/global_scroll_target_mixin.js';
import {JapaneseDictionary} from '../mojom-webui/user_data_japanese_dictionary.mojom-webui.js';
import {routes} from '../router.js';

import {getTemplate} from './os_japanese_manage_user_dictionary_page.html.js';
import {UserDataServiceProvider} from './user_data_service_provider.js';

// TODO(b/265559727): Remove I18nMixin if `this.i18n` methods are not being used
// by this element.
// TODO(b/265559727): Remove GlobalScrollTargetMixin if it is unused.
const OsSettingsJapaneseManageUserDictionaryPageElementBase =
    GlobalScrollTargetMixin(I18nMixin(PolymerElement));

const NEW_DICTIONARY_NAME = 'New Dictionary';

class OsSettingsJapaneseManageUserDictionaryPageElement extends
    OsSettingsJapaneseManageUserDictionaryPageElementBase {
  static get is() {
    return 'os-settings-japanese-manage-user-dictionary-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      status: {
        type: String,
        value: 'No dictionary call made',
      },
    };
  }

  // Status that is shown on the page for debugging.
  status: string;

  // Internal properties for mixins.
  // From GlobalScrollTargetMixin.
  override subpageRoute = routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY;

  // All Japanese user dictionary data that is loaded into the app.
  private dictionaries_: JapaneseDictionary[] = [];

  override ready(): void {
    super.ready();
    this.addEventListener('dictionary-saved', this.getDictionaries_);
  }

  // Loads the dictionary objects from IME user data service.
  private async getDictionaries_(): Promise<void> {
    const response =
        (await UserDataServiceProvider.getRemote().fetchJapaneseDictionary())
            .response;
    if (response.errorReason !== undefined) {
      this.status = response.errorReason;
    }

    if (response.dictionaries !== undefined) {
      this.dictionaries_ = [...response.dictionaries];
      this.status = `number of dictionaries=${this.dictionaries_.length}`;
    }
  }

  // Adds a new dictionary with the name "New dictionary".
  private async addDictionary_(): Promise<void> {
    const resp =
        (await UserDataServiceProvider.getRemote().createJapaneseDictionary(
             this.newDictName_()))
            .status;
    if (resp.success) {
      this.getDictionaries_();
    }
  }

  // The backend does not let you add the same dictionary name twice. We have to
  // automatically append an incrementing number to it if there is a clash.
  private newDictName_(): string {
    let count = 0;
    let newName = NEW_DICTIONARY_NAME;
    while (this.dictionaries_.some(
        (dict: JapaneseDictionary) => dict.name === newName)) {
      count++;
      newName = `${NEW_DICTIONARY_NAME} ${count}`;
    }

    return newName;
  }

  // Used to get the last index of the synced entries of each dictionary so that
  // dictionary components can figure out whether to add or edit entries when
  // saving to storage.
  private getLastIndex_(x: Object[]): number {
    return x.length - 1;
  }
}

customElements.define(
    OsSettingsJapaneseManageUserDictionaryPageElement.is,
    OsSettingsJapaneseManageUserDictionaryPageElement);

declare global {
  interface HTMLElementTagNameMap {
    [OsSettingsJapaneseManageUserDictionaryPageElement.is]:
        OsSettingsJapaneseManageUserDictionaryPageElement;
  }
}
