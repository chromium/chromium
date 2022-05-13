// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AppManagementStoreClient, AppManagementStoreClientInterface} from './store_client.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {AppManagementStoreClientInterface}
 * @implements {I18nBehaviorInterface}
 */
const AppManagementSupportedLinksOverlappingAppsDialogElementBase =
    mixinBehaviors([AppManagementStoreClient, I18nBehavior], PolymerElement);

/** @polymer */
class AppManagementSupportedLinksOverlappingAppsDialogElement extends
    AppManagementSupportedLinksOverlappingAppsDialogElementBase {
  static get is() {
    return 'app-management-supported-links-overlapping-apps-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
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
    };
  }

  connectedCallback() {
    super.connectedCallback();

    this.watch('apps_', state => state.apps);
    this.updateFromStore();
  }

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
  }

  wasConfirmed() {
    return this.$.dialog.getNative().returnValue === 'success';
  }

  /** @private */
  onChangeClick_() {
    this.$.dialog.close();
  }

  /** @private */
  onCancelClick_() {
    this.$.dialog.cancel();
  }
}

customElements.define(
    AppManagementSupportedLinksOverlappingAppsDialogElement.is,
    AppManagementSupportedLinksOverlappingAppsDialogElement);
