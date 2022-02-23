// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './supported_links_overlapping_apps_dialog.js';
import './supported_links_dialog.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';
import '//resources/cr_elements/cr_radio_button/cr_radio_button.m.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.m.js';

import {AppManagementUserAction, AppType} from '//resources/cr_components/app_management/constants.js';
import {assert} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {recordAppManagementUserAction} from 'chrome://resources/cr_components/app_management/util.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {recordSettingChange} from '../../metrics_recorder.m.js';

import {BrowserProxy} from './browser_proxy.js';
import {AppManagementStoreClient} from './store_client.js';

class AppManagementAppDetailsItem extends PolymerElement {
  static get is() {
    return 'app-management-app-details-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!App} */
      app: Object,

      /**
       * @type {boolean}
       */
      hidden_: {
        type: Boolean,
        computed: 'isHidden_()',
        reflectToAttribute: true,
      },
    };
  }

  /**
   * The supported links item is not available when the feature
   * flag is disabled.
   *
   * @returns {boolean}
   * @private
   */
  isHidden_() {
    return !loadTimeData.getBoolean('appManagementAppDetailsEnabled');
  }
}

customElements.define(
    AppManagementAppDetailsItem.is, AppManagementAppDetailsItem);