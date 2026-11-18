// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-entry' is an element representing a single eTLD+1 site entity.
 */
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '../site_favicon.js';

import type {CrCollapseElement} from 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {FocusRowMixinLit} from 'chrome://resources/cr_elements/focus_row_mixin_lit.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {AllSitesAction2, SortMethod} from './constants.js';
import {getCss} from './site_entry.css.js';
import {getHtml} from './site_entry.html.js';
import type {OriginInfo, SiteGroup} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

export interface SiteEntryElement {
  $: {
    expandIcon: CrIconButtonElement,
    collapseParent: HTMLElement,
    cookies: HTMLElement,
    displayName: HTMLElement,
    originList: CrLazyRenderLitElement<CrCollapseElement>,
    toggleButton: HTMLElement,
    extensionIdDescription: HTMLElement,
  };
}

const SiteEntryElementBase =
    FocusRowMixinLit(SiteSettingsMixinLit(I18nMixinLit(CrLitElement)));

export class SiteEntryElement extends SiteEntryElementBase {
  static get is() {
    return 'site-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * An object representing a group of sites with the same eTLD+1.
       */
      siteGroup: {type: Object},

      /**
       * Whether the collapsible list of origins is expanded.
       */
      expanded_: {type: Boolean},

      /**
       * The origins in |siteGroup|, sorted according to |sortMethod|.
       */
      sortedOrigins_: {type: Array},

      /**
       * The string to display when there is a non-zero number of cookies.
       */
      cookieString_: {type: String},

      /**
       * The position of this site-entry in its parent list.
       */
      listIndex: {type: Number},

      /**
       * The string to display showing the overall usage of this site-entry.
       */
      overallUsageString_: {type: String},

      /**
       * An array containing the strings to display showing the individual disk
       * usage for each origin in |siteGroup|.
       */
      originUsages_: {type: Array},

      /**
       * An array containing the strings to display showing the individual
       * cookies number for each origin in |siteGroup|.
       */
      cookiesNum_: {type: Array},

      /**
       * The selected sort method.
       */
      sortMethod: {type: String},
    };
  }

  accessor siteGroup: SiteGroup|undefined = undefined;
  protected accessor expanded_: boolean = false;
  protected accessor sortedOrigins_: OriginInfo[] = [];
  protected accessor cookieString_: string = '';
  accessor listIndex: number = -1;
  protected accessor overallUsageString_: string = '';
  protected accessor originUsages_: string[] = [];
  protected accessor cookiesNum_: string[] = [];
  accessor sortMethod: SortMethod|undefined = undefined;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('siteGroup')) {
      if (this.siteGroup) {
        if (!this.isGrouped_()) {
          // Ensure ungrouped |siteGroup|s do not get stuck in an opened state.
          this.expanded_ = false;
        }
        this.calculateUsageInfo_(this.siteGroup);
        this.getCookieNumString_(this.siteGroup.numCookies).then(string => {
          this.cookieString_ = string;
        });
        this.updateOrigins_(this.sortMethod);
      } else {
        this.sortedOrigins_ = [];
      }
    }

    if (changedProperties.has('sortMethod') &&
        !changedProperties.has('siteGroup')) {
      this.updateOrigins_(this.sortMethod);
    }
  }

  /**
   * Whether the list of origins displayed in this site-entry is a group of
   * eTLD+1 origins or not.
   */
  protected isGrouped_(): boolean {
    if (!this.siteGroup) {
      return false;
    }
    if (this.siteGroup.origins.length > 1 ||
        this.siteGroup.numCookies > this.siteGroup.origins[0].numCookies ||
        this.siteGroup.origins.some(o => o.isPartitioned)) {
      return true;
    }
    return false;
  }

  /**
   * Returns any non-HTTPS scheme/protocol for the siteGroup that only contains
   * one origin. Otherwise, returns a empty string.
   * @return The scheme if non-HTTPS, or empty string if HTTPS.
   */
  protected getSiteGroupScheme_(): string {
    if (!this.siteGroup || this.isGrouped_()) {
      return '';
    }
    return this.getOriginScheme_(this.siteGroup.origins[0]);
  }

  /**
   * Returns any non-HTTPS scheme/protocol for the origin. Otherwise, returns
   * an empty string.
   * @return The scheme if non-HTTPS, or empty string if HTTPS.
   */
  protected getOriginScheme_(origin: OriginInfo): string {
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
   * @return URL that is used for fetching the favicon
   */
  protected getSiteGroupIcon_(): string {
    if (!this.siteGroup) {
      return '';
    }
    const origins = this.siteGroup.origins;
    assert(origins);
    assert(origins.length >= 1);
    if (origins.length === 1) {
      return origins[0].origin;
    }
    // If we can find a origin with format "www.etld+1", use the favicon of this
    // origin. Otherwise find the origin with largest storage, and use the
    // number of cookies as a tie breaker.
    for (const originInfo of origins) {
      if (this.siteGroup.etldPlus1 &&
          this.toUrl(originInfo.origin)!.host ===
              'www.' + this.siteGroup.etldPlus1) {
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
    return this.browserProxy.getNumCookiesString(numCookies);
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
  protected onOriginClick_(e: MouseEvent) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const origin = this.sortedOrigins_[index];
    assert(origin);
    if (origin.isPartitioned) {
      return;
    }
    this.navigateToSiteDetails_(origin.origin);
    this.browserProxy.recordAction(AllSitesAction2.ENTER_SITE_DETAILS);
    chrome.metricsPrivate.recordUserAction('AllSites_EnterSiteDetails');
  }

  /**
   * A handler for clicking on a site-entry heading. This will either show a
   * list of origins or directly navigates to Site Details if there is only one.
   */
  protected onSiteEntryClick_() {
    assert(this.siteGroup);
    // Individual origins don't expand - just go straight to Site Details.
    if (!this.isGrouped_()) {
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
    this.expanded_ = !this.expanded_;
    this.$.originList.get();
  }

  protected onRemoveClick_(e: Event) {
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
  protected getClassForIndex_(): string {
    return this.listIndex > 0 ? 'hr' : '';
  }

  protected getSubpageLabel_(): string {
    return this.i18n(
        'siteSettingsSiteDetailsSubpageAccessibilityLabel',
        this.siteGroup?.displayName || '');
  }

  protected getOriginSubpageLabel_(origin: string): string {
    return this.i18n(
        'siteSettingsSiteDetailsSubpageAccessibilityLabel',
        this.originRepresentation(origin));
  }

  protected getRemoveOriginButtonTitle_(origin: string): string {
    return this.i18n(
        'siteSettingsCookieRemoveSite', this.originRepresentation(origin));
  }

  /**
   * Update the order and data display text for origins.
   */
  private updateOrigins_(sortMethod?: SortMethod) {
    if (!this.siteGroup) {
      this.sortedOrigins_ = [];
      return;
    }
    if (!sortMethod || !this.isGrouped_()) {
      this.sortedOrigins_ = this.siteGroup.origins;
      return;
    }

    const origins = [...this.siteGroup.origins];
    origins.sort(this.sortFunction_(sortMethod));
    this.sortedOrigins_ = origins;

    this.originUsages_ = new Array(origins.length);
    origins.forEach((originInfo, i) => {
      this.browserProxy.getFormattedBytes(originInfo.usage).then((string) => {
        const next = [...this.originUsages_];
        next[i] = string;
        this.originUsages_ = next;
      });
    });

    this.cookiesNum_ = new Array(origins.length);
    origins.forEach((originInfo, i) => {
      this.getCookieNumString_(originInfo.numCookies).then((string) => {
        const next = [...this.cookiesNum_];
        next[i] = string;
        this.cookiesNum_ = next;
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

  /**
   * Get extension id description string for an extension |siteGroup|.
   */
  protected getExtensionIdDescription_(): string {
    if (!this.siteGroup || !this.siteGroup.origins.length) {
      return '';
    }
    const id = this.originRepresentation(this.siteGroup.origins[0].origin);
    return loadTimeData.getStringF('siteSettingsExtensionIdDescription', id);
  }

  /**
   * Check if the given |siteGroup| is an extension.
   */
  protected isExtension_(): boolean {
    return this.getSiteGroupScheme_() === 'chrome-extension';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-entry': SiteEntryElement;
  }
}

customElements.define(SiteEntryElement.is, SiteEntryElement);
