// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-japanese-manage-user-dictionary-page' is a
 * sub-page for editing the dictionary of words used for Japanese input
 * methods.
 */

import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route} from '../router.js';
import {GlobalScrollTargetBehavior, GlobalScrollTargetBehaviorInterface} from '../global_scroll_target_behavior.js';
import {routes} from '../os_route.js';

import {getTemplate} from './os_japanese_manage_user_dictionary_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {GlobalScrollTargetBehaviorInterface}
 */
const OsSettingsJapaneseManageUserDictionaryPageElementBase =
    mixinBehaviors([I18nBehavior, GlobalScrollTargetBehavior], PolymerElement);

/** @polymer */
class OsSettingsJapaneseManageUserDictionaryPageElement extends
    OsSettingsJapaneseManageUserDictionaryPageElementBase {
  static get is() {
    return 'os-settings-japanese-manage-user-dictionary-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Needed for GlobalScrollTargetBehavior.
       * @type {!Route}
       * @override
       */
      subpageRoute: {
        type: Object,
        value: routes.OS_LANGUAGES_JAPANESE_MANAGE_USER_DICTIONARY,
      },
    };
  }
}

customElements.define(
    OsSettingsJapaneseManageUserDictionaryPageElement.is,
    OsSettingsJapaneseManageUserDictionaryPageElement);
