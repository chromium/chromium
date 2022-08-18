// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_search_field/cr_search_field.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '../settings_shared.css.js';
import './site_data_entry.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {ListPropertyUpdateMixin} from 'chrome://resources/js/list_property_update_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {DomRepeatEvent, microTask, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {FocusConfig} from '../focus_config.js';
import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Route, Router} from '../router.js';

import {LocalDataBrowserProxy, LocalDataBrowserProxyImpl, LocalDataItem} from './local_data_browser_proxy.js';
import {getTemplate} from './site_data.html.js';

interface SelectedItem {
  item: LocalDataItem;
  index: number;
}

export interface SiteDataElement {
  $: {
    confirmDeleteDialog: CrDialogElement,
    confirmDeleteThirdPartyDialog: CrDialogElement,
    list: IronListElement,
    removeShowingSites: HTMLElement,
    removeAllThirdPartyCookies: HTMLElement,
  };
}

const SiteDataElementBase = ListPropertyUpdateMixin(
    GlobalScrollTargetMixin(WebUIListenerMixin(BaseMixin(PolymerElement))));

export class SiteDataElement extends SiteDataElementBase {
  static get is() {
    return 'site-data';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The current filter applied to the cookie data list.
       */
      filter: {
        observer: 'onFilterChanged_',
        notify: true,
        type: String,
      },

      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

      isLoading_: Boolean,

      sites: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * GlobalScrollTargetMixin
       */
      subpageRoute: {
        type: Object,
        value: routes.SITE_SETTINGS_SITE_DATA,
      },

      lastFocused_: Object,
      listBlurred_: Boolean,
    };
  }

  filter: string;
  focusConfig: FocusConfig;
  private isLoading_: boolean;
  sites: LocalDataItem[];
  subpageRoute: Route;
  private listBlurred_: boolean;
  private browserProxy_: LocalDataBrowserProxy =
      LocalDataBrowserProxyImpl.getInstance();
  private lastSelected_: SelectedItem|null;

  constructor() {
    super();

    /**
     * When navigating to site data details sub-page, |lastSelected_| holds the
     * site name as well as the index of the selected site. This is used when
     * navigating back to site data in order to focus on the correct site.
     */
    this.lastSelected_ = null;
  }

  override ready() {
    super.ready();

    this.addWebUIListener('on-tree-item-removed', () => this.updateSiteList_());
  }

  /**
   * Reload cookies when the site data page is visited.
   *
   * RouteObserverMixin
   */
  override currentRouteChanged(currentRoute: Route, previousRoute: Route) {
    super.currentRouteChanged(currentRoute);
    // Reload cookies on navigation to the site data page from a different
    // page. Avoid reloading on repeated navigations to the same page, as these
    // are likely search queries.
    if (currentRoute === routes.SITE_SETTINGS_SITE_DATA &&
        currentRoute !== previousRoute) {
      this.isLoading_ = true;
      // Needed to fix iron-list rendering issue. The list will not render
      // correctly until a scroll occurs.
      // See https://crbug.com/853906.
      const ironList = this.shadowRoot!.querySelector('iron-list')!;
      ironList.scrollToIndex(0);
      this.browserProxy_.reloadCookies().then(() => this.updateSiteList_());
    }
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    if (routes.SITE_SETTINGS_DATA_DETAILS) {
      const onNavigatedTo = () => microTask.run(() => {
        if (this.lastSelected_ === null || this.sites.length === 0) {
          return;
        }

        const lastSelectedSite = this.lastSelected_.item.site;
        const lastSelectedIndex = this.lastSelected_.index;
        this.lastSelected_ = null;

        const indexFromId =
            this.sites.findIndex(site => site.site === lastSelectedSite);

        // If the site is no longer in |sites|, use the index as a fallback.
        // Since the sites are sorted, an alternative could be to select the
        // site that comes next in sort order.
        const indexFallback = lastSelectedIndex < this.sites.length ?
            lastSelectedIndex :
            this.sites.length - 1;
        const index = indexFromId > -1 ? indexFromId : indexFallback;
        this.focusOnSiteSelectButton_(index);
      });
      this.focusConfig.set(
          routes.SITE_SETTINGS_DATA_DETAILS.path, onNavigatedTo);
    }
  }

  private focusOnSiteSelectButton_(index: number) {
    const ironList = this.shadowRoot!.querySelector('iron-list')!;
    ironList.focusItem(index);
    const siteToSelect = this.sites[index].site.replace(/[.]/g, '\\.');
    const button =
        this.$$(`#siteItem_${siteToSelect}`)!.shadowRoot!.querySelector(
            '.subpage-arrow');
    assert(button);
    focusWithoutInk(button);
  }

  private onFilterChanged_(_current: string, previous?: string) {
    // Ignore filter changes which do not occur on the site data page. The
    // site settings data details subpage expects the tree model to remain in
    // the same state.
    if (previous === undefined ||
        Router.getInstance().getCurrentRoute() !==
            routes.SITE_SETTINGS_SITE_DATA) {
      return;
    }
    this.updateSiteList_();
  }

  /**
   * Gather all the site data.
   */
  private updateSiteList_() {
    this.isLoading_ = true;
    this.browserProxy_.getDisplayList(this.filter).then(localDataItems => {
      this.updateList('sites', item => item.site, localDataItems);
      this.isLoading_ = false;
      this.fire('site-data-list-complete');
    });
  }

  /**
   * Returns the string to use for the Remove label.
   * @param filter The current filter string.
   */
  private computeRemoveLabel_(filter: string): string {
    if (filter.length === 0) {
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    }
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  }

  private onCloseDialog_() {
    this.$.confirmDeleteDialog.close();
  }

  private onCloseThirdPartyDialog_() {
    this.$.confirmDeleteThirdPartyDialog.close();
  }

  private onConfirmDeleteDialogClosed_() {
    focusWithoutInk(this.$.removeShowingSites);
  }

  private onConfirmDeleteThirdPartyDialogClosed_() {
    focusWithoutInk(this.$.removeAllThirdPartyCookies);
  }

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   */
  private onRemoveShowingSitesTap_(e: Event) {
    e.preventDefault();
    this.$.confirmDeleteDialog.showModal();
  }

  /**
   * Shows a dialog to confirm the deletion of cookies available
   * in third-party contexts and associated site data.
   */
  private onRemoveThirdPartyCookiesTap_(e: Event) {
    e.preventDefault();
    this.$.confirmDeleteThirdPartyDialog.showModal();
  }

  /**
   * Called when deletion for all showing sites has been confirmed.
   */
  private onConfirmDelete_() {
    this.$.confirmDeleteDialog.close();
    if (this.filter.length === 0) {
      MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
          PrivacyElementInteractions.SITE_DATA_REMOVE_ALL);
      this.browserProxy_.removeAll().then(() => {
        this.sites = [];
      });
    } else {
      MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
          PrivacyElementInteractions.SITE_DATA_REMOVE_FILTERED);
      this.browserProxy_.removeShownItems();
      // We just deleted all items found by the filter, let's reset the filter.
      this.fire('clear-subpage-search');
    }
  }

  /**
   * Called when deletion of all third-party cookies and site data has been
   * confirmed.
   */
  private onConfirmThirdPartyDelete_() {
    this.$.confirmDeleteThirdPartyDialog.close();
    this.browserProxy_.removeAllThirdPartyCookies().then(() => {
      this.updateSiteList_();
    });
  }

  private onSiteClick_(event: DomRepeatEvent<LocalDataItem>) {
    // If any delete button is selected, the focus will be in a bad state when
    // returning to this page. To avoid this, the site select button is given
    // focus. See https://crbug.com/872197.
    this.focusOnSiteSelectButton_(event.model.index);
    Router.getInstance().navigateTo(
        routes.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + event.model.item.site));
    this.lastSelected_ = event.model;
  }

  private showRemoveThirdPartyCookies_(): boolean {
    return loadTimeData.getBoolean('enableRemovingAllThirdPartyCookies') &&
        this.sites.length > 0 && this.filter.length === 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'site-data': SiteDataElement;
  }
}

customElements.define(SiteDataElement.is, SiteDataElement);
