// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_page/settings_section.js';
import '../settings_page_styles.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin} from '../router.js';
import {MainPageMixin} from '../settings_page/main_page_mixin.js';

import {getTemplate} from './basic_page.html.js';

const SettingsBasicPageElementBase =
    MainPageMixin(RouteObserverMixin(I18nMixin(PolymerElement)));

export class SettingsBasicPageElement extends SettingsBasicPageElementBase {
  static get is() {
    return 'settings-basic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether a search operation is in progress or previous search
       * results are being displayed.
       */
      inSearchMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  declare inSearchMode: boolean;

  /** Overrides MainPageMixin method. */
  override containsRoute(route: Route|null): boolean {
    return !route || routes.PRIVACY.contains(route);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-basic-page': SettingsBasicPageElement;
  }
}

customElements.define(SettingsBasicPageElement.is, SettingsBasicPageElement);
