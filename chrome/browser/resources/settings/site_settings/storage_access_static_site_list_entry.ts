// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-embedding-site-list-entry' is an element representing a
 * single storage access permission. To be used within
 * 'storage-access-site-list-entry'.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {getTemplate} from './storage_access_static_site_list_entry.html.js';

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
    SiteSettingsMixin(PolymerElement);

export class StorageAccessStaticSiteListEntryElement extends
    StorageAccessStaticSiteListEntryElementBase {
  static get is() {
    return 'storage-access-static-site-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
    };
  }

  model: StorageAccessStaticSiteListEntry;

  /**
   * Triggered when the reset button is clicked. Resets a single storage access
   * site permission.
   */
  private onResetButtonClick_() {
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
