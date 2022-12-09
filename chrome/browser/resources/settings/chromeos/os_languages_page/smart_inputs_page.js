// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-smart-inputs-page' is the settings sub-page
 * to provide users with assistive or expressive input options.
 */

import 'chrome://resources/cr_elements/cr_toggle/cr_toggle.js';
import '../../controls/settings_toggle_button.js';
import '../../settings_shared.css.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route} from '../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

import {getTemplate} from './smart_inputs_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const OsSettingsSmartInputsPageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class OsSettingsSmartInputsPageElement extends
    OsSettingsSmartInputsPageElementBase {
  static get is() {
    return 'os-settings-smart-inputs-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /** @private */
      allowAssistivePersonalInfo_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowAssistivePersonalInfo');
        },
      },

      /** @private */
      allowEmojiSuggestion_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('allowEmojiSuggestion');
        },
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kShowPersonalInformationSuggestions,
          Setting.kShowEmojiSuggestions,
        ]),
      },
    };
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_SMART_INPUTS) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * Opens Chrome browser's autofill manage addresses setting page.
   * @private
   */
  onManagePersonalInfoClick_() {
    window.open('chrome://settings/addresses');
  }

  /**
   * @param {!Event} e
   * @private
   */
  onPersonalInfoSuggestionToggled_(e) {
    this.setPrefValue(
        'assistive_input.personal_info_enabled', e.target.checked);
  }
}

customElements.define(
    OsSettingsSmartInputsPageElement.is, OsSettingsSmartInputsPageElement);
