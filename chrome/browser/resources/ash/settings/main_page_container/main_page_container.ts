// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'main-page-container' is the container hosting all the top-level pages.
 */

/**
 * All top-level basic pages should be imported below.
 */
// clang-format off
import '../device_page/device_page.js';
import '../internet_page/internet_page.js';
import '../kerberos_page/kerberos_page.js';
import '../multidevice_page/multidevice_page.js';
import '../os_a11y_page/os_a11y_page.js';
import '../os_about_page/os_about_page.js';
import '../os_apps_page/os_apps_page.js';
import '../os_bluetooth_page/os_bluetooth_page.js';
import '../os_people_page/os_people_page.js';
import '../os_privacy_page/os_privacy_page.js';
import '../personalization_page/personalization_page.js';
import '../system_preferences_page/system_preferences_page.js';
// clang-format on

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/ash/common/cr_elements/icons.html.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../os_languages_page/languages.js';
import '../os_settings_icons.html.js';
import './page_displayer.js';

import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {PrefsState} from '../common/types.js';
import {Section} from '../mojom-webui/routes.mojom-webui.js';
import {AboutPageBrowserProxyImpl} from '../os_about_page/about_page_browser_proxy.js';
import type {AndroidAppsInfo} from '../os_apps_page/android_apps_browser_proxy.js';
import {AndroidAppsBrowserProxyImpl} from '../os_apps_page/android_apps_browser_proxy.js';
import type {LanguageHelper, LanguagesModel} from '../os_languages_page/languages_types.js';
import type {OsPageAvailability} from '../os_page_availability.js';
import type {Route} from '../router.js';

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
      SectionEnum_: {
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
  androidAppsInfo?: AndroidAppsInfo;
  pageAvailability: OsPageAvailability;

  // Languages data and API
  private languages_: LanguagesModel|undefined;
  private languageHelper_: LanguageHelper|undefined;

  private isShowingSubpage_: boolean;
  private showSecondaryUserBanner_: boolean;
  private showUpdateRequiredEolBanner_: boolean;

  override ready(): void {
    super.ready();

    this.setAttribute('role', 'main');
    this.addEventListener('showing-subpage', this.onShowingSubpage);
  }

  override connectedCallback(): void {
    super.connectedCallback();

    this.addWebUiListener(
        'android-apps-info-update', this.androidAppsInfoUpdate_.bind(this));
    AndroidAppsBrowserProxyImpl.getInstance().requestAndroidAppsInfo();

    AboutPageBrowserProxyImpl.getInstance().pageReady();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route): void {
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
    return !this.isShowingSubpage_ && this.showUpdateRequiredEolBanner_;
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
   * @param opened Whether the menu is expanded.
   * @return Icon name.
   */
  private getArrowIcon_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [MainPageContainerElement.is]: MainPageContainerElement;
  }
}

customElements.define(MainPageContainerElement.is, MainPageContainerElement);
