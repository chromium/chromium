// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'os-settings-main' displays the selected settings page.
 */
import '/shared/settings/prefs/prefs.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/js/search_highlight_utils.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './managed_footnote.js';
import '../main_page_container/main_page_container.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertExists} from '../assert_extras.js';
import {RouteObserverMixin} from '../common/route_observer_mixin.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {isAboutRoute, Route} from '../router.js';

import {getTemplate} from './os_settings_main.html.js';

declare global {
  interface HTMLElementEventMap {
    'showing-main-page': CustomEvent;
    'showing-subpage': CustomEvent;
    'showing-section': CustomEvent<HTMLElement>;
  }
}

export interface OsSettingsMainElement {
  $: {
    overscroll: HTMLDivElement,
  };
}

const OsSettingsMainElementBase = RouteObserverMixin(PolymerElement);

export class OsSettingsMainElement extends OsSettingsMainElementBase {
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
       * When OsSettingsRevampWayfinding feature flag is disabled,
       * os-about-page and main-page-container are mututally exclusive. Only one
       * can be visible at a time.
       */
      isShowingAboutPage_: {
        type: Object,
        value: false,
      },

      isShowingSubpage_: Boolean,

      toolbarSpinnerActive: {
        type: Boolean,
        value: false,
        notify: true,
      },

      /**
       * Dictionary defining page availability.
       */
      pageAvailability: Object,
    };
  }

  prefs: Object;
  advancedToggleExpanded: boolean;
  toolbarSpinnerActive: boolean;
  pageAvailability: OsPageAvailability;
  private overscroll_: number;
  private isShowingAboutPage_: boolean;
  private isShowingSubpage_: boolean;
  private boundScroll_: (() => void)|null;

  constructor() {
    super();

    this.boundScroll_ = null;
  }

  override ready(): void {
    super.ready();

    this.addEventListener('showing-main-page', this.onShowingMainPage);
    this.addEventListener('showing-subpage', this.onShowingSubpage);
    this.addEventListener('showing-section', this.onShowingSection);
  }

  private overscrollChanged_(): void {
    assertExists(this.offsetParent);

    if (!this.overscroll_ && this.boundScroll_) {
      this.offsetParent.removeEventListener('scroll', this.boundScroll_);
      window.removeEventListener('resize', this.boundScroll_);
      this.boundScroll_ = null;
    } else if (this.overscroll_ && !this.boundScroll_) {
      this.boundScroll_ = () => {
        if (!this.isShowingSubpage_) {
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
  private setOverscroll_(minHeight?: number): void {
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
  override currentRouteChanged(newRoute: Route): void {
    const inAbout = isAboutRoute(newRoute);
    this.isShowingAboutPage_ = inAbout;

    if (!newRoute.isSubpage()) {
      document.title = inAbout ? loadTimeData.getStringF(
                                     'settingsAltPageTitle',
                                     loadTimeData.getString('aboutPageTitle')) :
                                 loadTimeData.getString('settings');
    }
  }

  private onShowingMainPage(): void {
    this.isShowingSubpage_ = false;
  }

  private onShowingSubpage(): void {
    this.isShowingSubpage_ = true;
  }

  /**
   * A handler for the 'showing-section' event fired from
   * main-page-container, indicating that a section should be
   * scrolled into view as a result of a navigation.
   */
  private onShowingSection(e: CustomEvent<HTMLElement>): void {
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
    return !this.isShowingSubpage_ && !this.isShowingAboutPage_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'os-settings-main': OsSettingsMainElement;
  }
}

customElements.define(OsSettingsMainElement.is, OsSettingsMainElement);
