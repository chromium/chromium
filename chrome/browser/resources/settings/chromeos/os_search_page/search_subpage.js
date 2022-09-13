// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-subpage' is the settings sub-page containing
 * search engine and quick answers settings.
 */
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../prefs/pref_util.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';
import 'chrome://resources/cr_components/localized_link/localized_link.js';
import './search_engine.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/cr_elements/i18n_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior, PrefsBehaviorInterface} from '../prefs_behavior.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {PrefsBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 */
const SettingsSearchSubpageElementBase = mixinBehaviors(
    [DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior],
    PolymerElement);

/** @polymer */
class SettingsSearchSubpageElement extends SettingsSearchSubpageElementBase {
  static get is() {
    return 'settings-search-subpage';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kPreferredSearchEngine,
          Setting.kQuickAnswersOnOff,
          Setting.kQuickAnswersDefinition,
          Setting.kQuickAnswersTranslation,
          Setting.kQuickAnswersUnitConversion,
        ]),
      },

      /** @private */
      quickAnswersTranslationDisabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('quickAnswersTranslationDisabled');
        },
      },

      /** @private */
      quickAnswersSubToggleEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('quickAnswersSubToggleEnabled');
        },
      },

      /** @private */
      quickAnswersSubLabel_: {
        type: String,
        value() {
          return this.getAriaLabelledSubLabel_(
              this.i18nAdvanced('quickAnswersEnableDescriptionWithLink'));
        },
      },

      /** @private */
      translationSubLabel_: {
        type: String,
        value() {
          return this.getAriaLabelledSubLabel_(
              this.i18nAdvanced('quickAnswersTranslationEnableDescription'));
        },
      },
    };
  }

  /**
   * @param {!Route} route
   * @param {!Route=} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.SEARCH_SUBPAGE) {
      return;
    }

    this.attemptDeepLink();
  }

  /**
   * @private
   */
  onSettingsLinkClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  }

  /**
   * Attaches aria attributes to the sub label.
   * @param {string} subLabel
   * @return {string}
   * @private
   */
  getAriaLabelledSubLabel_(subLabel) {
    // Creating a <localized-link> to get aria-labelled content with
    // the link. Since <settings-toggle-button> is a shared element which does
    // not have access to <localized-link> internally, we create dummy
    // element and take its innerHTML here.
    const link = document.createElement('localized-link');
    link.setAttribute('localized-string', subLabel);
    link.setAttribute('hidden', true);
    document.body.appendChild(link);
    const innerHTML = link.shadowRoot.querySelector('#container').innerHTML;
    document.body.removeChild(link);
    return innerHTML;
  }
}

customElements.define(
    SettingsSearchSubpageElement.is, SettingsSearchSubpageElement);
