// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-manage-user-dictionary-page' is a
 * sub-page for editing the dictionary of words used for Japanese input
 * methods.
 */

import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../common/global_scroll_target_mixin.js';
import {routes} from '../router.js';

import {getTemplate} from './os_japanese_manage_user_dictionary_page.html.js';

// TODO(b/265559727): Remove I18nMixin if `this.i18n` methods are not being used
// by this element.
// TODO(b/265559727): Remove GlobalScrollTargetMixin if it is unused.
const OsSettingsJapaneseManageUserDictionaryPageElementBase =
    GlobalScrollTargetMixin(I18nMixin(PolymerElement));

class OsSettingsJapaneseManageUserDictionaryPageElement extends
    OsSettingsJapaneseManageUserDictionaryPageElementBase {
  static get is() {
    return 'os-settings-japanese-manage-user-dictionary-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  // Internal properties for mixins.
  // From GlobalScrollTargetMixin.
  override subpageRoute = routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY;
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
