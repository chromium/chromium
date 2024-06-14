// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'storage-access-site-list-entry' is an element representing a group of
 * storage access permissions with the same origin and type of permission (e.g.
 * allowed, blocked).
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings_shared.css.js';
import '../site_favicon.js';

import {FocusRowMixin} from 'chrome://resources/cr_elements/focus_row_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import type {DomIf} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import type {StorageAccessEmbeddingException, StorageAccessSiteException} from './site_settings_prefs_browser_proxy.js';
import {getTemplate} from './storage_access_site_list_entry.html.js';
import type {StorageAccessStaticSiteListEntry} from './storage_access_static_site_list_entry.js';

const StorageAccessSiteListEntryElementBase =
    FocusRowMixin(SiteSettingsMixin(I18nMixin(PolymerElement)));

export class StorageAccessSiteListEntryElement extends
    StorageAccessSiteListEntryElementBase {
  static get is() {
    return 'storage-access-site-list-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * A group of storage access site exceptions with the same `origin` and
       * `setting`.
       */
      model: {
        type: Object,
        observer: 'onModelChanged_',
      },

      /**
       * Signals if the expand button is opened or closed.
       */
      expanded_: {
        type: Boolean,
        observer: 'onExpandedChanged_',
        notify: true,
        value: false,
      },
    };
  }

  model: StorageAccessSiteException;

  private description_: string;
  private expandAriaLabel_: string;
  private expanded_: boolean;

  /**
   * Triggered when the top row reset button is clicked.
   * Resets all the permissions in `model.exceptions` i.e. all
   * permissions with the same origin.
   */
  private onResetAllButtonClick_() {
    for (const exception of this.model.exceptions) {
      this.browserProxy.resetCategoryPermissionForPattern(
          this.model.origin, exception.embeddingOrigin,
          ContentSettingsTypes.STORAGE_ACCESS, exception.incognito);
    }
  }

  /**
   * A handler for the model change.
   */
  private onModelChanged_() {
    this.description_ = this.computeDescription_();
    this.expandAriaLabel_ = this.computeExpandButtonAriaLabel_();
  }

  /**
   * A handler for clicking on the top-row. This will scroll to the
   * element if needed.
   */
  private onExpandedChanged_() {
    if (!this.shouldBeCollapsible_()) {
      return;
    }

    this.description_ = this.computeDescription_();
    this.expandAriaLabel_ = this.computeExpandButtonAriaLabel_();

    if (!this.expanded_) {
      return;
    }

    // Renders the nested rows if they haven't been opened before, so we can
    // scroll to make them visible if necessary.
    this.shadowRoot!.querySelector<DomIf>('#originList')!.render();

    this.scrollIntoViewIfNeeded();
  }

  private getResetAllButtonAriaLabel_() {
    return this.i18n('storageAccessResetAll', this.model.displayName);
  }

  private getResetButtonAriaLabel_(item: StorageAccessEmbeddingException) {
    return this.i18n(
        'storageAccessResetSite', this.model.displayName,
        item.embeddingDisplayName);
  }

  private computeExpandButtonAriaLabel_() {
    return this.expanded_ ? this.i18n('storageAccessCloseExpand') :
                            this.i18n('storageAccessOpenExpand');
  }

  /**
   * @returns the correct description according to the widget's state.
   */
  private computeDescription_(): string {
    if (!this.model || !this.model.openDescription ||
        !this.model.closeDescription) {
      return '';
    }

    return this.expanded_ ? this.model.openDescription :
                            this.model.closeDescription;
  }

  private shouldBeStatic_(): boolean {
    if (!this.model) {
      return false;
    }

    return this.model.exceptions.length === 0;
  }

  private shouldBeCollapsible_(): boolean {
    if (!this.model) {
      return false;
    }

    return this.model.exceptions.length !== 0;
  }

  private getStaticSiteEntryForModel_(): StorageAccessStaticSiteListEntry {
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

  private getStaticSiteEntryForException_(
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
