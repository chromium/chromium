// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-setting-exceptions' is the polymer element for showing a certain
 * category of exceptions under Site Settings.
 */
import './site_list.js';

import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {getTemplate} from './category_setting_exceptions.html.js';
import {ContentSetting, ContentSettingsTypes} from './constants.js';
import {SiteSettingsMixin} from './site_settings_mixin.js';
import {DefaultSettingSource} from './site_settings_prefs_browser_proxy.js';

const CategorySettingExceptionsElementBase =
    SiteSettingsMixin(WebUiListenerMixin(PolymerElement));

export class CategorySettingExceptionsElement extends
    CategorySettingExceptionsElementBase {
  static get is() {
    return 'category-setting-exceptions';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The string description shown below the header.
       */
      description: {
        type: String,
        value: function() {
          return loadTimeData.getString(
              'siteSettingsCustomizedBehaviorsDescription');
        },
      },

      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       */
      readOnlyList: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the default value is managed by a policy.
       */
      defaultManaged_: Boolean,

      /**
       * The heading text for the blocked exception list.
       */
      blockHeader: String,

      /**
       * The heading text for the allowed exception list.
       */
      allowHeader: String,

      searchFilter: String,

      /**
       * If true, displays the Allow site list. Defaults to true.
       */
      showAllowSiteList_: {
        type: Boolean,
        computed: 'computeShowAllowSiteList_(category)',
      },

      /**
       * Expose ContentSetting enum to HTML bindings.
       */
      contentSettingEnum_: {
        type: Object,
        value: ContentSetting,
      },
    };
  }

  static get observers() {
    return [
      'updateDefaultManaged_(category)',
    ];
  }

  description: string;
  private readOnlyList: boolean;
  private defaultManaged_: boolean;
  blockHeader: string;
  allowHeader: string;
  searchFilter: string;
  private showAllowSiteList_: boolean;

  override ready() {
    super.ready();

    this.addWebUiListener(
        'contentSettingCategoryChanged', () => this.updateDefaultManaged_());
  }

  /**
   * Hides particular category subtypes if |this.category| does not support the
   * content setting of that type.
   */
  private computeShowAllowSiteList_(): boolean {
    // TODO(crbug.com/40101962): This function should return true when the
    // feature flag for Persistent Permissions is removed.
    return this.category !== ContentSettingsTypes.FILE_SYSTEM_WRITE;
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
  private getReadOnlyList_(): boolean {
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
