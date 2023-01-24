// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-manage-user-dictionary-page' is a
 * sub-page for editing the dictionary of words used for Japanese input
 * methods.
 */

import '../../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior, GlobalScrollTargetBehaviorInterface} from '../global_scroll_target_behavior.js';
import {routes} from '../os_route.js';

import {getTemplate} from './os_japanese_manage_user_dictionary_page.html.js';

// TODO(b/265559727): Remove I18nMixin if `this.i18n` methods are not being used
// by this element.
const OsSettingsJapaneseManageUserDictionaryPageElementBase =
    I18nMixin(PolymerElement);

const OsSettingsJapaneseManageUserDictionaryPageElementBaseWithBehaviors =
    mixinBehaviors(
        // TODO(b/265559727): Remove GlobalScrollTargetBehavior if it is unused.
        [GlobalScrollTargetBehavior],
        OsSettingsJapaneseManageUserDictionaryPageElementBase) as
        typeof OsSettingsJapaneseManageUserDictionaryPageElementBase &
    (new (...args: any[]) => GlobalScrollTargetBehaviorInterface);

class OsSettingsJapaneseManageUserDictionaryPageElement extends
    OsSettingsJapaneseManageUserDictionaryPageElementBaseWithBehaviors {
  static get is() {
    return 'os-settings-japanese-manage-user-dictionary-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      // TODO(b/265554350): Remove this property from properties() as it is
      // already specified in GlobalScrollTargetBehavior, and move the default
      // value to the field initializer.
      /**
       * Needed for GlobalScrollTargetBehavior.
       */
      subpageRoute: {
        type: Object,
        value: routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY,
      },
    };
  }

  // Internal properties for mixins.
  // From GlobalScrollTargetBehavior.
  // protected override subpageRoute =
  //     routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY;
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
