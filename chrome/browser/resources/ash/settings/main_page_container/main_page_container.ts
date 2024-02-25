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
import '../system_preferences_page/system_preferences_page.js';
// clang-format on

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_about_page/eol_offer_section.js';
import '../os_languages_page/languages.js';
import '../os_settings_icons.html.js';
import '../os_settings_page/settings_idle_load.js';
import './page_displayer.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {beforeNextRender, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {castExists} from '../assert_extras.js';
import {isRevampWayfindingEnabled} from '../common/load_time_booleans.js';
import {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {AboutPageBrowserProxyImpl} from '../os_about_page/about_page_browser_proxy.js';
import {AndroidAppsBrowserProxyImpl, AndroidAppsInfo} from '../os_apps_page/android_apps_browser_proxy.js';
import {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import {OsPageAvailability} from '../os_page_availability.js';
import {isAboutRoute, isAdvancedRoute, isBasicRoute, Route, Router} from '../router.js';

import {getTemplate} from './main_page_container.html.js';
import {MainPageMixin} from './main_page_mixin.js';

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
      isShowingSubpage_: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether the user is a secondary user. Computed so that it is calculated
       * correctly after loadTimeData is available.
       */
      showSecondaryUserBanner_: {
        type: Boolean,
        computed: 'computeShowSecondaryUserBanner_(isShowingSubpage_)',
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

      currentRoute_: {
        type: Object,
        value: null,
      },

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

      shouldShowBasicPageContainer_: {
        type: Boolean,
        computed: 'computeShouldShowBasicPageContainer_(' +
            'currentRoute_, isShowingSubpage_, isRevampWayfindingEnabled_)',
      },

      shouldShowAdvancedPageContainer_: {
        type: Boolean,
        computed: 'computeShouldShowAdvancedPageContainer(' +
            'advancedToggleExpanded, currentRoute_, isShowingSubpage_, ' +
            'isRevampWayfindingEnabled_)',
      },

      shouldShowAdvancedToggle_: {
        type: Boolean,
        computed: 'computeShouldShowAdvancedToggle(' +
            'currentRoute_, isShowingSubpage_, isRevampWayfindingEnabled_)',
      },

      shouldShowAboutPageContainer_: {
        type: Boolean,
        computed: 'computeShouldShowAboutPageContainer(' +
            'currentRoute_, isRevampWayfindingEnabled_)',
      },

      /**
       * This is used to cache the set of languages from <settings-languages>
       * via bi-directional data-binding.
       */
      languages_: Object,

      /**
       * This is used to cache the language helper API from <settings-languages>
       * via bi-directional data-binding.
       */
      languageHelper_: Object,
    };
  }

  prefs: PrefsState;
  advancedToggleExpanded: boolean;
  androidAppsInfo?: AndroidAppsInfo;
  pageAvailability: OsPageAvailability;

  // Languages data and API
  private languages_: LanguagesModel|undefined;
  private languageHelper_: LanguageHelper|undefined;

  private isShowingSubpage_: boolean;
  private showSecondaryUserBanner_: boolean;
  private showUpdateRequiredEolBanner_: boolean;
  private currentRoute_: Route|null;
  /**
   * Used to avoid handling a new toggle while currently toggling.
   */
  private advancedTogglingInProgress_: boolean;
  private showEolIncentive_: boolean;
  private shouldShowOfferText_: boolean;
  private isRevampWayfindingEnabled_: boolean;
  private shouldShowBasicPageContainer_: boolean;
  private shouldShowAdvancedPageContainer_: boolean;
  private shouldShowAdvancedToggle_: boolean;
  private shouldShowAboutPageContainer_: boolean;

  constructor() {
    super();
    this.advancedTogglingInProgress_ = false;
  }

  override ready(): void {
    super.ready();

    this.setAttribute('role', 'main');
    this.addEventListener('showing-subpage', this.onShowingSubpage);
  }

  override connectedCallback(): void {
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

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
    this.currentRoute_ = newRoute;

    if (isAdvancedRoute(newRoute)) {
      this.advancedToggleExpanded = true;
    }

    if (oldRoute?.isSubpage()) {
      // If the new route isn't the same expanded section, reset
      // isShowingSubpage_ for the next transition.
      if (!newRoute.isSubpage() || newRoute.section !== oldRoute.section) {
        this.isShowingSubpage_ = false;
      }
    } else {
      assert(!this.isShowingSubpage_);
    }

    // MainPageMixin#currentRouteChanged() should be the super class method
    super.currentRouteChanged(newRoute, oldRoute);
  }

  override containsRoute(_route: Route|undefined): boolean {
    // All routes are contained under this element.
    return true;
  }

  /** Stamp page in the DOM depending on page availability */
  private shouldStampPage_(
      pageAvailability: OsPageAvailability, pageName: Section): boolean {
    return !!pageAvailability[pageName];
  }

  private computeShowSecondaryUserBanner_(): boolean {
    return !this.isShowingSubpage_ &&
        loadTimeData.getBoolean('isSecondaryUser');
  }

  private computeShowUpdateRequiredEolBanner_(): boolean {
    return !this.isShowingSubpage_ && this.showUpdateRequiredEolBanner_ &&
        !this.showEolIncentive_;
  }

  private computeShowEolIncentive_(): boolean {
    return !this.isShowingSubpage_ && this.showEolIncentive_;
  }

  private androidAppsInfoUpdate_(info: AndroidAppsInfo): void {
    this.androidAppsInfo = info;
  }

  /**
   * Hides the update required EOL banner. It is shown again when Settings is
   * re-opened.
   */
  private onCloseEolBannerClicked_(): void {
    this.showUpdateRequiredEolBanner_ = false;
  }

  private onShowingSubpage(): void {
    this.isShowingSubpage_ = true;
  }

  /**
   * Render the advanced page now (don't wait for idle).
   */
  private advancedToggleExpandedChanged_(): void {
    if (!this.advancedToggleExpanded) {
      return;
    }

    // In Polymer2, async() does not wait long enough for layout to complete.
    // beforeNextRender() must be used instead.
    beforeNextRender(this, () => {
      this.loadAdvancedPage();
    });
  }

  private advancedToggleClicked_(): void {
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

  private computeShouldShowBasicPageContainer_(): boolean {
    if (this.isRevampWayfindingEnabled_) {
      return isBasicRoute(this.currentRoute_);
    }

    // When infinite scroll exists, never show when the about page is visible.
    if (isAboutRoute(this.currentRoute_)) {
      return false;
    }

    // Show if:
    // 1. On the main page (not a subpage)
    // 2. OR if the current subpage exists within the basic page
    if (this.isShowingSubpage_) {
      return isBasicRoute(this.currentRoute_);
    }
    return true;
  }

  private computeShouldShowAdvancedPageContainer(): boolean {
    if (this.isRevampWayfindingEnabled_) {
      return isAdvancedRoute(this.currentRoute_);
    }

    // When infinite scroll exists, never show when the about page is visible.
    if (isAboutRoute(this.currentRoute_)) {
      return false;
    }

    // Show if:
    // 1. On the main page and the advanced toggle is expanded
    // 2. OR if the current subpage exists within the advanced page
    if (this.isShowingSubpage_) {
      return isAdvancedRoute(this.currentRoute_);
    }
    return this.advancedToggleExpanded;
  }

  private computeShouldShowAdvancedToggle(): boolean {
    if (this.isRevampWayfindingEnabled_) {
      // Under the Settings Revamp, the advanced toggle should never show.
      return false;
    }

    // When infinite scroll exists, never show when the about page is visible.
    if (isAboutRoute(this.currentRoute_)) {
      return false;
    }

    // Only show if on the main page (not a subpage)
    return !this.isShowingSubpage_;
  }

  private computeShouldShowAboutPageContainer(): boolean {
    // Only show if the current route exists within the about page
    return isAboutRoute(this.currentRoute_);
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
