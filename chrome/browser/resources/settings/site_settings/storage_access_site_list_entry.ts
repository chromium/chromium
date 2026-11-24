// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-site-list-entry' is an element representing a group of
 * storage access permissions with the same origin and type of permission (e.g.
 * allowed, blocked).
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './storage_access_static_site_list_entry.js';
import '../site_favicon.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {ContentSetting, ContentSettingsTypes} from './constants.js';
import type {StorageAccessEmbeddingException, StorageAccessSiteException} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';
import {getCss} from './storage_access_site_list_entry.css.js';
import {getHtml} from './storage_access_site_list_entry.html.js';
import type {StorageAccessStaticSiteListEntry} from './storage_access_static_site_list_entry.js';

const StorageAccessSiteListEntryElementBase =
    SiteSettingsMixinLit(I18nMixinLit(CrLitElement));

export class StorageAccessSiteListEntryElement extends
    StorageAccessSiteListEntryElementBase {
  static get is() {
    return 'storage-access-site-list-entry';
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
       * A group of storage access site exceptions with the same `origin` and
       * `setting`.
       */
      model: {type: Object},

      /**
       * Signals if the expand button is opened or closed.
       */
      expanded_: {type: Boolean},
    };
  }

  accessor model: StorageAccessSiteException = {
    origin: '',
    displayName: '',
    setting: ContentSetting.DEFAULT,
    exceptions: [],
  };
  protected accessor expanded_: boolean = false;

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('expanded_') && this.expanded_ &&
        this.shouldBeCollapsible_()) {
      this.scrollIntoViewIfNeeded();
    }
  }

  /**
   * Triggered when the top row reset button is clicked.
   * Resets all the permissions in `model.exceptions` i.e. all
   * permissions with the same origin.
   */
  protected onResetAllButtonClick_() {
    for (const exception of this.model.exceptions) {
      this.browserProxy.resetCategoryPermissionForPattern(
          this.model.origin, exception.embeddingOrigin,
          ContentSettingsTypes.STORAGE_ACCESS, exception.incognito);
    }
  }

  protected onExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.expanded_ = e.detail.value;
  }

  protected getResetAllButtonAriaLabel_(): string {
    return this.i18n('storageAccessResetAll', this.model.displayName);
  }

  private getResetButtonAriaLabel_(item: StorageAccessEmbeddingException):
      string {
    return this.i18n(
        'storageAccessResetSite', this.model.displayName,
        item.embeddingDisplayName);
  }

  protected getExpandButtonAriaLabel_(): string {
    return this.expanded_ ? this.i18n('storageAccessCloseExpand') :
                            this.i18n('storageAccessOpenExpand');
  }

  /**
   * @returns the correct description according to the widget's state.
   */
  protected getDescription_(): string {
    if (!this.model.openDescription || !this.model.closeDescription) {
      return '';
    }

    return this.expanded_ ? this.model.openDescription :
                            this.model.closeDescription;
  }

  protected shouldBeStatic_(): boolean {
    return this.model.exceptions.length === 0;
  }

  private shouldBeCollapsible_(): boolean {
    return this.model.exceptions.length > 0;
  }

  protected getStaticSiteEntryForModel_(): StorageAccessStaticSiteListEntry {
    return {
      faviconOrigin: this.model.origin,
      displayName: this.model.displayName,
      description: this.model.description,
      resetAriaLabel: this.getResetAllButtonAriaLabel_(),
      origin: this.model.origin,
      embeddingOrigin: '',
      incognito: this.model.incognito || false,
    };
  }

  protected getStaticSiteEntryForException_(
      item: StorageAccessEmbeddingException): StorageAccessStaticSiteListEntry {
    return {
      faviconOrigin: item.embeddingOrigin,
      displayName: item.embeddingDisplayName,
      description: item.description,
      resetAriaLabel: this.getResetButtonAriaLabel_(item),
      origin: this.model.origin,
      embeddingOrigin: item.embeddingOrigin,
      incognito: item.incognito,
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'storage-access-site-list-entry': StorageAccessSiteListEntryElement;
  }
}

customElements.define(
    StorageAccessSiteListEntryElement.is, StorageAccessSiteListEntryElement);
