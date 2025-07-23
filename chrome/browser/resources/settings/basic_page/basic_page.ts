// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../privacy_page/privacy_guide/privacy_guide_promo.js';
import '../privacy_page/privacy_page.js';
import '../safety_hub/safety_hub_entry_point.js';
import '../settings_page/settings_section.js';
import '../settings_page_styles.css.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


import {pageVisibility} from '../page_visibility.js';
import type {PageVisibility} from '../page_visibility.js';
import {PrivacyGuideAvailabilityMixin} from '../privacy_page/privacy_guide/privacy_guide_availability_mixin.js';
import type {PrivacyGuideBrowserProxy} from '../privacy_page/privacy_guide/privacy_guide_browser_proxy.js';
import {MAX_PRIVACY_GUIDE_PROMO_IMPRESSION, PrivacyGuideBrowserProxyImpl} from '../privacy_page/privacy_guide/privacy_guide_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import {getSearchManager} from '../search_settings.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {MainPageMixin} from '../settings_page/main_page_mixin.js';

import {getTemplate} from './basic_page.html.js';

const SettingsBasicPageElementBase =
    PrefsMixin(MainPageMixin(RouteObserverMixin(
        PrivacyGuideAvailabilityMixin(I18nMixin(PolymerElement)))));

export class SettingsBasicPageElement extends SettingsBasicPageElementBase
    implements SettingsPlugin {
  static get is() {
    return 'settings-basic-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Dictionary defining page visibility.
       */
      pageVisibility_: {
        type: Object,
        value() {
          return pageVisibility || {};
        },
      },

      /**
       * Whether a search operation is in progress or previous search
       * results are being displayed.
       */
      inSearchMode: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * True if the basic page should currently display the privacy guide
       * promo.
       */
      showPrivacyGuidePromo_: {
        type: Boolean,
        value: false,
      },

      currentRoute_: Object,
    };
  }

  static get observers() {
    return [
      'updatePrivacyGuidePromoVisibility_(isPrivacyGuideAvailable, prefs.privacy_guide.viewed.value)',
    ];
  }

  declare private pageVisibility_: PageVisibility;
  declare inSearchMode: boolean;

  declare private currentRoute_: Route;
  declare private showPrivacyGuidePromo_: boolean;
  private privacyGuidePromoWasShown_: boolean;
  private privacyGuideBrowserProxy_: PrivacyGuideBrowserProxy =
      PrivacyGuideBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.currentRoute_ = Router.getInstance().getCurrentRoute();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    super.currentRouteChanged(newRoute, oldRoute);
    if (newRoute === routes.PRIVACY) {
      this.updatePrivacyGuidePromoVisibility_();
    }
  }

  /** Overrides MainPageMixin method. */
  override containsRoute(route: Route|null): boolean {
    return !route || routes.PRIVACY.contains(route);
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private updatePrivacyGuidePromoVisibility_() {
    if (!this.isPrivacyGuideAvailable ||
        this.pageVisibility_.privacy === false || this.prefs === undefined ||
        this.getPref('privacy_guide.viewed').value ||
        this.privacyGuideBrowserProxy_.getPromoImpressionCount() >=
            MAX_PRIVACY_GUIDE_PROMO_IMPRESSION ||
        this.currentRoute_ !== routes.PRIVACY) {
      this.showPrivacyGuidePromo_ = false;
      return;
    }
    this.showPrivacyGuidePromo_ = true;
    if (!this.privacyGuidePromoWasShown_) {
      this.privacyGuideBrowserProxy_.incrementPromoImpressionCount();
      this.privacyGuidePromoWasShown_ = true;
    }
  }

  // SettingsPlugin implementation.
  async searchContents(query: string) {
    const basicPage = this.shadowRoot!.querySelector<HTMLElement>('#basicPage');
    assert(basicPage);
    const request = await getSearchManager().search(query, basicPage);
    return request.getSearchResult();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-basic-page': SettingsBasicPageElement;
  }
}

customElements.define(SettingsBasicPageElement.is, SettingsBasicPageElement);
