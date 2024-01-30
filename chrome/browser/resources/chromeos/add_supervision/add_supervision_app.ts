// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './add_supervision_ui.js';
import './supervision/supervised_user_error.js';
import './supervision/supervised_user_offline.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './add_supervision_app.html.js';

enum Screens {
  /**
   * ERROR: Shown permanently after an error event.
   * OFFLINE: Shown when the device is offline.
   * ONLINE: Shown when the device is online.
   */
  ERROR = 'supervised-user-error',
  OFFLINE = 'supervised-user-offline',
  ONLINE = 'add-supervision-ui',
}

interface AddSupervisionApp {
  $: {
    viewManager: CrViewManagerElement,
  };
}

class AddSupervisionApp extends PolymerElement {
  static get is() {
    return 'add-supervision-app';
  }

  static get template() {
    return getTemplate();
  }

  private currentScreen: Screens;

  override ready() {
    super.ready();
    this.addEventListeners();
    this.switchToScreen(navigator.onLine ? Screens.ONLINE : Screens.OFFLINE);
  }

  private addEventListeners() {
    window.addEventListener('online', () => {
      this.switchToScreen(Screens.ONLINE);
    });

    window.addEventListener('offline', () => {
      this.switchToScreen(Screens.OFFLINE);
    });

    this.addEventListener('show-error', () => {
      this.switchToScreen(Screens.ERROR);
    });
  }

  private switchToScreen(screen: Screens) {
    if (this.isinvalidScreenSwitch(screen)) {
      return;
    }
    this.currentScreen = screen;
    this.$.viewManager.switchView(this.currentScreen);
  }

  private isinvalidScreenSwitch(screen: Screens): boolean {
    return this.currentScreen === screen ||
        this.currentScreen === Screens.ERROR;
  }
}
customElements.define(AddSupervisionApp.is, AddSupervisionApp);
