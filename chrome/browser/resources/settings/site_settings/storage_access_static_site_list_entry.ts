// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-static-site-list-entry' is an element representing a
 * single storage access permission. To be used within
 * 'storage-access-site-list-entry'.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/policy/cr_tooltip_icon.js';
import '../site_favicon.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';
import {getCss} from './storage_access_static_site_list_entry.css.js';
import {getHtml} from './storage_access_static_site_list_entry.html.js';

// This interface wraps all the properties we need to display a
// `StorageAccessStaticSiteListEntryElement`.
export interface StorageAccessStaticSiteListEntry {
  faviconOrigin: string;
  displayName: string;
  description?: string;
  resetAriaLabel: string;
  origin: string;
  embeddingOrigin: string;
  incognito: boolean;
}

export interface StorageAccessStaticSiteListEntryElement {
  $: {
    displayName: HTMLElement,
    resetButton: HTMLElement,
  };
}

const StorageAccessStaticSiteListEntryElementBase =
    SiteSettingsMixinLit(CrLitElement);

export class StorageAccessStaticSiteListEntryElement extends
    StorageAccessStaticSiteListEntryElementBase {
  static get is() {
    return 'storage-access-static-site-list-entry';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      model: {type: Object},
    };
  }

  accessor model: StorageAccessStaticSiteListEntry = {
    faviconOrigin: '',
    displayName: '',
    resetAriaLabel: '',
    origin: '',
    embeddingOrigin: '',
    incognito: false,
  };

  /**
   * Triggered when the reset button is clicked. Resets a single storage access
   * site permission.
   */
  protected onResetButtonClick_() {
    this.browserProxy.resetCategoryPermissionForPattern(
        this.model.origin, this.model.embeddingOrigin,
        ContentSettingsTypes.STORAGE_ACCESS, this.model.incognito);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'storage-access-static-site-list-entry':
        StorageAccessStaticSiteListEntryElement;
  }
}

customElements.define(
    StorageAccessStaticSiteListEntryElement.is,
    StorageAccessStaticSiteListEntryElement);
