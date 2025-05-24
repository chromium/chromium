// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './common.css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './edu_coexistence_error.js';
import './edu_coexistence_offline.js';
import './edu_coexistence_ui.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import type {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './edu_coexistence_app.html.js';

export enum Screens {
  ONLINE_FLOW = 'edu-coexistence-ui',
  ERROR = 'edu-coexistence-error',
  OFFLINE = 'edu-coexistence-offline',
}

export interface EduCoexistenceApp {
  $: {
    viewManager: CrViewManagerElement,
  };
}

const EduCoexistenceAppBase = WebUiListenerMixin(PolymerElement);

export class EduCoexistenceApp extends EduCoexistenceAppBase {
  static get is() {
    return 'edu-coexistence-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Whether the error screen should be shown.
       */
      isErrorShown: {
        type: Boolean,
        value: false,
      },
    };
  }

  isErrorShown: boolean;
  currentScreen: Screens;

  override ready() {
    super.ready();
    this.addWebUiListener('show-error-screen', () => {
      this.onError();
    });

    this.addEventListener('go-error', () => {
      this.onError();
    });

    window.addEventListener('online', () => {
      if (this.currentScreen !== Screens.ERROR) {
        this.switchToScreen(Screens.ONLINE_FLOW);
      }
    });

    window.addEventListener('offline', () => {
      if (this.currentScreen !== Screens.ERROR) {
        this.switchToScreen(Screens.OFFLINE);
      }
    });
    this.setInitialScreen(navigator.onLine);
  }

  getCurrentScreenForTest(): Screens {
    return this.currentScreen;
  }

  private onError() {
    this.switchToScreen(Screens.ERROR);
  }

  /** Switches to the specified screen. */
  private switchToScreen(screen: Screens) {
    if (this.currentScreen === screen) {
      return;
    }
    this.currentScreen = screen;
    this.$.viewManager.switchView(this.currentScreen);
    this.dispatchEvent(new CustomEvent('switch-view-notify-for-testing'));
  }

  private setInitialScreen(isOnline: boolean) {
    const initialScreen = isOnline ? Screens.ONLINE_FLOW : Screens.OFFLINE;
    this.switchToScreen(initialScreen);
  }
}

customElements.define(EduCoexistenceApp.is, EduCoexistenceApp);
