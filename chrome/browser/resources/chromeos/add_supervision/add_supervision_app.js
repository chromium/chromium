// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './add_supervision_ui.js';
import './supervision/supervised_user_error.js';
import './supervision/supervised_user_offline.js';
import 'chrome://resources/cr_elements/cr_view_manager/cr_view_manager.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


/** @enum {string} */
const Screens = {
  /**
   * ERROR: Shown permanently after an error event.
   * OFFLINE: Shown when the device is offline.
   * ONLINE: Shown when the device is online.
   */
  ERROR: 'supervised-user-error',
  OFFLINE: 'supervised-user-offline',
  ONLINE: 'add-supervision-ui',
};

class AddSupervisionApp extends PolymerElement {
  static get is() {
    return 'add-supervision-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Specifies what the current screen is.
       * @private {Screens}
       */
      currentScreen_: {
        type: Screens,
      },
    };
  }

  /** @override */
  ready() {
    super.ready();
    this.addEventListeners_();
    this.switchToScreen_(navigator.onLine ? Screens.ONLINE : Screens.OFFLINE);
  }

  /** @private */
  addEventListeners_() {
    window.addEventListener('online', () => {
      this.switchToScreen_(Screens.ONLINE);
    });

    window.addEventListener('offline', () => {
      this.switchToScreen_(Screens.OFFLINE);
    });

    this.addEventListener('show-error', () => {
      this.switchToScreen_(Screens.ERROR);
    });
  }

  /**
   * Switches to the specified screen.
   * @param {Screens} screen
   * @private
   */
  switchToScreen_(screen) {
    if (this.isinvalidScreenSwitch_(screen)) {
      return;
    }
    this.currentScreen_ = screen;
    /** @type {CrViewManagerElement} */ (this.$.viewManager)
        .switchView(this.currentScreen_);
  }

  /**
   * Returns true if the app cannot navigate from the current screen to the
   * screen provided.
   * @param {Screens} screen
   * @return {boolean}
   * @private
   */
  isinvalidScreenSwitch_(screen) {
    return this.currentScreen_ === screen ||
        this.currentScreen_ === Screens.ERROR;
  }
}
customElements.define(AddSupervisionApp.is, AddSupervisionApp);
