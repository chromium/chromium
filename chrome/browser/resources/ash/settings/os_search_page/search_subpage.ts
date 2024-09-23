// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-search-subpage' is the settings sub-page containing
 * search engine and quick answers settings.
 */
import 'chrome://resources/ash/common/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/policy/cr_policy_pref_indicator.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '../controls/controlled_button.js';
import '../controls/settings_toggle_button.js';
import '/shared/settings/prefs/prefs.js';
import '/shared/settings/prefs/pref_util.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import './search_engine.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {DeepLinkingMixin} from '../common/deep_linking_mixin.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {Setting} from '../mojom-webui/setting.mojom-webui.js';
import {Route, Router, routes} from '../router.js';

import {getTemplate} from './search_subpage.html.js';

const SettingsSearchSubpageElementBase =
    DeepLinkingMixin(RouteObserverMixin(PrefsMixin(I18nMixin(PolymerElement))));

export class SettingsSearchSubpageElement extends
    SettingsSearchSubpageElementBase {
  static get is() {
    return 'settings-search-subpage';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Used by DeepLinkingMixin to focus this page's deep links.
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set<Setting>([
          Setting.kPreferredSearchEngine,
          Setting.kQuickAnswersOnOff,
          Setting.kQuickAnswersDefinition,
          Setting.kQuickAnswersTranslation,
          Setting.kQuickAnswersUnitConversion,
        ]),
      },

      quickAnswersTranslationDisabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('quickAnswersTranslationDisabled');
        },
      },

      quickAnswersSubToggleEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('quickAnswersSubToggleEnabled');
        },
      },

      quickAnswersSubLabel_: {
        type: String,
      },

      translationSubLabel_: {
        type: String,
      },
    };
  }

  private quickAnswersSubLabel_: string;
  private quickAnswersSubToggleEnabled_: boolean;
  private quickAnswersTranslationDisabled_: boolean;
  private translationSubLabel_: string;

  constructor() {
    super();

    this.quickAnswersSubLabel_ = this.getAriaLabelledSubLabel_(
        this.i18nAdvanced('quickAnswersEnableDescriptionWithLink').toString());
    this.translationSubLabel_ = this.getAriaLabelledSubLabel_(
        this.i18nAdvanced('quickAnswersTranslationEnableDescription')
            .toString());
  }

  override currentRouteChanged(route: Route, _oldRoute?: Route): void {
    // Does not apply to this page.
    if (route !== routes.SEARCH_SUBPAGE) {
      return;
    }

    this.attemptDeepLink();
  }

  private onSettingsLinkClick_(): void {
    Router.getInstance().navigateTo(routes.OS_LANGUAGES_LANGUAGES);
  }

  /**
   * Attaches aria attributes to the sub label.
   */
  private getAriaLabelledSubLabel_(subLabel: string): string {
    // Creating a <localized-link> to get aria-labelled content with
    // the link. Since <settings-toggle-button> is a shared element which does
    // not have access to <localized-link> internally, we create dummy
    // element and take its innerHTML here.
    const link = document.createElement('localized-link');
    link.setAttribute('localized-string', subLabel);
    link.setAttribute('hidden', 'true');
    document.body.appendChild(link);
    const innerHTML =
        castExists(link.shadowRoot!.getElementById('container')).innerHTML;
    document.body.removeChild(link);
    return innerHTML;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-search-subpage': SettingsSearchSubpageElement;
  }
}

customElements.define(
    SettingsSearchSubpageElement.is, SettingsSearchSubpageElement);
