// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-get-most-chrome-page' is the settings page information about how
 * to get the most out of Chrome.
 */

import '../icons.html.js';
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {MetricsBrowserProxyImpl} from '../metrics_browser_proxy.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';

import {getTemplate} from './get_most_chrome_page.html.js';

export enum GetTheMostOutOfChromeUserAction {
  FIRST_SECTION_EXPANDED =
    'Settings.GetTheMostOutOfChrome.FirstSectionExpanded',
  SECOND_SECTION_EXPANDED =
    'Settings.GetTheMostOutOfChrome.SecondSectionExpanded',
  THIRD_SECTION_EXPANDED =
    'Settings.GetTheMostOutOfChrome.ThirdSectionExpanded',
}

const SettingsGetMostChromePageElementBase =
    RouteObserverMixin(I18nMixin(PolymerElement));

export class SettingsGetMostChromePageElement extends
    SettingsGetMostChromePageElementBase {
  static get is() {
    return 'settings-get-most-chrome-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      expandedFirst_: {
        type: Boolean,
        value: false,
        observer: 'firstChanged_',
      },
      expandedSecond_: {
        type: Boolean,
        value: false,
        observer: 'secondChanged_',
      },
      expandedThird_: {
        type: Boolean,
        value: false,
        observer: 'thirdChanged_',
      },
    };
  }

  private expandedFirst_: boolean;
  private expandedSecond_: boolean;
  private expandedThird_: boolean;

  override ready() {
    super.ready();

    // Add aria-description to links dynamically, as the links reside in
    // localized strings.
    this.shadowRoot!.querySelectorAll('a').forEach(
        link =>
            link.setAttribute('aria-description', this.i18n('opensInNewTab')));
  }

  override currentRouteChanged(newRoute: Route) {
    if (newRoute === Router.getInstance().getRoutes().GET_MOST_CHROME) {
      HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
          TrustSafetyInteraction.OPENED_GET_MOST_CHROME);
    }
  }

  private firstChanged_() {
    if (this.expandedFirst_) {
      MetricsBrowserProxyImpl.getInstance().recordAction(
          GetTheMostOutOfChromeUserAction.FIRST_SECTION_EXPANDED);
    }
  }

  private secondChanged_() {
    if (this.expandedSecond_) {
      MetricsBrowserProxyImpl.getInstance().recordAction(
          GetTheMostOutOfChromeUserAction.SECOND_SECTION_EXPANDED);
    }
  }

  private thirdChanged_() {
    if (this.expandedThird_) {
      MetricsBrowserProxyImpl.getInstance().recordAction(
          GetTheMostOutOfChromeUserAction.THIRD_SECTION_EXPANDED);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-get-most-chrome-page': SettingsGetMostChromePageElement;
  }
}

customElements.define(
    SettingsGetMostChromePageElement.is, SettingsGetMostChromePageElement);
