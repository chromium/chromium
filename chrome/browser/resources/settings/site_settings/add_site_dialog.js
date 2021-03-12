// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'add-site-dialog' provides a dialog to add exceptions for a given Content
 * Settings category.
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import '../settings_shared_css.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';

import {ContentSetting, ContentSettingsTypes, SITE_EXCEPTION_WILDCARD} from './constants.js';
import {SiteSettingsBehavior} from './site_settings_behavior.js';

Polymer({
  is: 'add-site-dialog',

  _template: html`{__html_template__}`,

  behaviors: [SiteSettingsBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * What kind of setting, e.g. Location, Camera, Cookies, and so on.
     * @type {ContentSettingsTypes}
     */
    category: String,

    /**
     * Whether this is about an Allow, Block, SessionOnly, or other.
     * @type {ContentSetting}
     */
    contentSetting: String,

    hasIncognito: {
      type: Boolean,
      observer: 'hasIncognitoChanged_',
    },

    /**
     * The site to add an exception for.
     * @private
     */
    site_: String,

    /**
     * The error message to display when the pattern is invalid.
     * @private
     */
    errorMessage_: String,
  },

  /** @override */
  attached() {
    assert(this.category);
    assert(this.contentSetting);
    assert(typeof this.hasIncognito !== 'undefined');

    this.$.dialog.showModal();
  },

  /**
   * Validates that the pattern entered is valid.
   * @private
   */
  validate_() {
    // If input is empty, disable the action button, but don't show the red
    // invalid message.
    if (this.$.site.value.trim() === '') {
      this.$.site.invalid = false;
      this.$.add.disabled = true;
      return;
    }

    this.browserProxy.isPatternValidForType(this.site_, this.category)
        .then(({isValid, reason}) => {
          this.$.site.invalid = !isValid;
          this.$.add.disabled = !isValid;
          this.errorMessage_ = reason || '';
        });
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /**
   * The tap handler for the Add [Site] button (adds the pattern and closes
   * the dialog).
   * @private
   */
  onSubmit_() {
    assert(!this.$.add.disabled);
    let primaryPattern = this.site_;
    let secondaryPattern = SITE_EXCEPTION_WILDCARD;

    if (this.$.thirdParties.checked) {
      primaryPattern = SITE_EXCEPTION_WILDCARD;
      secondaryPattern = this.site_;
    }

    this.browserProxy.setCategoryPermissionForPattern(
        primaryPattern, secondaryPattern, this.category, this.contentSetting,
        this.$.incognito.checked);

    this.$.dialog.close();
  },

  /** @private */
  showIncognitoSessionOnly_() {
    return this.hasIncognito && !loadTimeData.getBoolean('isGuest') &&
        this.contentSetting !== ContentSetting.SESSION_ONLY;
  },

  /** @private */
  hasIncognitoChanged_() {
    if (!this.hasIncognito) {
      this.$.incognito.checked = false;
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldHideThirdPartyCookieCheckbox_() {
    return this.category !== ContentSettingsTypes.COOKIES;
  },
});
