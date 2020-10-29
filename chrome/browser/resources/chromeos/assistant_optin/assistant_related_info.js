// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * related info screen.
 *
 * Event 'loading' will be fired when the page is loading/reloading.
 * Event 'loaded' will be fired when the page has been successfully loaded.
 */

/**
 * Name of the screen.
 * @type {string}
 */
const RELATED_INFO_SCREEN_ID = 'RelatedInfoScreen';

Polymer({
  is: 'assistant-related-info',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  properties: {
    /**
     * Whether page content is loading.
     */
    loading: {
      type: Boolean,
      value: true,
    },

    /**
     * Title key of the screen.
     */
    titleKey: {
      type: String,
      value: 'assistantRelatedInfoTitle',
    },

    /**
     * Content key of the screen.
     */
    contentKey: {
      type: String,
      value: 'assistantRelatedInfoMessage',
    },
  },

  /**
   * Whether all the consent text strings has been successfully loaded.
   * @type {boolean}
   * @private
   */
  consentStringLoaded_: false,

  /**
   * Whether the screen has been shown to the user.
   * @type {boolean}
   * @private
   */
  screenShown_: false,

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /**
   * On-tap event handler for next button.
   *
   * @private
   */
  onNextTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['next-pressed']);
  },

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    if (this.loading) {
      return;
    }
    this.loading = true;
    this.browserProxy_.userActed(RELATED_INFO_SCREEN_ID, ['skip-pressed']);
  },

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Reloads the page.
   */
  reloadPage() {
    this.fire('loading');
    this.loading = true;
  },

  /**
   * Reload the page with the given consent string text data.
   */
  reloadContent(data) {
    this.titleKey = data['activityControlNeeded'] ?
        'assistantRelatedInfoTitle' :
        'assistantRelatedInfoReturnedUserTitle';
    this.contentKey = data['activityControlNeeded'] ?
        'assistantRelatedInfoMessage' :
        'assistantRelatedInfoReturnedUserMessage';
    this.consentStringLoaded_ = true;
    this.onPageLoaded();
  },

  /**
   * Handles event when all the page content has been loaded.
   */
  onPageLoaded() {
    this.fire('loaded');
    this.loading = false;
    this.$['next-button'].focus();
    if (!this.hidden && !this.screenShown_) {
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  },

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    if (!this.consentStringLoaded_) {
      this.reloadPage();
    } else {
      Polymer.RenderStatus.afterNextRender(
          this, () => this.$['next-button'].focus());
      this.browserProxy_.screenShown(RELATED_INFO_SCREEN_ID);
      this.screenShown_ = true;
    }
  },
});
