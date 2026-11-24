// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-site-list' is an element representing a list of storage
 * access permissions group with the same type of permission (e.g. allow,
 * block).
 */
import './storage_access_site_list_entry.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import type {ContentSetting} from './constants.js';
import {ContentSettingsTypes, INVALID_CATEGORY_SUBTYPE} from './constants.js';
import type {StorageAccessEmbeddingException, StorageAccessSiteException} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';
import {getHtml} from './storage_access_site_list.html.js';

function getFilteredEmbeddingExceptions(
    exceptions: StorageAccessEmbeddingException[],
    searchFilter: string): StorageAccessEmbeddingException[] {
  type SearchablePropertyEmbedding = 'embeddingDisplayName'|'embeddingOrigin';
  const propNamesEmbedding: SearchablePropertyEmbedding[] =
      ['embeddingDisplayName', 'embeddingOrigin'];

  return exceptions.filter(
      embedding => propNamesEmbedding.some(
          propName =>
              embedding[propName].toLowerCase().includes(searchFilter)));
}

export interface StorageAccessSiteListElement {
  $: {
    listContainer: HTMLElement,
  };
}

const StorageAccessSiteListElementBase =
    SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement));

export class StorageAccessSiteListElement extends
    StorageAccessSiteListElementBase {
  static get is() {
    return 'storage-access-site-list';
  }

  static override get styles() {
    return getSettingsSharedCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Header shown for the |categorySubtype|.
       */
      categoryHeader: {type: String},

      /**
       * Array of group of storage access site exceptions of |categorySubtype|
       * to display in the widget.
       */
      storageAccessExceptions_: {type: Array},

      /**
       * The type of category this widget is displaying data for. Normally
       * either 'allow' or 'block', representing which sites are allowed or
       * blocked respectively from Storage Access while embedded on another
       * site.
       */
      categorySubtype: {type: String},

      searchFilter: {type: String},
    };
  }

  accessor categoryHeader: string = '';
  private accessor storageAccessExceptions_: StorageAccessSiteException[] = [];
  accessor categorySubtype: ContentSetting|typeof INVALID_CATEGORY_SUBTYPE =
      INVALID_CATEGORY_SUBTYPE;
  accessor searchFilter: string = '';

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

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('categorySubtype')) {
      this.populateList_();
    }
  }

  /**
   * Populates the StorageAccessSiteList for display.
   */
  private async populateList_() {
    if (!this.categorySubtype) {
      return;
    }

    this.storageAccessExceptions_ =
        await this.browserProxy.getStorageAccessExceptionList(
            this.categorySubtype);
  }

  /**
   * Whether there are any results to show to the user according with the
   * |searchFilter|.
   */
  protected shouldShowNoSearchResults_(): boolean {
    return this.storageAccessExceptions_.length > 0 &&
        this.getFilteredExceptions_().length === 0;
  }

  /**
   * Whether there are any storage access site exceptions of |categorySubtype|.
   */
  protected hasExceptions_(): boolean {
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
  protected getFilteredExceptions_(): StorageAccessSiteException[] {
    if (!this.searchFilter) {
      return this.storageAccessExceptions_.slice();
    }

    const searchFilter = this.searchFilter.toLowerCase();

    type SearchableProperty = 'displayName'|'origin';
    const propNames: SearchableProperty[] = ['displayName', 'origin'];

    return this.storageAccessExceptions_.filter(
        site => propNames.some(propName => {
          return site[propName].toLowerCase().includes(searchFilter) ||
              getFilteredEmbeddingExceptions(site.exceptions, searchFilter)
                  .length;
        }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'storage-access-site-list': StorageAccessSiteListElement;
  }
}

customElements.define(
    StorageAccessSiteListElement.is, StorageAccessSiteListElement);
