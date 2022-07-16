// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {assert} from '//resources/js/assert.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreClient} from './store_client.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'app-management-supported-links-overlapping-apps-dialog',

  behaviors: [
    AppManagementStoreClient,
    I18nBehavior,
  ],

  properties: {
    /** @type {!App} */
    app: Object,

    /**
     * @private {AppMap}
     */
    apps_: {
      type: Object,
    },

    /**
     * @private {Array<string>}
     */
    overlappingAppIds: {
      type: Array,
    },
  },

  attached() {
    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  },

  getBodyText_(apps) {
    const appNames = this.overlappingAppIds.map(app_id => {
      assert(apps[app_id]);
      return apps[app_id].title;
    });

    const appTitle = this.app.title;
    assert(appTitle);

    switch (appNames.length) {
      case 1:
        return this.i18n(
            'appManagementIntentOverlapDialogText1App', appTitle, appNames[0]);
      case 2:
        return this.i18n(
            'appManagementIntentOverlapDialogText2Apps', appTitle, ...appNames);
      case 3:
        return this.i18n(
            'appManagementIntentOverlapDialogText3Apps', appTitle, ...appNames);
      case 4:
        return this.i18n(
            'appManagementIntentOverlapDialogText4Apps', appTitle,
            ...appNames.slice(0, 3));
      default:
        return this.i18n(
            'appManagementIntentOverlapDialogText5OrMoreApps', appTitle,
            ...appNames.slice(0, 3), appNames.length - 3);
    }
  },

  wasConfirmed() {
    return this.$.dialog.getNative().returnValue === 'success';
  },

  onChangeClick_() {
    this.$.dialog.close();
  },

  onCancelClick_() {
    this.$.dialog.cancel();
  },

});
