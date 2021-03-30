// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-startup-urls-page' is the settings page
 * containing the urls that will be opened when chrome is started.
 */

import 'chrome://resources/js/action_link.js';
import 'chrome://resources/cr_elements/action_link_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import '../controls/extension_controlled_indicator.js';
import '../settings_shared_css.js';
import './startup_url_dialog.js';

import {CrScrollableBehavior} from 'chrome://resources/cr_elements/cr_scrollable_behavior.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {EDIT_STARTUP_URL_EVENT} from './startup_url_entry.js';
import {StartupPageInfo, StartupUrlsPageBrowserProxy, StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-startup-urls-page',

  behaviors: [CrScrollableBehavior, WebUIListenerBehavior],

  properties: {
    prefs: Object,

    /**
     * Pages to load upon browser startup.
     * @private {!Array<!StartupPageInfo>}
     */
    startupPages_: Array,

    /** @private */
    showStartupUrlDialog_: Boolean,

    /** @private {?StartupPageInfo} */
    startupUrlDialogModel_: Object,

    /** @private {Object}*/
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,
  },

  /** @private {?StartupUrlsPageBrowserProxy} */
  browserProxy_: null,

  /**
   * The element to return focus to, when the startup-url-dialog is closed.
   * @private {?HTMLElement}
   */
  startupUrlDialogAnchor_: null,

  /** @override */
  attached() {
    this.browserProxy_ = StartupUrlsPageBrowserProxyImpl.getInstance();
    this.addWebUIListener('update-startup-pages', startupPages => {
      // If an "edit" URL dialog was open, close it, because the underlying page
      // might have just been removed (and model indices have changed anyway).
      if (this.startupUrlDialogModel_) {
        this.destroyUrlDialog_();
      }
      this.startupPages_ = startupPages;
      this.updateScrollableContents();
    });
    this.browserProxy_.loadStartupPages();

    this.addEventListener(EDIT_STARTUP_URL_EVENT, event => {
      this.startupUrlDialogModel_ = event.detail.model;
      this.startupUrlDialogAnchor_ = event.detail.anchor;
      this.showStartupUrlDialog_ = true;
      event.stopPropagation();
    });
  },

  /**
   * @param {!Event} e
   * @private
   */
  onAddPageTap_(e) {
    e.preventDefault();
    this.showStartupUrlDialog_ = true;
    this.startupUrlDialogAnchor_ =
        /** @type {!HTMLElement} */ (this.$$('#addPage a[is=action-link]'));
  },

  /** @private */
  destroyUrlDialog_() {
    this.showStartupUrlDialog_ = false;
    this.startupUrlDialogModel_ = null;
    if (this.startupUrlDialogAnchor_) {
      focusWithoutInk(assert(this.startupUrlDialogAnchor_));
      this.startupUrlDialogAnchor_ = null;
    }
  },

  /** @private */
  onUseCurrentPagesTap_() {
    this.browserProxy_.useCurrentPages();
  },

  /**
   * @return {boolean} Whether "Add new page" and "Use current pages" are
   *     allowed.
   * @private
   */
  shouldAllowUrlsEdit_() {
    return this.get('prefs.session.startup_urls.enforcement') !==
        chrome.settingsPrivate.Enforcement.ENFORCED;
  },
});
