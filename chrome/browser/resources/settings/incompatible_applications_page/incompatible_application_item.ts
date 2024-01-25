// Copyright 2018 The Chromium Authors
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

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '../settings_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './incompatible_application_item.html.js';
import type {IncompatibleApplicationsBrowserProxy} from './incompatible_applications_browser_proxy.js';
import {ActionTypes, IncompatibleApplicationsBrowserProxyImpl} from './incompatible_applications_browser_proxy.js';

const IncompatibleApplicationItemElementBase = I18nMixin(PolymerElement);

export class IncompatibleApplicationItemElement extends
    IncompatibleApplicationItemElementBase {
  static get is() {
    return 'incompatible-application-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The name of the application to be displayed. Also used for the
       * UNINSTALL action, where the name is passed to the
       * startApplicationUninstallation() call.
       */
      applicationName: String,

      /**
       * The type of the action to be taken on this incompatible application.
       * Must be one of BlacklistMessageType in
       * chrome/browser/win/conflicts/proto/module_list.proto.
       */
      actionType: Number,

      /**
       * For the actions MORE_INFO and UPGRADE, this is the URL that must be
       * opened when the action button is tapped.
       */
      actionUrl: String,
    };
  }

  applicationName: string;
  actionType: ActionTypes;
  actionUrl: string;
  private browserProxy_: IncompatibleApplicationsBrowserProxy =
      IncompatibleApplicationsBrowserProxyImpl.getInstance();

  /**
   * Executes the action for this incompatible application, depending on
   * actionType.
   */
  private onActionClick_() {
    if (this.actionType === ActionTypes.UNINSTALL) {
      this.browserProxy_.startApplicationUninstallation(this.applicationName);
    } else if (
        this.actionType === ActionTypes.MORE_INFO ||
        this.actionType === ActionTypes.UPGRADE) {
      this.browserProxy_.openUrl(this.actionUrl);
    } else {
      assertNotReached();
    }
  }

  /**
   * @return The label that should be applied to the action button.
   */
  private getActionName_(actionType: ActionTypes): string {
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
  }
}

customElements.define(
    IncompatibleApplicationItemElement.is, IncompatibleApplicationItemElement);
