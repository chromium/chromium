// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
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
// <if expr="not is_chromeos">
import '../default_browser_page/default_browser_page.js';

// </if>

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ensureLazyLoaded} from '../ensure_lazy_loaded.js';
import {loadTimeData} from '../i18n_setup.js';
import {pageVisibility} from '../page_visibility.js';
import type {AppearancePageVisibility, PageVisibility, PrivacyPageVisibility} from '../page_visibility.js';
import {getTopLevelRoute, routes} from '../route.js';
import {RouteObserverMixinLit} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import {combineSearchResults} from '../search_settings.js';

import {getCss} from './settings_main.css.js';
import {getHtml} from './settings_main.html.js';
import type {SettingsPlugin} from './settings_plugin.js';


export interface SearchFinishedDetail {
  query: string;
  matchCount: number;
}

declare global {
  interface HTMLElementEventMap {
    'search-finished': CustomEvent<SearchFinishedDetail>;
  }
}

export interface SettingsMainElement {
  $: {
    noSearchResults: HTMLElement,
    switcher: CrViewManagerElement,
  };
}

const SettingsMainElementBase = RouteObserverMixinLit(CrLitElement);

export class SettingsMainElement extends SettingsMainElementBase {
  static get is() {
    return 'settings-main';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageVisibility_: {type: Object},
      lastRoute_: {type: Object},
      routes_: {type: Object},

      /**
       * Whether a search operation is in progress or previous search results
       * are being displayed.
       */
      inSearchMode_: {type: Boolean},
      showNoResultsFound_: {type: Boolean},
      showResetProfileBanner_: {type: Boolean},
      settingsRefresh2026Enabled_: {type: Boolean},
      toolbarSpinnerActive: {
        type: Boolean,
        notify: true,
      },
    };
  }

  protected accessor pageVisibility_: PageVisibility = pageVisibility || {};
  protected accessor lastRoute_: Route|null = null;
  protected accessor routes_: SettingsRoutes = routes;
  protected accessor inSearchMode_: boolean = false;
  protected accessor showNoResultsFound_: boolean = false;
  protected accessor showResetProfileBanner_: boolean =
      loadTimeData.getBoolean('showResetProfileBanner');
  protected accessor settingsRefresh2026Enabled_: boolean =
      loadTimeData.getString('settingsRefresh2026') !== '';
  accessor toolbarSpinnerActive: boolean = false;

  private eventTracker_: EventTracker = new EventTracker();
  private pendingViewSwitching_: PromiseResolver<void> = new PromiseResolver();
  private topLevelEquivalentRoute_: Route = getTopLevelRoute();
  private currentQuery_: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.setAttribute('role', 'main');

    // Request loading of the lazy loaded module within an idle callback.
    requestIdleCallback(() => ensureLazyLoaded());
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('inSearchMode_')) {
      this.onInSearchModeChanged_(this.inSearchMode_);
    }
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
    await this.updateComplete;

    if (this.lastRoute_ !== effectiveRoute || !this.isConnected) {
      // A newer currentRouteChanged call happened while awaiting or no longer
      // connected (both can happen in tests). Do nothing.
      this.pendingViewSwitching_.resolve();
      return;
    }

    const newSection = effectiveRoute.section;
    const sectionElement = this.$.switcher.querySelector(`#${newSection}`);
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
  async searchContents(query: string): Promise<void> {
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;
    this.currentQuery_ = query;

    if (query === '') {
      // Synchronously remove 'show-all' instead of waiting for later observers
      // to do so, to avoid a noticeable flicker when clearing search results.
      this.$.switcher.toggleAttribute('show-all', false);
    }

    // Ensure any dynamically rendered plugins have been stamped.
    await this.updateComplete;

    // Issue a search requests for each plugin.
    const toSearch =
        Array.from(this.$.switcher.querySelectorAll<HTMLElement&SettingsPlugin>(
            '[slot=view] > *:not(dom-if)'));
    const promises = toSearch.map(async element => {
      await customElements.whenDefined(element.tagName.toLowerCase());
      if (query !== this.currentQuery_) {
        // Handle case where a newer query was issued while waiting for the
        // whenDefined calls to resolve.
        return {
          canceled: true,
          matchCount: 0,
          wasClearSearch: false,
        };
      }
      const result = await element.searchContents(query);
      // Reveal each plugin immediately, instead of waiting of all
      // search results to come back.
      element.toggleAttribute(
          'hidden-by-search', query === '' ? false : result.matchCount === 0);
      return result;
    });

    const results = await Promise.all(promises);
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

    this.fire('search-finished', {query, matchCount: result.matchCount});
  }

  /**
   * Notifies about the user interacting with one of the search results, but
   * only while search results are being displayed, so that no listener is
   * registered during normal browsing of the Settings page.
   *
   * Both 'click' and 'change' are observed, because neither covers every
   * result on its own. Results that navigate elsewhere when opened are only
   * visible as a 'click', whereas controls that commonly show up as results,
   * settings-toggle-button and cr-toggle for example, stop 'click' events from
   * propagating and fire no 'click' at all when activated with the keyboard.
   * Those do fire a composed 'change' event when their value changes.
   *
   * The two can both fire for a single interaction, and 'click' also fires for
   * clicks that landed next to a result rather than on one, so this is an
   * upper bound rather than an exact signal. Consumers are expected to be
   * interested in the first notification only.
   */
  private onInSearchModeChanged_(inSearchMode: boolean) {
    if (!inSearchMode) {
      this.eventTracker_.remove(this.$.switcher, 'click');
      this.eventTracker_.remove(this.$.switcher, 'change');
      return;
    }

    const notifyInteraction = () => {
      this.fire('search-result-interaction');
    };
    this.eventTracker_.add(this.$.switcher, 'click', notifyInteraction);
    this.eventTracker_.add(this.$.switcher, 'change', notifyInteraction);
  }

  protected renderPlugin_(route: Route): boolean {
    return this.inSearchMode_ ||
        (!!this.lastRoute_ && route.contains(this.lastRoute_));
  }

  protected showPage_(
      visibility?: boolean|AppearancePageVisibility|
      PrivacyPageVisibility): boolean {
    return visibility !== false;
  }

  protected showAiPage_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('showAiPage') && this.showPage_(visibility);
  }

  protected showManagedHeader_(): boolean {
    return !this.inSearchMode_ && !!this.lastRoute_ &&
        this.lastRoute_ !== routes.ABOUT && !this.lastRoute_.isSubpage();
  }

  protected shouldShowAll_(): boolean {
    return this.inSearchMode_ && !!this.lastRoute_ &&
        !this.lastRoute_.isSubpage();
  }

  protected onResetProfileBannerClose_() {
    this.showResetProfileBanner_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-main': SettingsMainElement;
  }
}

customElements.define(SettingsMainElement.is, SettingsMainElement);
