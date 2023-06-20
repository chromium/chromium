// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'main-page-container' is the container hosting all the
 * main (top-level) pages, including advanced pages.
 */

/**
 * All top-level basic pages should be imported below. Top-level advanced pages
 * should be imported in lazy_load.ts instead.
 */
// clang-format off
import '../device_page/device_page.js';
import '../internet_page/internet_page.js';
import '../kerberos_page/kerberos_page.js';
import '../multidevice_page/multidevice_page.js';
import '../os_a11y_page/os_a11y_page.js';
import '../os_apps_page/os_apps_page.js';
import '../os_bluetooth_page/os_bluetooth_page.js';
import '../os_people_page/os_people_page.js';
import '../os_privacy_page/os_privacy_page.js';
import '../os_search_page/os_search_page.js';
import '../personalization_page/personalization_page.js';
// clang-format on

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_settings_page/settings_idle_load.js';
import '../os_about_page/eol_offer_section.js';
import '../os_settings_icons.html.js';
import './main_page_container_styles.css.js';
import './page_displayer.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {beforeNextRender, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {MainPageMixin} from '../main_page_mixin.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {AboutPageBrowserProxyImpl} from '../os_about_page/about_page_browser_proxy.js';
import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from '../os_apps_page/android_apps_browser_proxy.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {isAdvancedRoute, isBasicRoute, Route, Router} from '../router.js';

import {getTemplate} from './main_page_container.html.js';

const MainPageContainerElementBase =
    MainPageMixin(WebUiListenerMixin(PolymerElement));

export class MainPageContainerElement extends MainPageContainerElementBase {
  static get is() {
    return 'main-page-container' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /** Preferences state. */
      prefs: {
        type: Object,
        notify: true,
      },

      /** Mirror Section enum to be used in Polymer data bindings. */
      Section: {
        type: Object,
        value: Section,
      },

      androidAppsInfo: Object,

      /**
       * Dictionary defining page availability.
       */
      pageAvailability: {
        type: Object,
      },

      advancedToggleExpanded: {
        type: Boolean,
        value: false,
        notify: true,
        observer: 'advancedToggleExpandedChanged_',
      },

      /**
       * True if a section is fully expanded to hide other sections beneath it.
       * False otherwise (even while animating a section open/closed).
       */
      hasExpandedSection_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user is a secondary user. Computed so that it is calculated
       * correctly after loadTimeData is available.
       */
      showSecondaryUserBanner_: {
        type: Boolean,
        computed: 'computeShowSecondaryUserBanner_(hasExpandedSection_)',
      },

      /**
       * Whether to show banner indicating the user to return this device as an
       * update is required as per policy but the device has reached end of
       * life.
       */
      showUpdateRequiredEolBanner_: {
        type: Boolean,
        value: !!loadTimeData.getString('updateRequiredEolBannerText'),
      },

      currentRoute_: Object,

      showEolIncentive_: {
        type: Boolean,
        value: false,
      },

      shouldShowOfferText_: {
        type: Boolean,
        value: false,
      },

      isRevampWayfindingEnabled_: {
        type: Boolean,
        value: () => {
          return isRevampWayfindingEnabled();
        },
      },

      shouldShowAdvancedToggle_: {
        type: Boolean,
        computed: 'computeShouldShowAdvancedToggle_(' +
            'isRevampWayfindingEnabled_, hasExpandedSection_)',
      },
    };
  }

  androidAppsInfo?: AndroidAppsInfo;
  pageAvailability: OsPageAvailability;
  advancedToggleExpanded: boolean;
  private hasExpandedSection_: boolean;
  private showSecondaryUserBanner_: boolean;
  private showUpdateRequiredEolBanner_: boolean;
  private currentRoute_: Route;
  /**
   * Used to avoid handling a new toggle while currently toggling.
   */
  private advancedTogglingInProgress_: boolean;
  private showEolIncentive_: boolean;
  private shouldShowOfferText_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private shouldShowAdvancedToggle_: boolean;

  constructor() {
    super();
    this.advancedTogglingInProgress_ = false;
  }

  override ready() {
    super.ready();

    this.setAttribute('role', 'main');
    this.addEventListener('subpage-expand', this.onSubpageExpanded_);
  }

  override connectedCallback() {
    super.connectedCallback();

    this.currentRoute_ = Router.getInstance().currentRoute;

    this.addWebUiListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    AndroidAppsBrowserProxyImpl.getInstance().requestAndroidAppsInfo();

    AboutPageBrowserProxyImpl.getInstance().pageReady();
    AboutPageBrowserProxyImpl.getInstance().getEndOfLifeInfo().then(result => {
      this.showEolIncentive_ = !!result.shouldShowEndOfLifeIncentive;
      this.shouldShowOfferText_ = !!result.shouldShowOfferText;
    });
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    if (isAdvancedRoute(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute && oldRoute.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // hasExpandedSection_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section !== oldRoute.section) {
        this.hasExpandedSection_ = false;
      }
    } else {
      assert(!this.hasExpandedSection_);
    }

    // MainPageMixin#currentRouteChanged() should be the super class method
    super.currentRouteChanged(newRoute, oldRoute);
  }

  override containsRoute(route: Route|undefined) {
    return !route || isBasicRoute(route) || isAdvancedRoute(route);
  }

  /** Stamp page in the DOM depending on page availability */
  private shouldStampPage_(
      pageAvailability: OsPageAvailability, pageName: Section): boolean {
    return !!pageAvailability[pageName];
  }

  private computeShowSecondaryUserBanner_(): boolean {
    return !this.hasExpandedSection_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  }

  private computeShowUpdateRequiredEolBanner_(): boolean {
    return !this.hasExpandedSection_ && this.showUpdateRequiredEolBanner_ &&
        !this.showEolIncentive_;
  }

  private computeShowEolIncentive_(): boolean {
    return !this.hasExpandedSection_ && this.showEolIncentive_;
  }

  private computeShouldShowAdvancedToggle_(): boolean {
    return !this.isRevampWayfindingEnabled_ && !this.hasExpandedSection_;
  }

  private androidAppsInfoUpdate_(info: AndroidAppsInfo) {
    this.androidAppsInfo = info;
  }

  /**
   * Hides the update required EOL banner. It is shown again when Settings is
   * re-opened.
   */
  private onCloseEolBannerClicked_() {
    this.showUpdateRequiredEolBanner_ = false;
  }

  /**
   * Hides everything but the newly expanded subpage.
   */
  private onSubpageExpanded_() {
    this.hasExpandedSection_ = true;
  }

  /**
   * Render the advanced page now (don't wait for idle).
   */
  private advancedToggleExpandedChanged_() {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // beforeNextRender() must be used instead.
    beforeNextRender(this, () => {
      this.loadAdvancedPage();
    });
  }

  private advancedToggleClicked_() {
    if (this.advancedTogglingInProgress_) {
      return;
    }

    this.advancedTogglingInProgress_ = true;
    const toggle =
        castExists(this.shadowRoot!.getElementById('toggleContainer'));

    if (!this.advancedToggleExpanded) {
      this.advancedToggleExpanded = true;
      microTask.run(() => {
        this.loadAdvancedPage().then(() => {
          const event = new CustomEvent('scroll-to-top', {
            bubbles: true,
            composed: true,
            detail: {
              top: toggle.offsetTop,
              callback: () => {
                this.advancedTogglingInProgress_ = false;
              },
            },
          });
          this.dispatchEvent(event);
        });
      });
    } else {
      const event = new CustomEvent('scroll-to-bottom', {
        bubbles: true,
        composed: true,
        detail: {
          bottom: toggle.offsetTop + toggle.offsetHeight + 24,
          callback: () => {
            this.advancedToggleExpanded = false;
            this.advancedTogglingInProgress_ = false;
          },
        },
      });
      this.dispatchEvent(event);
    }
  }

  private showBasicPage_(): boolean {
    return !this.hasExpandedSection_ || isBasicRoute(this.currentRoute_);
  }

  private showAdvancedPage_(): boolean {
    if (this.hasExpandedSection_) {
      // Show the Advanced page only if the current route is an Advanced
      // subpage.
      return isAdvancedRoute(this.currentRoute_);
    }
    return this.advancedToggleExpanded;
  }

  /**
   * @param opened Whether the menu is expanded.
   * @return Icon name.
   */
  private getArrowIcon_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MainPageContainerElement.is]: MainPageContainerElement;
  }
}

customElements.define(MainPageContainerElement.is, MainPageContainerElement);
