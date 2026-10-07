// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-setting-exceptions' is the polymer element for showing a certain
 * category of exceptions under Site Settings.
 */
import './site_list.js';

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './category_setting_exceptions.css.js';
import {getHtml} from './category_setting_exceptions.html.js';
import {ContentSettingsTypes} from './constants.js';
import {DefaultSettingSource} from './site_settings_browser_proxy.js';
import {SiteSettingsMixinLit} from './site_settings_mixin_lit.js';

const CategorySettingExceptionsElementBase =
    SiteSettingsMixinLit(WebUiListenerMixinLit(CrLitElement));

export class CategorySettingExceptionsElement extends
    CategorySettingExceptionsElementBase {
  static get is() {
    return 'category-setting-exceptions';
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
       * The string description shown below the header.
       */
      description: {type: String},

      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       */
      readOnlyList: {type: Boolean},

      /**
       * True if the default value is managed by a policy.
       */
      defaultManaged_: {type: Boolean},

      /**
       * The heading text for the blocked exception list.
       */
      blockHeader: {type: String},

      /**
       * The heading text for the allowed exception list.
       */
      allowHeader: {type: String},

      searchFilter: {type: String},
    };
  }

  accessor description: string =
      loadTimeData.getString('siteSettingsCustomizedBehaviorsDescription');
  accessor readOnlyList: boolean = false;
  private accessor defaultManaged_: boolean = false;
  accessor blockHeader: string = '';
  accessor allowHeader: string = '';
  accessor searchFilter: string = '';

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'contentSettingCategoryChanged', () => this.updateDefaultManaged_());
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('category')) {
      this.updateDefaultManaged_();
    }
  }

  /**
   * Hides particular category subtypes if |this.category| does not support the
   * content setting of that type.
   */
  protected shouldShowAllowSiteList_(): boolean {
    // TODO(crbug.com/40101962): This function should return true when the
    // feature flag for Persistent Permissions is removed.
    return this.category !== ContentSettingsTypes.FILE_SYSTEM_WRITE &&
        this.category !== ContentSettingsTypes.INLINE_CUE_MENU;
  }

  /**
   * Updates whether or not the default value is managed by a policy.
   */
  private updateDefaultManaged_() {
    if (this.category === undefined) {
      return;
    }

    this.browserProxy.getDefaultValueForContentType(this.category)
        .then(update => {
          this.defaultManaged_ = update.source === DefaultSettingSource.POLICY;
        });
  }

  /**
   * Returns true if this list is explicitly marked as readonly by a consumer
   * of this component or if the default value for these exceptions are managed
   * by a policy. User should not be able to set exceptions to managed default
   * values.
   */
  protected getReadOnlyList_(): boolean {
    return this.readOnlyList || this.defaultManaged_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'category-setting-exceptions': CategorySettingExceptionsElement;
  }
}

customElements.define(
    CategorySettingExceptionsElement.is, CategorySettingExceptionsElement);
