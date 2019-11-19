// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'category-setting-exceptions' is the polymer element for showing a certain
 * category of exceptions under Site Settings.
 */
Polymer({
  is: 'category-setting-exceptions',

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {

    /**
     * The string ID of the category that this element is displaying data for.
     * See site_settings/constants.js for possible values.
     * @type {!settings.ContentSettingsTypes}
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
  },

  observers: [
    'updateDefaultManaged_(category)',
  ],

  /** @override */
  ready: function() {
    this.ContentSetting = settings.ContentSetting;
    this.addWebUIListener(
        'contentSettingCategoryChanged', this.updateDefaultManaged_.bind(this));
  },

  /**
   * Hides particular category subtypes if |this.category| does not support the
   * content setting of that type.
   * @return {boolean}
   * @private
   */
  computeShowAllowSiteList_: function() {
    return this.category !=
        settings.ContentSettingsTypes.NATIVE_FILE_SYSTEM_WRITE;
  },

  /**
   * Updates whether or not the default value is managed by a policy.
   * @private
   */
  updateDefaultManaged_: function() {
    if (this.category === undefined) {
      return;
    }

    this.browserProxy.getDefaultValueForContentType(this.category)
      .then(update => {
        this.defaultManaged_ =
          update.source === settings.SiteSettingSource.POLICY;
      });
  },

  /**
   * Returns true if this list is explicitly marked as readonly by a consumer
   * of this component or if the default value for these exceptions are managed
   * by a policy. User should not be able to set exceptions to managed default
   * values.
   * @return {boolean}
   * @private
   */
  getReadOnlyList_: function() {
    return this.readOnlyList || this.defaultManaged_;
  }
});
