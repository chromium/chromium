// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import 'chrome://resources/cr_elements/icons.html.js';
import '../about_page/about_page.js';
import '../ai_page/ai_page_index.js';
import '../appearance_page/appearance_page_index.js';
import '../autofill_page/autofill_page_index.js';
import '../on_startup_page/on_startup_page.js';
import '../people_page/people_page_index.js';
import '../performance_page/performance_page_index.js';
import '../privacy_page/privacy_page_index.js';
import '../reset_page/reset_profile_banner.js';
import '../search_page/search_page_index.js';
import '../your_saved_info_page/your_saved_info_page_index.js';
// <if expr="not is_chromeos">
import '../default_browser_page/default_browser_page.js';

// </if>

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {beforeNextRender, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="not is_chromeos">
import type {LanguagesModel} from '../languages_page/languages_types.js';
// </if>
import {pageVisibility} from '../page_visibility.js';
import type {PageVisibility} from '../page_visibility.js';
import {getTopLevelRoute, routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import {combineSearchResults} from '../search_settings.js';

import {getTemplate} from './settings_main.html.js';
import type {SettingsPlugin} from './settings_plugin.js';


export interface SettingsMainElement {
  $: {
    noSearchResults: HTMLElement,
    switcher: CrViewManagerElement,
  };
}

const SettingsMainElementBase = RouteObserverMixin(PolymerElement);

export class SettingsMainElement extends SettingsMainElementBase {
  static get is() {
    return 'settings-main';
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

      pageVisibility_: {
        type: Object,
        value: () => pageVisibility || {},
      },

      lastRoute_: {
        type: Object,
        value: null,
      },

      routes_: {
        type: Object,
        value: () => routes,
      },

      /**
       * Whether a search operation is in progress or previous search results
       * are being displayed.
       */
      inSearchMode_: {
        type: Boolean,
        value: false,
      },

      showNoResultsFound_: {
        type: Boolean,
        value: false,
      },

      showResetProfileBanner_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showResetProfileBanner');
        },
      },

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },

      // <if expr="not is_chromeos">
      languages_: Object,
      // </if>
    };
  }

  declare prefs: {[key: string]: any};
  declare private pageVisibility_: PageVisibility;
  declare private lastRoute_: Route|null;
  declare private routes_: SettingsRoutes;
  declare private inSearchMode_: boolean;
  declare private showNoResultsFound_: boolean;
  declare private showResetProfileBanner_: boolean;
  declare toolbarSpinnerActive: boolean;

  // <if expr="not is_chromeos">
  declare private languages_?: LanguagesModel;
  // </if>

  private pendingViewSwitching_: PromiseResolver<void> = new PromiseResolver();
  private topLevelEquivalentRoute_: Route = getTopLevelRoute();

  override connectedCallback() {
    super.connectedCallback();

    this.setAttribute('role', 'main');

    // Request loading of the lazy loaded module within an idle callback.
    requestIdleCallback(() => ensureLazyLoaded());
  }

  private beforeNextRenderPromise_(): Promise<void> {
    return new Promise(res => {
      beforeNextRender(this, res);
    });
  }

  override async currentRouteChanged(route: Route) {
    this.pendingViewSwitching_ = new PromiseResolver();

    if (routes.ADVANCED && routes.ADVANCED.contains(route)) {
      // Load the lazy module immediately, don't wait for requestIdleCallback()
      // to fire. No-op if it has already fired.
      ensureLazyLoaded();
    }

    const effectiveRoute =
        route === routes.BASIC ? this.topLevelEquivalentRoute_ : route;

    if (this.lastRoute_ === effectiveRoute) {
      // Nothing to do.
      this.pendingViewSwitching_.resolve();
      return;
    }

    this.lastRoute_ = effectiveRoute;

    const newSection = effectiveRoute.section;
    let sectionElement = this.$.switcher.querySelector(`#${newSection}`);
    if (!sectionElement) {
      // Wait for any pageVisibility <dom-if>s to render and try again.
      await this.beforeNextRenderPromise_();

      if (this.lastRoute_ !== effectiveRoute || !this.isConnected) {
        // A newer currentRouteChanged call happened while awaiting or no longer
        // connected (both can happen in tests). Do nothing.
        this.pendingViewSwitching_.resolve();
        return;
      }
      sectionElement = this.$.switcher.querySelector(`#${newSection}`);
    }

    assert(sectionElement);
    await this.$.switcher.switchView(
        sectionElement.id, 'no-animation', 'no-animation');
    this.pendingViewSwitching_.resolve();
  }

  // Exposed for tests, to allow making visibility assertions about
  // cr-view-manager views without flaking. Should be called after
  // currentRouteChanged is called.
  whenViewSwitchingDone(): Promise<void> {
    return this.pendingViewSwitching_.promise;
  }

  /**
   * @return A promise indicating that searching finished.
   */
  searchContents(query: string): Promise<void> {
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;

    if (query === '') {
      // Synchronously remove 'show-all' instead of waiting for later observers
      // to do so, to avoid a noticeable flicker when clearing search results.
      this.$.switcher.toggleAttribute('show-all', false);
    }

    // Call flush() to ensure any dom-if nodes have been updated.
    flush();

    // Issue a search requests for each plugin.
    const toSearch =
        Array.from(this.$.switcher.querySelectorAll<HTMLElement&SettingsPlugin>(
            '[slot=view] > *:not(dom-if)'));
    const promises = toSearch.map(element => {
      return customElements.whenDefined(element.tagName.toLowerCase())
          .then(() => {
            return element.searchContents(query).then(result => {
              // Reveal each plugin immediately, instead of waiting of all
              // search results to come back.
              element.toggleAttribute(
                  'hidden-by-search',
                  query === '' ? false : result.matchCount === 0);
              return result;
            });
          });
    });

    return Promise.all(promises).then(results => {
      const result = combineSearchResults(results);
      if (result.canceled) {
        // Nothing to do here. A previous search request was canceled
        // because a new search request was issued with a different query
        // before the previous completed.
        return;
      }

      this.toolbarSpinnerActive = false;
      this.inSearchMode_ = !result.wasClearSearch;
      this.showNoResultsFound_ = this.inSearchMode_ && result.matchCount === 0;

      if (this.inSearchMode_) {
        getAnnouncerInstance().announce(
            this.showNoResultsFound_ ?
                loadTimeData.getString('searchNoResults') :
                loadTimeData.getStringF('searchResults', query));
      }
    });
  }

  private renderPlugin_(route: Route): boolean {
    return this.inSearchMode_ ||
        (!!this.lastRoute_ && route.contains(this.lastRoute_));
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private showAiPage_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('showAiPage') && this.showPage_(visibility);
  }

  private showAutofillPage_(visibility?: boolean): boolean {
    return !loadTimeData.getBoolean('enableYourSavedInfoSettingsPage') &&
        this.showPage_(visibility);
  }

  private showYourSavedInfoPage_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('enableYourSavedInfoSettingsPage') &&
        this.showPage_(visibility);
  }

  private showManagedHeader_(): boolean {
    return !this.inSearchMode_ && !!this.lastRoute_ &&
        this.lastRoute_ !== routes.ABOUT && !this.lastRoute_.isSubpage();
  }

  private shouldShowAll_(): boolean {
    return this.inSearchMode_ && !!this.lastRoute_ &&
        !this.lastRoute_.isSubpage();
  }

  private onResetProfileBannerClose_() {
    this.showResetProfileBanner_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-main': SettingsMainElement;
  }
}

customElements.define(SettingsMainElement.is, SettingsMainElement);
