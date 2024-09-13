// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-basic-page' is the settings page containing the actual settings.
 */
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../ai_page/ai_page.js';
import '../appearance_page/appearance_page.js';
import '../privacy_page/privacy_guide/privacy_guide_promo.js';
import '../privacy_page/privacy_page.js';
import '../safety_check_page/safety_check_page.js';
import '../safety_hub/safety_hub_entry_point.js';
import '../autofill_page/autofill_page.js';
import '../controls/settings_idle_load.js';
import '../on_startup_page/on_startup_page.js';
import '../people_page/people_page.js';
import '../performance_page/battery_page.js';
import '../performance_page/memory_page.js';
import '../performance_page/performance_page.js';
import '../performance_page/speed_page.js';
import '../reset_page/reset_profile_banner.js';
import '../search_page/search_page.js';
import '../settings_page/settings_section.js';
import '../settings_page_styles.css.js';
// <if expr="not is_chromeos">
import '../default_browser_page/default_browser_page.js';
// </if>
// <if expr="not chromeos_ash">
import '../languages_page/languages.js';

// </if>

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format off
// <if expr="chromeos_ash">
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
// </if>
// clang-format on



import type {SettingsIdleLoadElement} from '../controls/settings_idle_load.js';
import {loadTimeData} from '../i18n_setup.js';
// <if expr="not chromeos_ash">
import type {LanguageHelper, LanguagesModel} from '../languages_page/languages_types.js';
// </if>
import type {PageVisibility} from '../page_visibility.js';
import type {PerformanceBrowserProxy} from '../performance_page/performance_browser_proxy.js';
import {PerformanceBrowserProxyImpl, PerformanceFeedbackCategory} from '../performance_page/performance_browser_proxy.js';
import {PrivacyGuideAvailabilityMixin} from '../privacy_page/privacy_guide/privacy_guide_availability_mixin.js';
import type {PrivacyGuideBrowserProxy} from '../privacy_page/privacy_guide/privacy_guide_browser_proxy.js';
import {MAX_PRIVACY_GUIDE_PROMO_IMPRESSION, PrivacyGuideBrowserProxyImpl} from '../privacy_page/privacy_guide/privacy_guide_browser_proxy.js';
import {routes} from '../route.js';
import type {Route} from '../router.js';
import {RouteObserverMixin, Router} from '../router.js';
import type {SearchResult} from '../search_settings.js';
import {getSearchManager} from '../search_settings.js';
import {MainPageMixin} from '../settings_page/main_page_mixin.js';

import {getTemplate} from './basic_page.html.js';

const SettingsBasicPageElementBase =
    PrefsMixin(MainPageMixin(RouteObserverMixin(PrivacyGuideAvailabilityMixin(
        WebUiListenerMixin(I18nMixin(PolymerElement))))));

export class SettingsBasicPageElement extends SettingsBasicPageElementBase {
  static get is() {
    return 'settings-basic-page';
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

      // <if expr="not chromeos_ash">
      /**
       * Read-only reference to the languages model provided by the
       * 'settings-languages' instance.
       */
      languages: {
        type: Object,
        notify: true,
      },

      languageHelper: Object,
      // </if>

      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: {
        type: Object,
        value() {
          return {};
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
       * True if the basic page should currently display the reset profile
       * banner.
       */
      showResetProfileBanner_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showResetProfileBanner');
        },
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

      /**
       * Used to avoid handling a new toggle while currently toggling.
       */
      advancedTogglingInProgress_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      /**
       * Used to hide battery settings section if the device has no battery
       */
      showBatterySettings_: {
        type: Boolean,
        value: false,
      },

      showAdvancedFeaturesMainControl_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('showAdvancedFeaturesMainControl'),
      },
    };
  }

  static get observers() {
    return [
      'updatePrivacyGuidePromoVisibility_(isPrivacyGuideAvailable, prefs.privacy_guide.viewed.value)',
    ];
  }

  // <if expr="not chromeos_ash">
  languages?: LanguagesModel;
  languageHelper: LanguageHelper;
  // </if>
  pageVisibility: PageVisibility;
  inSearchMode: boolean;
  private showResetProfileBanner_: boolean;

  private currentRoute_: Route;
  private advancedTogglingInProgress_: boolean;
  private showBatterySettings_: boolean;
  private showAdvancedFeaturesMainControl_: boolean;

  private showPrivacyGuidePromo_: boolean;
  private privacyGuidePromoWasShown_: boolean;
  private privacyGuideBrowserProxy_: PrivacyGuideBrowserProxy =
      PrivacyGuideBrowserProxyImpl.getInstance();
  private performanceBrowserProxy_: PerformanceBrowserProxy =
      PerformanceBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.setAttribute('role', 'main');
  }


  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'device-has-battery-changed',
        this.onDeviceHasBatteryChanged_.bind(this));
    this.performanceBrowserProxy_.getDeviceHasBattery().then(
        this.onDeviceHasBatteryChanged_.bind(this));

    this.currentRoute_ = Router.getInstance().getCurrentRoute();
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    this.currentRoute_ = newRoute;

    if (routes.ADVANCED && routes.ADVANCED.contains(newRoute)) {
      // Render the advanced page now (don't wait for idle).
      // In Polymer3, async() does not wait long enough for layout to complete.
      // beforeNextRender() must be used instead.
      beforeNextRender(this, () => {
        this.getIdleLoad_();
      });
    }

    super.currentRouteChanged(newRoute, oldRoute);
    if (newRoute === routes.PRIVACY) {
      this.updatePrivacyGuidePromoVisibility_();
    }
  }

  /** Overrides MainPageMixin method. */
  override containsRoute(route: Route|null): boolean {
    return !route || routes.BASIC.contains(route) ||
        (routes.ADVANCED && routes.ADVANCED.contains(route));
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private getIdleLoad_(): Promise<Element> {
    const idleLoad = this.shadowRoot!.querySelector<SettingsIdleLoadElement>(
        '#advancedPageTemplate');
    assert(idleLoad);
    return idleLoad.get();
  }

  private updatePrivacyGuidePromoVisibility_() {
    if (!this.isPrivacyGuideAvailable ||
        this.pageVisibility.privacy === false || this.prefs === undefined ||
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

  private onDeviceHasBatteryChanged_(deviceHasBattery: boolean) {
    this.showBatterySettings_ = deviceHasBattery;
  }

  /**
   * Queues a task to search the basic sections, then another for the advanced
   * sections.
   * @param query The text to search for.
   * @return A signal indicating that searching finished.
   */
  searchContents(query: string): Promise<SearchResult> {
    const basicPage = this.shadowRoot!.querySelector<HTMLElement>('#basicPage');
    assert(basicPage);
    const whenSearchDone = [
      getSearchManager().search(query, basicPage),
    ];

    if (this.pageVisibility.advancedSettings !== false) {
      whenSearchDone.push(this.getIdleLoad_().then(function(advancedPage) {
        return getSearchManager().search(query, advancedPage);
      }));
    }

    return Promise.all(whenSearchDone).then(function(requests) {
      // Combine the SearchRequests results to a single SearchResult object.
      return {
        canceled: requests.some(function(r) {
          return r.canceled;
        }),
        didFindMatches: requests.some(function(r) {
          return r.didFindMatches();
        }),
        // All requests correspond to the same user query, so only need to check
        // one of them.
        wasClearSearch: requests[0].isSame(''),
      };
    });
  }

  // <if expr="chromeos_ash">
  private onOpenChromeOsLanguagesSettingsClick_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('osSettingsLanguagesPageUrl'));
  }
  // </if>

  private onResetProfileBannerClosed_() {
    this.showResetProfileBanner_ = false;
  }

  private fire_(eventName: string, detail: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  /**
   * @return Whether to show #basicPage. This is an optimization to lazy render
   *     #basicPage only when a section/subpage within it is being shown, or
   *     when in search mode.
   */
  private showBasicPage_(): boolean {
    if (this.currentRoute_ === undefined) {
      return false;
    }

    return this.inSearchMode || routes.BASIC.contains(this.currentRoute_);
  }

  private showAdvancedSettings_(visibility?: boolean): boolean {
    return this.showPage_(visibility);
  }

  private showSafetyCheckPage_(visibility?: boolean): boolean {
    return !loadTimeData.getBoolean('enableSafetyHub') &&
        this.showPage_(visibility);
  }

  private showSafetyHubEntryPointPage_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('enableSafetyHub') &&
        this.showPage_(visibility);
  }

  private showAiInfoCard_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('enableAiSettingsPageRefresh') &&
        this.showExperimentalAdvancedPage_(visibility);
  }

  private showExperimentalAdvancedPage_(visibility?: boolean): boolean {
    return loadTimeData.getBoolean('showAdvancedFeaturesMainControl') &&
        this.showPage_(visibility);
  }

  // <if expr="_google_chrome">
  private onSendPerformanceFeedbackClick_(e: Event) {
    e.stopPropagation();
    this.performanceBrowserProxy_.openFeedbackDialog(
        PerformanceFeedbackCategory.NOTIFICATIONS);
  }

  private onSendMemorySaverFeedbackClick_(e: Event) {
    e.stopPropagation();
    this.performanceBrowserProxy_.openFeedbackDialog(
        PerformanceFeedbackCategory.TABS);
  }

  private onSendBatterySaverFeedbackClick_(e: Event) {
    e.stopPropagation();
    this.performanceBrowserProxy_.openFeedbackDialog(
        PerformanceFeedbackCategory.BATTERY);
  }

  private onSendSpeedFeedbackClick_(e: Event) {
    e.stopPropagation();
    this.performanceBrowserProxy_.openFeedbackDialog(
        PerformanceFeedbackCategory.SPEED);
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-basic-page': SettingsBasicPageElement;
  }
}

customElements.define(SettingsBasicPageElement.is, SettingsBasicPageElement);
