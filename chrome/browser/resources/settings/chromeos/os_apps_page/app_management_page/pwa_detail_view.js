// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './app_details_item.js';
import './pin_to_shelf_item.js';
import './supported_links_item.js';
import './shared_style.js';
import 'chrome://resources/cr_components/app_management/file_handling_item.js';
import 'chrome://resources/cr_components/app_management/icons.html.js';
import 'chrome://resources/cr_components/app_management/more_permissions_item.js';
import 'chrome://resources/cr_components/app_management/permission_item.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {getAppIcon, getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreClient, AppManagementStoreClientInterface} from './store_client.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 */
const AppManagementPwaDetailViewElementBase =
    mixinBehaviors([AppManagementStoreClient], PolymerElement);

/** @polymer */
class AppManagementPwaDetailViewElement extends
    AppManagementPwaDetailViewElementBase {
  static get is() {
    return 'app-management-pwa-detail-view';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @private {App}
       */
      app_: Object,

      /**
       * @private {boolean}
       */
      listExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();

    this.listExpanded_ = false;
  }

  /** @private */
  toggleListExpanded_() {
    this.listExpanded_ = !this.listExpanded_;
  }

  /**
   * @param {App} app
   * @return {string}
   * @private
   */
  iconUrlFromId_(app) {
    return getAppIcon(app);
  }

  /**
   * @param {boolean} listExpanded
   * @return {string}
   * @private
   */
  getCollapsedIcon_(listExpanded) {
    return listExpanded ? 'cr:expand-less' : 'cr:expand-more';
  }
}

customElements.define(
    AppManagementPwaDetailViewElement.is, AppManagementPwaDetailViewElement);
