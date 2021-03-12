// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'incompatible-application-item' represents one item in a "list-box" of
 * incompatible applications, as defined in
 * chrome/browser/win/conflicts/incompatible_applications_updater_win.h.
 * This element contains a button that can be used to remove or update the
 * incompatible application, depending on the value of the action-type property.
 *
 * Example usage:
 *
 *   <div class="list-box">
 *     <incompatible-application-item
 *       application-name="Google Chrome"
 *       action-type="1"
 *       action-url="https://www.google.com/chrome/more-info">
 *     </incompatible-application-item>
 *   </div>
 *
 * or
 *
 *   <div class="list-box">
 *     <template is="dom-repeat" items="[[applications]]" as="application">
 *       <incompatible-application-item
 *         application-name="[[application.name]]"
 *         action-type="[[application.actionType]]"
 *         action-url="[[application.actionUrl]]">
 *       </incompatible-application-item>
 *     </template>
 *   </div>
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import '../settings_shared_css.js';

import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ActionTypes, IncompatibleApplicationsBrowserProxy, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_browser_proxy.js';

Polymer({
  is: 'incompatible-application-item',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The name of the application to be displayed. Also used for the UNINSTALL
     * action, where the name is passed to the startApplicationUninstallation()
     * call.
     */
    applicationName: String,

    /**
     * The type of the action to be taken on this incompatible application. Must
     * be one of BlacklistMessageType in
     * chrome/browser/win/conflicts/proto/module_list.proto.
     * @type {!ActionTypes}
     */
    actionType: Number,

    /**
     * For the actions MORE_INFO and UPGRADE, this is the URL that must be
     * opened when the action button is tapped.
     */
    actionUrl: String,
  },

  /** @private {IncompatibleApplicationsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = IncompatibleApplicationsBrowserProxyImpl.getInstance();
  },

  /**
   * Executes the action for this incompatible application, depending on
   * actionType.
   * @private
   */
  onActionTap_() {
    if (this.actionType === ActionTypes.UNINSTALL) {
      this.browserProxy_.startApplicationUninstallation(this.applicationName);
    } else if (
        this.actionType === ActionTypes.MORE_INFO ||
        this.actionType === ActionTypes.UPGRADE) {
      this.browserProxy_.openURL(this.actionUrl);
    } else {
      assertNotReached();
    }
  },

  /**
   * @return {string} The label that should be applied to the action button.
   * @private
   */
  getActionName_(actionType) {
    if (actionType === ActionTypes.UNINSTALL) {
      return this.i18n('incompatibleApplicationsRemoveButton');
    }
    if (actionType === ActionTypes.MORE_INFO) {
      return this.i18n('learnMore');
    }
    if (actionType === ActionTypes.UPGRADE) {
      return this.i18n('incompatibleApplicationsUpdateButton');
    }
    assertNotReached();
  },
});
