// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-main' displays the selected settings page.
 */
import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/js/search_highlight_utils.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_about_page/os_about_page.js';
import '../os_settings_page/os_settings_page.js';
import '../../prefs/prefs.js';
import '../../settings_shared.css.js';
import '../../settings_vars.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../assert_extras.js';
import {OsPageVisibility} from '../os_page_visibility.js';
import {routes} from '../os_settings_routes.js';
import {RouteObserverMixin} from '../route_observer_mixin.js';
import {Route, Router} from '../router.js';

import {getTemplate} from './os_settings_main.html.js';

interface MainPageVisibility {
  about: boolean;
  settings: boolean;
}

interface OsSettingsMainElement {
  $: {
    overscroll: HTMLDivElement,
  };
}

const OsSettingsMainElementBase = RouteObserverMixin(PolymerElement);

class OsSettingsMainElement extends OsSettingsMainElementBase {
  static get is() {
    return 'os-settings-main';
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

      advancedToggleExpanded: {
        type: Boolean,
        notify: true,
      },

      overscroll_: {
        type: Number,
        observer: 'overscrollChanged_',
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

      showAndroidApps: Boolean,

      showArcvmManageUsb: Boolean,

      showCrostini: Boolean,

      showReset: Boolean,

      showStartup: Boolean,

      showKerberosSection: Boolean,

      havePlayStoreApp: Boolean,
    };
  }

  prefs: Object;
  advancedToggleExpanded: boolean;
  toolbarSpinnerActive: boolean;
  pageVisibility: OsPageVisibility;
  showAndroidApps: boolean;
  showArcvmManageUsb: boolean;
  showCrostini: boolean;
  showReset: boolean;
  showStartup: boolean;
  showKerberosSection: boolean;
  havePlayStoreApp: boolean;
  private overscroll_: number;
  private showPages_: MainPageVisibility;
  private showingSubpage_: boolean;
  private boundScroll_: (() => void)|null;

  constructor() {
    super();

    this.boundScroll_ = null;
  }

  private overscrollChanged_() {
    assertExists(this.offsetParent);

    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      window.removeEventListener('resize', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = () => {
        if (!this.showingSubpage_) {
          this.setOverscroll_(0);
        }
      };

      this.offsetParent.addEventListener('scroll', this.boundScroll_);
      window.addEventListener('resize', this.boundScroll_);
    }
  }

  /**
   * Sets the overscroll padding. Never forces a scroll, i.e., always leaves
   * any currently visible overflow as-is.
   * @param minHeight The minimum overscroll height needed.
   */
  private setOverscroll_(minHeight?: number) {
    const scroller = this.offsetParent;
    if (!scroller) {
      return;
    }
    const overscroll = this.$.overscroll;
    const visibleBottom = scroller.scrollTop + scroller.clientHeight;
    const overscrollBottom = overscroll.offsetTop + overscroll.scrollHeight;
    // How much of the overscroll is visible (may be negative).
    const visibleOverscroll =
        overscroll.scrollHeight - (overscrollBottom - visibleBottom);
    this.overscroll_ = Math.max(minHeight || 0, Math.ceil(visibleOverscroll));
  }

  /**
   * Updates the hidden state of the about and settings pages based on the
   * current route.
   */
  override currentRouteChanged(newRoute: Route) {
    const inAbout = routes.ABOUT.contains(Router.getInstance().currentRoute);
    this.showPages_ = {about: inAbout, settings: !inAbout};

    if (!newRoute.isSubpage()) {
      document.title = inAbout ? loadTimeData.getStringF(
                                     'settingsAltPageTitle',
                                     loadTimeData.getString('aboutPageTitle')) :
                                 loadTimeData.getString('settings');
    }
  }

  private onShowingSubpage_() {
    this.showingSubpage_ = true;
  }

  private onShowingMainPage_() {
    this.showingSubpage_ = false;
  }

  /**
   * A handler for the 'showing-section' event fired from os-settings-page,
   * indicating that a section should be scrolled into view as a result of a
   * navigation.
   */
  private onShowingSection_(e: CustomEvent<HTMLElement>) {
    const section = e.detail;
    // Calculate the height that the overscroll padding should be set to, so
    // that the given section is displayed at the top of the viewport.
    // Find the distance from the section's top to the overscroll.
    const sectionTop =
        (section.offsetParent as HTMLElement).offsetTop + section.offsetTop;
    const distance = this.$.overscroll.offsetTop - sectionTop;

    const overscroll = Math.max(0, this.offsetParent!.clientHeight - distance);
    this.setOverscroll_(overscroll);
    section.scrollIntoView();
    section.focus();
  }

  private showManagedHeader_(): boolean {
    return !this.showingSubpage_ && !this.showPages_.about;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-main': OsSettingsMainElement;
  }
}

customElements.define(OsSettingsMainElement.is, OsSettingsMainElement);
