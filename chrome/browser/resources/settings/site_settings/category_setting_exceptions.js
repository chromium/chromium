// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-setting-exceptions' is the polymer element for showing a certain
 * category of exceptions under Site Settings.
 */
import './site_list.js';

import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {ContentSetting, ContentSettingsTypes, SiteSettingSource} from './constants.js';
import {SiteSettingsMixin, SiteSettingsMixinInterface} from './site_settings_mixin.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {SiteSettingsMixinInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const CategorySettingExceptionsElementBase = mixinBehaviors(
    [I18nBehavior, WebUIListenerBehavior], SiteSettingsMixin(PolymerElement));

/** @polymer */
export class CategorySettingExceptionsElement extends
    CategorySettingExceptionsElementBase {
  static get is() {
    return 'category-setting-exceptions';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The string description shown below the header.
       */
      description: {
        type: String,
        value() {
          return this.i18n('siteSettingsCustomizedBehaviorsDescription');
        }
      },

      /**
       * The string ID of the category that this element is displaying data for.
       * See site_settings/constants.js for possible values.
       * @type {!ContentSettingsTypes}
       */
      category: String,

      /**
       * Some content types (like Location) do not allow the user to manually
       * edit the exception list from within Settings.
       * @private
       */
      readOnlyList: {
        type: Boolean,
        value: false,
      },

      /**
       * True if the default value is managed by a policy.
       * @private
       */
      defaultManaged_: Boolean,

      /**
       * The heading text for the blocked exception list.
       */
      blockHeader: String,

      /** @private */
      enableContentSettingsRedesign_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enableContentSettingsRedesign');
        }
      },

      /**
       * The heading text for the allowed exception list.
       */
      allowHeader: String,

      searchFilter: String,

      /**
       * If true, displays the Allow site list. Defaults to true.
       * @private
       */
      showAllowSiteList_: {
        type: Boolean,
        computed: 'computeShowAllowSiteList_(category)',
      },

      /**
       * If true, displays the Block site list. Defaults to true.
       */
      showBlockSiteList_: {
        type: Boolean,
        value: true,
      },

      /**
       * Expose ContentSetting enum to HTML bindings.
       * @private
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

  /** @override */
  ready() {
    super.ready();

    this.addWebUIListener(
        'contentSettingCategoryChanged', this.updateDefaultManaged_.bind(this));
  }

  /**
   * Hides particular category subtypes if |this.category| does not support the
   * content setting of that type.
   * @return {boolean}
   * @private
   */
  computeShowAllowSiteList_() {
    return this.category !== ContentSettingsTypes.FILE_SYSTEM_WRITE;
  }

  /**
   * Updates whether or not the default value is managed by a policy.
   * @private
   */
  updateDefaultManaged_() {
    if (this.category === undefined) {
      return;
    }

    this.browserProxy.getDefaultValueForContentType(this.category)
        .then(update => {
          this.defaultManaged_ = update.source === SiteSettingSource.POLICY;
        });
  }

  /**
   * Returns true if this list is explicitly marked as readonly by a consumer
   * of this component or if the default value for these exceptions are managed
   * by a policy. User should not be able to set exceptions to managed default
   * values.
   * @return {boolean}
   * @private
   */
  getReadOnlyList_() {
    return this.readOnlyList || this.defaultManaged_;
  }
}

customElements.define(
    CategorySettingExceptionsElement.is, CategorySettingExceptionsElement);
