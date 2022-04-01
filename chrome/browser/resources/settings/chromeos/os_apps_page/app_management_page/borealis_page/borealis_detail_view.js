// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kBorealisClientAppId = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

import {Polymer, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '../pin_to_shelf_item.js';
import '../shared_style.js';
import '//resources/cr_components/app_management/icons.js';
import '//resources/cr_components/app_management/permission_item.js';
import '//resources/cr_elements/icons.m.js';

import {AppManagementStoreClient} from '../store_client.js';
import {getSelectedApp} from 'chrome://resources/cr_components/app_management/util.js';
import {routes} from '../../../os_route.js';
import {Router} from '../../../../router.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-borealis-detail-view',

  behaviors: [
    AppManagementStoreClient,
  ],

  properties: {
    /** @private {App} */
    app_: {
      type: Object,
    }
  },

  attached() {
    // When the state is changed, get the new selected app and assign it to
    // |app_|
    this.watch('app_', state => getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @return {boolean}
   * @protected
   */
  isMainApp_() {
    return this.app_.id === kBorealisClientAppId;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onBorealisLinkClicked_(event) {
    event.detail.event.preventDefault();
    const params = new URLSearchParams();
    params.append('id', kBorealisClientAppId);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
  },
});
