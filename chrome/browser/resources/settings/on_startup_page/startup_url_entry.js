// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview settings-startup-url-entry represents a UI component that
 * displays a URL that is loaded during startup. It includes a menu that allows
 * the user to edit/remove the entry.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import '../settings_shared_css.js';
import '../site_favicon.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {FocusRowBehavior} from 'chrome://resources/js/cr/ui/focus_row_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {StartupPageInfo, StartupUrlsPageBrowserProxyImpl} from './startup_urls_page_browser_proxy.js';

/**
 * The name of the event fired from this element when the "Edit" option is
 * clicked.
 * @type {string}
 */
export const EDIT_STARTUP_URL_EVENT = 'edit-startup-url';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-startup-url-entry',

  behaviors: [FocusRowBehavior],

  properties: {
    editable: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /** @type {!StartupPageInfo} */
    model: Object,
  },

  /** @private */
  onRemoveTap_() {
    this.$$('cr-action-menu').close();
    StartupUrlsPageBrowserProxyImpl.getInstance().removeStartupPage(
        this.model.modelIndex);
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditTap_(e) {
    e.preventDefault();
    this.$$('cr-action-menu').close();
    this.fire(EDIT_STARTUP_URL_EVENT, {
      model: this.model,
      anchor: this.$$('#dots'),
    });
  },

  /** @private */
  onDotsTap_() {
    const actionMenu =
        /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    actionMenu.showAt(assert(this.$$('#dots')));
  },
});
