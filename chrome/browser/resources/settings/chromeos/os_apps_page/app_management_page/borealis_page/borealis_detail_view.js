// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kBorealisMainAppId = 'epfhbkiklgmlkhfpbcdleadnhcfdjfmo';

Polymer({
  is: 'app-management-borealis-detail-view',

  behaviors: [
    app_management.AppManagementStoreClient,
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
    this.watch('app_', state => app_management.util.getSelectedApp(state));
    this.updateFromStore();
  },

  /**
   * @return {boolean}
   * @protected
   */
  isMainApp_() {
    return this.app_.id === kBorealisMainAppId;
  },

  /**
   * @param {!Event} event
   * @private
   */
  onBorealisLinkClicked_(event) {
    event.detail.event.preventDefault();
    const params = new URLSearchParams;
    params.append('id', kBorealisMainAppId);
    Router.getInstance().navigateTo(routes.APP_MANAGEMENT_DETAIL, params);
  },
});
