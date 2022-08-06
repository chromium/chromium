// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLazyRenderElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {IronCollapseElement} from 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import {DomRepeatEvent, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin, BaseMixinInterface} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {AllSitesAction2, SortMethod} from './constants.js';
import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl} from './local_data_browser_proxy.js';
import {getTemplate} from './site_entry.html.js';
import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';
import {OriginInfo, SiteGroup} from './site_settings_prefs_browser_proxy.js';


export interface SiteEntryElement {
  $: {
    expandIcon: CrIconButtonElement,
    collapseParent: HTMLElement,
    cookies: HTMLElement,
    displayName: HTMLElement,
    originList: CrLazyRenderElement<IronCollapseElement>,
    toggleButton: HTMLElement,
  };
}

const SiteEntryElementBase =
    mixinBehaviors(
        [FocusRowBehavior],
        BaseMixin(SiteSettingsMixin(I18nMixin(PolymerElement)))) as {
      new (): PolymerElement & I18nMixinInterface & SiteSettingsMixinInterface &
          BaseMixinInterface,
    };

export class SiteEntryElement extends SiteEntryElementBase {
  static get is() {
    return 'site-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * An object representing a group of sites with the same eTLD+1.
       */
      siteGroup: {
        type: Object,
        observer: 'onSiteGroupChanged_',
      },

      /**
       * The name to display beside the icon. If grouped_() is true, it will be
       * the eTLD+1 for all the origins, otherwise, it will return the host.
       */
      displayName_: String,

      /**
       * The string to display when there is a non-zero number of cookies.
       */
      cookieString_: String,

      /**
       * The position of this site-entry in its parent list.
       */
      listIndex: {
        type: Number,
        value: -1,
      },

      /**
       * The string to display showing the overall usage of this site-entry.
       */
      overallUsageString_: String,

      /**
       * An array containing the strings to display showing the individual disk
       * usage for each origin in |siteGroup|.
       */
      originUsages_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * An array containing the strings to display showing the individual
       * cookies number for each origin in |siteGroup|.
       */
      cookiesNum_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The selected sort method.
       */
      sortMethod: {type: String, observer: 'updateOrigins_'},

      enableConsolidatedSiteStorageControls_: {
        type: Boolean,
        value: () =>
            loadTimeData.getBoolean('consolidatedSiteStorageControlsEnabled'),
      },
    };
  }

  siteGroup: SiteGroup;
  private displayName_: string;
  private cookieString_: string;
  listIndex: number;
  private overallUsageString_: string;
  private originUsages_: string[];
  private cookiesNum_: string[];
  sortMethod?: SortMethod;
  private enableConsolidatedSiteStorageControls_: boolean;

  private button_: Element|null = null;
  private localDataBrowserProxy_: LocalDataBrowserProxy =
      LocalDataBrowserProxyImpl.getInstance();
  private eventTracker_: EventTracker = new EventTracker();

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.button_) {
      this.eventTracker_.remove(this.button_, 'keydown');
    }
  }

  private onButtonKeydown_(e: KeyboardEvent) {
    if (e.shiftKey && e.key === 'Tab') {
      this.focus();
    }
  }

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   * @param siteGroup The eTLD+1 group of origins.
   */
  private grouped_(siteGroup: SiteGroup): boolean {
    if (!siteGroup) {
      return false;
    }
    if (siteGroup.origins.length > 1 ||
        siteGroup.numCookies > siteGroup.origins[0].numCookies ||
        siteGroup.origins.some(o => o.isPartitioned)) {
      return true;
    }
    return false;
  }

  /**
   * Returns a user-friendly name for the siteGroup.
   * If grouped_() is true and eTLD+1 is available, returns the eTLD+1,
   * otherwise return the origin representation for the first origin.
   * @param siteGroup The eTLD+1 group of origins.
   * @return The user-friendly name.
   */
  private siteGroupRepresentation_(siteGroup: SiteGroup): string {
    if (!siteGroup) {
      return '';
    }
    if (this.grouped_(siteGroup)) {
      if (siteGroup.etldPlus1 !== '') {
        return siteGroup.etldPlus1;
      }
      // Fall back onto using the host of the first origin, if no eTLD+1 name
      // was computed.
    }
    return this.originRepresentation(siteGroup.origins[0].origin);
  }

  /**
   * @param siteGroup The eTLD+1 group of origins.
   */
  private onSiteGroupChanged_(siteGroup: SiteGroup) {
    // Update the button listener.
    if (this.button_) {
      this.eventTracker_.remove(this.button_, 'keydown');
    }
    this.button_ =
        this.shadowRoot!.querySelector('#toggleButton *:not([hidden])');
    assert(this.button_);
    this.eventTracker_.add(
        this.button_, 'keydown',
        (e: KeyboardEvent) => this.onButtonKeydown_(e));

    if (!this.grouped_(siteGroup)) {
      // Ensure ungrouped |siteGroup|s do not get stuck in an opened state.
      const collapseChild = this.$.originList.getIfExists();
      if (collapseChild && collapseChild.opened) {
        this.toggleCollapsible_();
      }
    }
    if (!siteGroup) {
      return;
    }
    this.calculateUsageInfo_(siteGroup);
    this.getCookieNumString_(siteGroup.numCookies).then(string => {
      this.cookieString_ = string;
    });
    this.updateOrigins_(this.sortMethod);
    this.displayName_ = this.siteGroupRepresentation_(siteGroup);
  }

  /**
   * Returns any non-HTTPS scheme/protocol for the siteGroup that only contains
   * one origin. Otherwise, returns a empty string.
   * @param siteGroup The eTLD+1 group of origins.
   * @return The scheme if non-HTTPS, or empty string if HTTPS.
   */
  private siteGroupScheme_(siteGroup: SiteGroup): string {
    if (!siteGroup || (this.grouped_(siteGroup))) {
      return '';
    }
    return this.originScheme_(siteGroup.origins[0]);
  }

  /**
   * Returns any non-HTTPS scheme/protocol for the origin. Otherwise, returns
   * an empty string.
   * @return The scheme if non-HTTPS, or empty string if HTTPS.
   */
  private originScheme_(origin: OriginInfo): string {
    const url = this.toUrl(origin.origin)!;
    const scheme = url.protocol.replace(new RegExp(':*$'), '');
    const HTTPS_SCHEME = 'https';
    if (scheme === HTTPS_SCHEME) {
      return '';
    }
    return scheme;
  }

  /**
   * Get an appropriate favicon that represents this group of eTLD+1 sites as a
   * whole.
   * @param siteGroup The eTLD+1 group of origins.
   * @return URL that is used for fetching the favicon
   */
  private getSiteGroupIcon_(siteGroup: SiteGroup): string {
    const origins = siteGroup.origins;
    assert(origins);
    assert(origins.length >= 1);
    if (origins.length === 1) {
      return origins[0].origin;
    }
    // If we can find a origin with format "www.etld+1", use the favicon of this
    // origin. Otherwise find the origin with largest storage, and use the
    // number of cookies as a tie breaker.
    for (const originInfo of origins) {
      if (this.toUrl(originInfo.origin)!.host ===
          'www.' + siteGroup.etldPlus1) {
        return originInfo.origin;
      }
    }
    const getMaxStorage = (max: OriginInfo, originInfo: OriginInfo) => {
      return (
          max.usage > originInfo.usage ||
                  (max.usage === originInfo.usage &&
                   max.numCookies > originInfo.numCookies) ?
              max :
              originInfo);
    };
    return origins.reduce(getMaxStorage, origins[0]).origin;
  }

  /**
   * Calculates the amount of disk storage used by the given eTLD+1.
   * Also updates the corresponding display strings.
   * @param siteGroup The eTLD+1 group of origins.
   */
  private calculateUsageInfo_(siteGroup: SiteGroup) {
    let overallUsage = 0;
    siteGroup.origins.forEach(originInfo => {
      overallUsage += originInfo.usage;
    });
    this.browserProxy.getFormattedBytes(overallUsage).then(string => {
      this.overallUsageString_ = string;
    });
  }

  /**
   * Get display string for number of cookies.
   */
  private getCookieNumString_(numCookies: number): Promise<string> {
    if (numCookies === 0) {
      return Promise.resolve('');
    }
    return this.localDataBrowserProxy_.getNumCookiesString(numCookies);
  }

  /**
   * Array binding for the |originUsages_| array for use in the HTML.
   * @param change The change record for the array.
   * @param index The index of the array item.
   */
  private originUsagesItem_(change: {base: string[]}, index: number): string {
    return change.base[index];
  }

  /**
   * Array binding for the |cookiesNum_| array for use in the HTML.
   * @param change The change record for the array.
   * @param index The index of the array item.
   */
  private originCookiesItem_(change: {base: string[]}, index: number): string {
    return change.base[index];
  }

  /**
   * Navigates to the corresponding Site Details page for the given origin.
   * @param origin The origin to navigate to the Site Details page for it.
   */
  private navigateToSiteDetails_(origin: string) {
    this.fire(
        'site-entry-selected', {item: this.siteGroup, index: this.listIndex});
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_SITE_DETAILS,
        new URLSearchParams('site=' + origin));
  }

  /**
   * A handler for selecting a site (by clicking on the origin).
   */
  private onOriginTap_(e: DomRepeatEvent<OriginInfo>) {
    if (this.siteGroup.origins[e.model.index].isPartitioned) {
      return;
    }
    this.navigateToSiteDetails_(this.siteGroup.origins[e.model.index].origin);
    this.browserProxy.recordAction(AllSitesAction2.ENTER_SITE_DETAILS);
    chrome.metricsPrivate.recordUserAction('AllSites_EnterSiteDetails');
  }

  /**
   * A handler for clicking on a site-entry heading. This will either show a
   * list of origins or directly navigates to Site Details if there is only one.
   */
  private onSiteEntryTap_() {
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.grouped_(this.siteGroup)) {
      this.navigateToSiteDetails_(this.siteGroup.origins[0].origin);
      this.browserProxy.recordAction(AllSitesAction2.ENTER_SITE_DETAILS);
      chrome.metricsPrivate.recordUserAction('AllSites_EnterSiteDetails');
      return;
    }
    this.toggleCollapsible_();

    // Make sure the expanded origins can be viewed without further scrolling
    // (in case |this| is already at the bottom of the viewport).
    this.scrollIntoViewIfNeeded();
  }

  /**
   * Toggles open and closed the list of origins if there is more than one.
   */
  private toggleCollapsible_() {
    const collapseChild = this.$.originList.get();
    collapseChild.toggle();
    this.$.toggleButton.setAttribute(
        'aria-expanded', collapseChild.opened ? 'true' : 'false');
    this.$.expandIcon.toggleClass('icon-expand-more');
    this.$.expandIcon.toggleClass('icon-expand-less');
    this.fire('iron-resize');
  }

  /**
   * Fires a custom event when the menu button is clicked. Sends the details
   * of the site entry item and where the menu should appear.
   */
  private showOverflowMenu_(e: Event) {
    this.fire('open-menu', {
      target: e.target,
      index: this.listIndex,
      item: this.siteGroup,
      origin: (e.target as HTMLElement).dataset['origin'],
      isPartitioned: (e.target as HTMLElement).dataset['partitioned'],
      actionScope: (e.target as HTMLElement).dataset['context'],
    });
  }

  private onRemove_(e: Event) {
    this.fire('remove-site', {
      target: e.target,
      index: this.listIndex,
      item: this.siteGroup,
      origin: (e.target as HTMLElement).dataset['origin'],
      isPartitioned:
          (e.target as HTMLElement).dataset['partitioned'] !== undefined,
      actionScope: (e.target as HTMLElement).dataset['context'],
    });
  }

  /**
   * Returns the correct class to apply depending on this site-entry's position
   * in a list.
   */
  private getClassForIndex_(index: number): string {
    return index > 0 ? 'hr' : '';
  }

  private getRemoveOriginButtonTitle_(origin: string): string {
    return this.i18n(
        'siteSettingsCookieRemoveSite', this.originRepresentation(origin));
  }

  /**
   * Update the order and data display text for origins.
   */
  private updateOrigins_(sortMethod?: SortMethod) {
    if (!sortMethod || !this.siteGroup || !this.grouped_(this.siteGroup)) {
      return;
    }

    const origins = this.siteGroup.origins.slice();
    origins.sort(this.sortFunction_(sortMethod));
    this.set('siteGroup.origins', origins);

    this.originUsages_ = new Array(origins.length);
    origins.forEach((originInfo, i) => {
      this.browserProxy.getFormattedBytes(originInfo.usage).then((string) => {
        this.set(`originUsages_.${i}`, string);
      });
    });

    this.cookiesNum_ = new Array(this.siteGroup.origins.length);
    origins.forEach((originInfo, i) => {
      this.getCookieNumString_(originInfo.numCookies).then((string) => {
        this.set(`cookiesNum_.${i}`, string);
      });
    });
  }

  /**
   * Sort functions for sorting origins based on selected method.
   */
  private sortFunction_(sortMethod: SortMethod):
      (o1: OriginInfo, o2: OriginInfo) => number {
    if (sortMethod === SortMethod.MOST_VISITED) {
      return (origin1, origin2) => {
        return (origin1.isPartitioned ? 1 : 0) -
            (origin2.isPartitioned ? 1 : 0) ||
            origin2.engagement - origin1.engagement;
      };
    } else if (sortMethod === SortMethod.STORAGE) {
      return (origin1, origin2) => {
        return (origin1.isPartitioned ? 1 : 0) -
            (origin2.isPartitioned ? 1 : 0) ||
            origin2.usage - origin1.usage ||
            origin2.numCookies - origin1.numCookies;
      };
    } else if (sortMethod === SortMethod.NAME) {
      return (origin1, origin2) => {
        return (origin1.isPartitioned ? 1 : 0) -
            (origin2.isPartitioned ? 1 : 0) ||
            origin1.origin.localeCompare(origin2.origin);
      };
    }
    assertNotReached();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-entry': SiteEntryElement;
  }
}

customElements.define(SiteEntryElement.is, SiteEntryElement);
