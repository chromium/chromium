// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_components/app_management/toggle_row.js';

import {AppManagementUserAction, OptionalBool} from 'chrome://resources/cr_components/app_management/constants.js';
import {convertOptionalBoolToBool, recordAppManagementUserAction, toggleOptionalBool} from 'chrome://resources/cr_components/app_management/util.js';
import {assert} from 'chrome://resources/js/assert.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {recordSettingChange} from '../../metrics_recorder.js';

import {BrowserProxy} from './browser_proxy.js';

/** @polymer */
class AppManagementPinToShelfItemElement extends PolymerElement {
  static get is() {
    return 'app-management-pin-to-shelf-item';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {App}
       */
      app: Object,

      /**
       * @type {boolean}
       */
      hidden: {
        type: Boolean,
        computed: 'isAvailable_(app)',
        reflectToAttribute: true,
      },

      /**
       * @type {boolean}
       */
      disabled: {
        type: Boolean,
        computed: 'isManaged_(app)',
        reflectToAttribute: true,
      },
    };
  }

  ready() {
    super.ready();

    this.addEventListener('click', this.onClick_);
    this.addEventListener('change', this.toggleSetting_);
  }

  /**
   * @param {App} app
   * @returns {boolean} true if the app is pinned
   * @private
   */
  getValue_(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.isPinned === OptionalBool.kTrue;
  }

  /**
   * @param {App} app
   * @returns {boolean} true if pinning is available.
   */
  isAvailable_(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.hidePinToShelf;
  }

  /**
   * @param {App} app
   * @returns {boolean} true if the pinning is managed by policy.
   * @private
   */
  isManaged_(app) {
    if (app === undefined) {
      return false;
    }
    assert(app);
    return app.isPolicyPinned === OptionalBool.kTrue;
  }

  /** @private */
  toggleSetting_() {
    const newState = assert(toggleOptionalBool(this.app.isPinned));
    const newStateBool = convertOptionalBoolToBool(newState);
    assert(
        newStateBool ===
        (/** @type {AppManagementToggleRowElement} */ (this.$['toggle-row']))
            .isChecked());
    BrowserProxy.getInstance().handler.setPinned(
        this.app.id,
        newState,
    );
    recordSettingChange();
    const userAction = newStateBool ?
        AppManagementUserAction.PIN_TO_SHELF_TURNED_ON :
        AppManagementUserAction.PIN_TO_SHELF_TURNED_OFF;
    recordAppManagementUserAction(this.app.type, userAction);
  }

  /**
   * @private
   */
  onClick_() {
    this.$['toggle-row'].click();
  }
}

customElements.define(
    AppManagementPinToShelfItemElement.is, AppManagementPinToShelfItemElement);
