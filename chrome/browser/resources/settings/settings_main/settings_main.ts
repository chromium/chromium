// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-main' displays the selected settings page.
 */
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/js/search_highlight_utils.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import '../about_page/about_page.js';
import '../basic_page/basic_page.js';
import '../search_settings.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import type {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {RouteObserverMixin, Router} from '../router.js';

import {getTemplate} from './settings_main.html.js';

interface MainPageVisibility {
  about: boolean;
  settings: boolean;
}

export interface SettingsMainElement {
  $: {
    noSearchResults: HTMLElement,
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

      /**
       * Controls which main pages are displayed via dom-ifs, based on the
       * current route.
       */
      showPages_: {
        type: Object,
        value() {
          return {about: false, settings: false};
        },
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

      showingSubpage_: Boolean,

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: Object,
    };
  }

  prefs: {[key: string]: any};
  private showPages_: MainPageVisibility;
  private inSearchMode_: boolean;
  private showNoResultsFound_: boolean;
  private showingSubpage_: boolean;
  toolbarSpinnerActive: boolean;
  pageVisibility?: PageVisibility;

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   */
  override currentRouteChanged() {
    const inAbout =
        routes.ABOUT.contains(Router.getInstance().getCurrentRoute());
    this.showPages_ = {about: inAbout, settings: !inAbout};
  }

  private onShowingSubpage_() {
    this.showingSubpage_ = true;
  }

  private onShowingMainPage_() {
    this.showingSubpage_ = false;
  }

  /**
   * @return A promise indicating that searching finished.
   */
  searchContents(query: string): Promise<void> {
    // Trigger rendering of the basic and advanced pages and search once ready.
    this.inSearchMode_ = true;
    this.toolbarSpinnerActive = true;

    return new Promise((resolve, _reject) => {
      setTimeout(() => {
        const page = this.shadowRoot!.querySelector('settings-basic-page')!;
        page.searchContents(query).then(result => {
          resolve();
          if (result.canceled) {
            // Nothing to do here. A previous search request was canceled
            // because a new search request was issued with a different query
            // before the previous completed.
            return;
          }

          this.toolbarSpinnerActive = false;
          this.inSearchMode_ = !result.wasClearSearch;
          this.showNoResultsFound_ =
              this.inSearchMode_ && !result.didFindMatches;

          if (this.inSearchMode_) {
            getAnnouncerInstance().announce(
                this.showNoResultsFound_ ?
                    loadTimeData.getString('searchNoResults') :
                    loadTimeData.getStringF('searchResults', query));
          }
        });
      }, 0);
    });
  }

  private showManagedHeader_(): boolean {
    return !this.inSearchMode_ && !this.showingSubpage_ &&
        !this.showPages_.about;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-main': SettingsMainElement;
  }
}

customElements.define(SettingsMainElement.is, SettingsMainElement);
