// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import '/shared/settings/prefs/prefs.js';
import '../basic_page/basic_page.js';
import '../safety_hub/safety_hub_entry_point.js';
import '../settings_page/settings_section.js';
import '../settings_shared.css.js';
import './privacy_guide/privacy_guide_promo.js';
import './privacy_page.js';

import {PrefsMixin} from '/shared/settings/prefs/prefs_mixin.js';
import type {CrViewManagerElement} from 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {beforeNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {pageVisibility} from '../page_visibility.js';
import type {PageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {RouteObserverMixin} from '../router.js';
import type {Route, SettingsRoutes} from '../router.js';
import type {SettingsPlugin} from '../settings_main/settings_plugin.js';
import {SearchableViewContainerMixin} from '../settings_page/searchable_view_container_mixin.js';

import {PrivacyGuideAvailabilityMixin} from './privacy_guide/privacy_guide_availability_mixin.js';
import type {PrivacyGuideBrowserProxy} from './privacy_guide/privacy_guide_browser_proxy.js';
import {MAX_PRIVACY_GUIDE_PROMO_IMPRESSION, PrivacyGuideBrowserProxyImpl} from './privacy_guide/privacy_guide_browser_proxy.js';
import {getTemplate} from './privacy_page_index.html.js';

// clang-format off
// <if expr="is_chromeos">
import {getTopLevelRoute} from '../route.js';
// </if>
// clang-format on


export interface SettingsPrivacyPageIndexElement {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const SettingsPrivacyPageIndexElementBase =
    SearchableViewContainerMixin(PrivacyGuideAvailabilityMixin(
        PrefsMixin(RouteObserverMixin(PolymerElement))));

export class SettingsPrivacyPageIndexElement extends
    SettingsPrivacyPageIndexElementBase implements SettingsPlugin {
  static get is() {
    return 'settings-privacy-page-index';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      prefs: Object,

      pageVisibility_: {
        type: Object,
        value() {
          return pageVisibility || {};
        },
      },

      routes_: {
        type: Object,
        value: () => routes,
      },

      showPrivacyGuidePromo_: {
        type: Boolean,
        value: false,
      },
    };
  }

  static get observers() {
    return [
      'updatePrivacyGuidePromoVisibility_(isPrivacyGuideAvailable, prefs.privacy_guide.viewed.value)',
    ];
  }

  declare prefs: {[key: string]: any};
  declare private pageVisibility_: PageVisibility;
  declare private routes_: SettingsRoutes;
  declare private showPrivacyGuidePromo_: boolean;

  private pendingViewSwitching_: PromiseResolver<void> = new PromiseResolver();
  private privacyGuidePromoWasShown_: boolean;
  private privacyGuideBrowserProxy_: PrivacyGuideBrowserProxy =
      PrivacyGuideBrowserProxyImpl.getInstance();

  private beforeNextRenderPromise_(): Promise<void> {
    return new Promise(res => {
      beforeNextRender(this, res);
    });
  }

  private getDefaultViews_(): string[] {
    const defaultViews = ['old'];

    if (this.isPrivacyGuideAvailable) {
      defaultViews.push('privacyGuidePromo');
    }

    if (this.showPage_(this.pageVisibility_.safetyHub)) {
      defaultViews.push('safetyHubEntryPoint');
    }

    return defaultViews;
  }

  private isNonMigratedPrivacyRoute_(route: Route): boolean {
    return routes.PRIVACY.contains(route) && !route.hasMigratedToPlugin;
  }

  private getViewIdsForRoute_(route: Route): string[] {
    switch (route) {
      case routes.PRIVACY:
        return this.getDefaultViews_();
      case routes.BASIC:
        // <if expr="is_chromeos">
        if (getTopLevelRoute() === routes.PRIVACY) {
          // On CrOS guest mode the "Privacy" section should be displayed when
          // on chrome://settings/.
          return this.getDefaultViews_();
        }
        // </if>

        // Display the default views if in search mode, since they could be part
        // of search results.
        return this.inSearchMode ? this.getDefaultViews_() : [];
      default: {
        if (this.isNonMigratedPrivacyRoute_(route)) {
          // Handle case where Privacy child route has not migrated to the new
          // architecture.
          return ['old'];
        }

        // Nothing to do. Other parent elements are responsible for updating
        // the displayed contents.
        return [];
      }
    }
  }

  override currentRouteChanged(newRoute: Route, oldRoute?: Route) {
    super.currentRouteChanged(newRoute, oldRoute);

    if (newRoute === routes.PRIVACY) {
      this.updatePrivacyGuidePromoVisibility_();
    }

    this.pendingViewSwitching_ = new PromiseResolver();

    // Need to wait for currentRouteChanged observers on child views to run
    // first, before switching views.
    queueMicrotask(async () => {
      const viewIds = this.getViewIdsForRoute_(newRoute);

      if (viewIds.length === 0) {
        // Nothing to do. Other parent elements are responsible for updating
        // the displayed contents.
        this.pendingViewSwitching_.resolve();
        return;
      }

      const allViewsExist =
          this.$.viewManager.querySelectorAll(viewIds.join(',')).length ===
          viewIds.length;
      if (!allViewsExist) {
        // Wait once for any lazy render <dom-if>s to render.
        await this.beforeNextRenderPromise_();

        if (this.currentRoute !== newRoute || !this.isConnected) {
          // A newer currentRouteChanged call happened while awaiting or no
          // longer connected (both can happen in tests). Do nothing.
          this.pendingViewSwitching_.resolve();
          return;
        }
      }

      await this.$.viewManager.switchViews(
          viewIds, 'no-animation', 'no-animation');
      this.pendingViewSwitching_.resolve();
    });
  }

  // Exposed for tests, to allow making visibility assertions about
  // cr-view-manager views without flaking. Should be called after
  // currentRouteChanged is called.
  whenViewSwitchingDone(): Promise<void> {
    return this.pendingViewSwitching_.promise;
  }

  private showPage_(visibility?: boolean): boolean {
    return visibility !== false;
  }

  private renderView_(route: Route): boolean {
    return this.inSearchMode ||
        (!!this.currentRoute && this.currentRoute === route);
  }

  private renderOldView_(): boolean {
    // <if expr="is_chromeos">
    if (getTopLevelRoute() === routes.PRIVACY &&
        this.currentRoute === routes.BASIC) {
      // On CrOS guest mode the "Privacy" section should be displayed when
      // on chrome://settings/.
      return true;
    }
    // </if>

    return this.inSearchMode ||
        (!!this.currentRoute &&
         this.isNonMigratedPrivacyRoute_(this.currentRoute));
  }

  private updatePrivacyGuidePromoVisibility_() {
    if (!this.isPrivacyGuideAvailable || this.prefs === undefined ||
        this.getPref('privacy_guide.viewed').value ||
        this.privacyGuideBrowserProxy_.getPromoImpressionCount() >=
            MAX_PRIVACY_GUIDE_PROMO_IMPRESSION ||
        this.currentRoute !== routes.PRIVACY) {
      this.showPrivacyGuidePromo_ = false;
      return;
    }
    this.showPrivacyGuidePromo_ = true;
    if (!this.privacyGuidePromoWasShown_) {
      this.privacyGuideBrowserProxy_.incrementPromoImpressionCount();
      this.privacyGuidePromoWasShown_ = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-privacy-page-index': SettingsPrivacyPageIndexElement;
  }
}

customElements.define(
    SettingsPrivacyPageIndexElement.is, SettingsPrivacyPageIndexElement);
