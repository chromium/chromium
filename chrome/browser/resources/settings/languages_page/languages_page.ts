// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-languages-page' is the settings page
 * for language and input method settings.
 */

import './languages_subpage.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface} from '../router.js';

import {getTemplate} from './languages_page.html.js';
import {LanguageSettingsMetricsProxy, LanguageSettingsMetricsProxyImpl, LanguageSettingsPageImpressionType} from './languages_settings_metrics_proxy.js';

const SettingsLanguagesPageElementBase =
    RouteObserverMixin(PolymerElement) as {
      new (): PolymerElement & RouteObserverMixinInterface,
    };

export class SettingsLanguagesPageElement extends
    SettingsLanguagesPageElementBase {
  static get is() {
    return 'settings-languages-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,
    };
  }

  private languageSettingsMetricsProxy_: LanguageSettingsMetricsProxy =
      LanguageSettingsMetricsProxyImpl.getInstance();

  override currentRouteChanged(currentRoute: Route) {
    if (currentRoute === routes.LANGUAGES) {
      this.languageSettingsMetricsProxy_.recordPageImpressionMetric(
          LanguageSettingsPageImpressionType.MAIN);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-languages-page': SettingsLanguagesPageElement;
  }
}

customElements.define(
    SettingsLanguagesPageElement.is, SettingsLanguagesPageElement);
