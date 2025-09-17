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
import {assert} from 'chrome://resources/js/assert.js';
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

  // SettingsViewMixin implementation.
  override getFocusConfig() {
    const map = new Map();

    const focusInPrivacyPage = (selector: string) => {
      const toFocus = this.shadowRoot!.querySelector('settings-privacy-page')!
                          .shadowRoot!.querySelector<HTMLElement>(selector);
      assert(toFocus);
      toFocus.focus();
    };

    if (routes.COOKIES) {
      map.set(
          routes.COOKIES.path,
          focusInPrivacyPage.bind(this, '#thirdPartyCookiesLinkRow'));
    }

    if (routes.INCOGNITO_TRACKING_PROTECTIONS) {
      map.set(
          routes.INCOGNITO_TRACKING_PROTECTIONS.path,
          focusInPrivacyPage.bind(
              this, '#incognitoTrackingProtectionsLinkRow'));
    }

    if (routes.PRIVACY_GUIDE) {
      map.set(
          routes.PRIVACY_GUIDE.path,
          focusInPrivacyPage.bind(this, '#privacyGuideLinkRow'));
    }

    if (routes.PRIVACY_SANDBOX) {
      map.set(
          routes.PRIVACY_SANDBOX.path,
          focusInPrivacyPage.bind(this, '#privacySandboxLinkRow'));
    }

    if (routes.SECURITY) {
      map.set(
          routes.SECURITY.path,
          focusInPrivacyPage.bind(this, '#securityLinkRow'));
    }

    if (routes.SITE_SETTINGS) {
      map.set(
          routes.SITE_SETTINGS.path,
          focusInPrivacyPage.bind(this, '#permissionsLinkRow'));
    }

    return map;
  }

  // SettingsViewMixin implementation.
  override getAssociatedControlFor(childViewId: string): HTMLElement {
    // TODO(crbug.com/424223101): getAssociatedControlFor() can only ever be
    // called for privacy specific subpages. Remove once the
    // <settings-basic-page> node intermediate layer is removed.
    return this.shadowRoot!.querySelector('settings-privacy-page')!
        .getAssociatedControlFor(childViewId);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-basic-page': SettingsBasicPageElement;
  }
}

customElements.define(SettingsBasicPageElement.is, SettingsBasicPageElement);
