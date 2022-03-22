// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-subpage' is the settings sub-page containing
 * search engine and quick answers settings.
 */
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/icons.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import '../../prefs/pref_util.js';
import '../../settings_shared_css.js';
import '../../settings_vars_css.js';
import '//resources/cr_components/localized_link/localized_link.js';
import './search_engine.js';

import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.js';
import {routes} from '../os_route.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-search-subpage',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kPreferredSearchEngine,
        chromeos.settings.mojom.Setting.kQuickAnswersOnOff,
        chromeos.settings.mojom.Setting.kQuickAnswersDefinition,
        chromeos.settings.mojom.Setting.kQuickAnswersTranslation,
        chromeos.settings.mojom.Setting.kQuickAnswersUnitConversion,
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
      }
    },

    /** @private */
    translationSubLabel_: {
      type: String,
      value() {
        return this.getAriaLabelledSubLabel_(
            this.i18nAdvanced('quickAnswersTranslationEnableDescription'));
      }
    },
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.SEARCH_SUBPAGE) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @private
   */
  onSettingsLinkClick_() {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  },

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
  },
});
