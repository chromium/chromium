// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-site-list' is an element representing a list of storage
 * access permissions group with the same type of permission (e.g. allow,
 * block).
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';

import {ListPropertyUpdateMixin} from 'chrome://resources/cr_elements/list_property_update_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {ContentSetting} from './constants.js';
import {ContentSettingsTypes, INVALID_CATEGORY_SUBTYPE} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {StorageAccessEmbeddingException, StorageAccessSiteException} from './site_settings_prefs_browser_proxy.js';
import {getTemplate} from './storage_access_site_list.html.js';

export interface StorageAccessSiteListElement {
  $: {
    listContainer: HTMLElement,
  };
}

const StorageAccessSiteListElementBase = ListPropertyUpdateMixin(
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement)));

export class StorageAccessSiteListElement extends
    StorageAccessSiteListElementBase {
  static get is() {
    return 'storage-access-site-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Header shown for the |categorySubtype|.
       */
      categoryHeader: String,

      /**
       * Array of group of storage access site exceptions of |categorySubtype|
       * to display in the widget.
       */
      storageAccessExceptions_: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * The type of category this widget is displaying data for. Normally
       * either 'allow' or 'block', representing which sites are allowed or
       * blocked respectively from Storage Access while embedded on another
       * site.
       */
      categorySubtype: {
        type: String,
        value: INVALID_CATEGORY_SUBTYPE,
      },

      searchFilter: {
        type: String,
        observer: 'getFilteredExceptions_',
      },
    };
  }

  static get observers() {
    return ['populateList_(categorySubtype, storageAccessExceptions_)'];
  }

  categorySubtype: ContentSetting;
  categoryHeader: string;
  searchFilter: string;

  private storageAccessExceptions_: StorageAccessSiteException[];

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingSitePermissionChanged',
        (category: ContentSettingsTypes) => {
          if (category !== ContentSettingsTypes.STORAGE_ACCESS) {
            return;
          }
          this.populateList_();
        });
    this.addWebUiListener(
        'onIncognitoStatusChanged', () => this.populateList_());
    this.browserProxy.updateIncognitoStatus();
  }

  /**
   * Populates the StorageAccessSiteList for display.
   */
  private async populateList_() {
    if (this.categorySubtype === undefined) {
      return;
    }

    const exceptionList = await this.browserProxy.getStorageAccessExceptionList(
        this.categorySubtype);
    this.updateList('storageAccessExceptions_', x => x.origin, exceptionList);
  }

  /**
   * Whether there are any results to show to the user according with the
   * |searchFilter|.
   */
  private showNoSearchResults_(): boolean {
    return this.storageAccessExceptions_.length > 0 &&
        this.getFilteredExceptions_().length === 0;
  }

  /**
   * Whether there are any storage access site exceptions of |categorySubtype|.
   */
  private hasExceptions_(): boolean {
    return this.storageAccessExceptions_.length > 0;
  }

  /**
   * Returns the filtered |StorageAccessSiteException|s that match the
   * |searchFilter|.
   *
   * It looks for matches in |displayName|, and |origin|. If the |origin| or
   * |displayName| don't match, it looks for matches in |embeddingDisplayName|,
   * and |embeddingOrigin|.
   */
  private getFilteredExceptions_(): StorageAccessSiteException[] {
    if (!this.searchFilter) {
      return this.storageAccessExceptions_.slice();
    }

    const searchFilter = this.searchFilter.toLowerCase();

    type SearchableProperty = 'displayName'|'origin';
    const propNames: SearchableProperty[] = ['displayName', 'origin'];

    return this.storageAccessExceptions_.filter(
        site => propNames.some(propName => {
          return site[propName].toLowerCase().includes(searchFilter) ||
              this.getFilteredEmbeddingExceptions_(
                      site.exceptions, searchFilter)
                  .length;
        }));
  }

  private getFilteredEmbeddingExceptions_(
      exceptions: StorageAccessEmbeddingException[],
      searchFilter: string): StorageAccessEmbeddingException[] {
    type SearchablePropertyEmbedding = 'embeddingDisplayName'|'embeddingOrigin';
    const propNamesEmbedding: SearchablePropertyEmbedding[] =
        ['embeddingDisplayName', 'embeddingOrigin'];

    return exceptions.filter(
        embedding => propNamesEmbedding.some(
            propNamesEmbedding =>
                embedding[propNamesEmbedding].toLowerCase().includes(
                    searchFilter)));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'storage-access-site-list': StorageAccessSiteListElement;
  }
}

customElements.define(
    StorageAccessSiteListElement.is, StorageAccessSiteListElement);
