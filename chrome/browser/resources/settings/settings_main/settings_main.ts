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
import '../basic_page/basic_page.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {assert} from 'chrome://resources/js/assert.js';
import {beforeNextRender, flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {pageVisibility} from '../page_visibility.js';
import type {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import {combineSearchResults} from '../search_settings.js';

import {getTemplate} from './settings_main.html.js';
import type {SettingsPlugin} from './settings_plugin.js';


function getTopLevelRoute() {
  if (!loadTimeData.getBoolean('isGuest')) {
    return routes.PEOPLE;
  }

  let guestTopLevelRoute = routes.SEARCH;
  // <if expr="chromeos_ash">
  guestTopLevelRoute = routes.PRIVACY;
  // </if>

  return guestTopLevelRoute;
}

const TOP_LEVEL_EQUIVALENT_ROUTE: Route = getTopLevelRoute();

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
        value: pageVisibility || {},
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

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },
    };
  }

  declare prefs: {[key: string]: any};
  declare private pageVisibility_: PageVisibility;
  declare private lastRoute_: Route|null;
  declare private routes_: SettingsRoutes;
  declare private inSearchMode_: boolean;
  declare private showNoResultsFound_: boolean;
  declare toolbarSpinnerActive: boolean;

  private beforeNextRenderPromise_(): Promise<void> {
    return new Promise(res => {
      beforeNextRender(this, res);
    });
  }

  override async currentRouteChanged(route: Route) {
    const effectiveRoute =
        route === routes.BASIC ? TOP_LEVEL_EQUIVALENT_ROUTE : route;

    if (this.lastRoute_ === effectiveRoute) {
      // Nothing to do.
      return;
    }

    this.lastRoute_ = effectiveRoute;

    if (!route.hasMigratedToPlugin) {
      // Case where the requested section still resides within the old
      // <settings-basic-page> element. Show that element, and let it handle
      // showing the correct content.
      this.$.switcher.switchView('old', 'no-animation', 'no-animation');
      return;
    }

    // Case where the requested section has migrated to the new "plugin"
    // architecture.
    const newSection = effectiveRoute.section;
    let sectionElement = this.$.switcher.querySelector(`#${newSection}`);
    if (!sectionElement) {
      // Wait for any pageVisibility <dom-if>s to render and try again.
      await this.beforeNextRenderPromise_();
      sectionElement = this.$.switcher.querySelector(`#${newSection}`);
    }

    assert(sectionElement);
    this.$.switcher.switchView(
        sectionElement.id, 'no-animation', 'no-animation');
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
                  query === '' ? false : !result.didFindMatches);
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
      this.showNoResultsFound_ = this.inSearchMode_ && !result.didFindMatches;

      if (this.inSearchMode_) {
        getAnnouncerInstance().announce(
            this.showNoResultsFound_ ?
                loadTimeData.getString('searchNoResults') :
                loadTimeData.getStringF('searchResults', query));
      }
    });
  }

  private renderBasicPage_(): boolean {
    return this.lastRoute_ !== routes.ABOUT;
  }

  private renderPlugin_(route: Route): boolean {
    assert(route.hasMigratedToPlugin);
    return this.inSearchMode_ ||
        (!!this.lastRoute_ && route.contains(this.lastRoute_));
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private showManagedHeader_(): boolean {
    return !this.inSearchMode_ && !!this.lastRoute_ &&
        this.lastRoute_ !== routes.ABOUT && !this.lastRoute_.isSubpage();
  }

  private shouldShowAll_(): boolean {
    return this.inSearchMode_ && !!this.lastRoute_ &&
        !this.lastRoute_.isSubpage();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-main': SettingsMainElement;
  }
}

customElements.define(SettingsMainElement.is, SettingsMainElement);
