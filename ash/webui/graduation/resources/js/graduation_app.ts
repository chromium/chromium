// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './graduation_error.js';
import './graduation_offline.js';
import './graduation_takeout_ui.js';
import './graduation_welcome.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AuthResult, GraduationScreen} from '../mojom/graduation_ui.mojom-webui.js';

import {getTemplate} from './graduation_app.html.js';
import {getGraduationUiHandler} from './graduation_ui_handler.js';

export enum Screens {
  /**
   * WELCOME: The welcome page shown on app launch.
   * TAKEOUT_UI: The screen containing the Takeout webview.
   * ERROR: The screen shown permanently after an error event.
   * OFFLINE: The screen shown when the device is offline.
   */
  WELCOME = 'graduation-welcome',
  TAKEOUT_UI = 'graduation-takeout-ui',
  ERROR = 'graduation-error',
  OFFLINE = 'graduation-offline',
}

export enum ScreenSwitchEvents {
  /**
   * SHOW_WELCOME: The event that triggers the welcome screen.
   * SHOW_TAKEOUT_UI: The event that triggers the Takeout webview screen.
   * SHOW_ERROR: The event that triggers the error screen.
   * ONLINE: The window event that triggers the welcome screen.
   * OFFLINE: The window event that triggers the offline screen.
   */
  SHOW_WELCOME = 'show-welcome-screen',
  SHOW_TAKEOUT_UI = 'show-takeout-screen',
  SHOW_ERROR = 'show-error-screen',
  ONLINE = 'online',
  OFFLINE = 'offline',
}

/**
 * The event that is fired on the current screen signaling that the app has
 * switched to it. Screens can listen for this event and perform actions on
 * becoming active.
 */
export const ScreenSwitchedEvent: string = 'on-screen-switched';

export interface GraduationApp {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export class GraduationApp extends PolymerElement {
  static get is() {
    return 'graduation-app';
  }

  static get template() {
    return getTemplate();
  }

  private authResult: AuthResult|null = null;
  private currentScreen: Screens;

  override ready() {
    super.ready();
    this.addEventListeners();

    if (!navigator.onLine) {
      this.switchToScreen(Screens.OFFLINE);
      return;
    }

    this.authenticate();
    this.switchToScreen(Screens.WELCOME);
  }

  private async authenticate(): Promise<void> {
    const authResult = await getGraduationUiHandler().authenticateWebview();
    this.authResult = authResult.result;
    this.shadowRoot!.querySelector(Screens.TAKEOUT_UI)!.onAuthComplete(
        this.authResult);
  }

  getCurrentScreenForTest(): Screens {
    return this.currentScreen;
  }

  private addEventListeners() {
    this.addEventListener(ScreenSwitchEvents.SHOW_TAKEOUT_UI, () => {
      this.switchToScreen(Screens.TAKEOUT_UI);
    });

    this.addEventListener(ScreenSwitchEvents.SHOW_WELCOME, () => {
      this.switchToScreen(Screens.WELCOME);
    });

    this.addEventListener(ScreenSwitchEvents.SHOW_ERROR, () => {
      // An error caused by an offline event can surface after the offline
      // screen is already shown. Don't show the error screen so the app has a
      // chance to reload when it comes online.
      if (this.currentScreen === Screens.OFFLINE) {
        return;
      }
      this.switchToScreen(Screens.ERROR);
    });

    window.addEventListener(ScreenSwitchEvents.ONLINE, () => {
      // If there is no authentication result, authentication has not been
      // attempted for the lifetime of the app, so authenticate now.
      if (this.authResult === null) {
        this.authenticate();
      }
      // If the app comes back online, start from the initial screen.
      this.switchToScreen(Screens.WELCOME);
    });

    window.addEventListener(ScreenSwitchEvents.OFFLINE, () => {
      this.switchToScreen(Screens.OFFLINE);
    });
  }

  private switchToScreen(screen: Screens) {
    if (!this.canSwitchToScreen(screen)) {
      return;
    }
    this.currentScreen = screen;
    this.$.viewManager.switchView(this.currentScreen);
    this.onScreenSwitched(this.currentScreen);
  }

  private canSwitchToScreen(screen: Screens): boolean {
    return this.currentScreen !== screen &&
        this.currentScreen !== Screens.ERROR;
  }

  private onScreenSwitched(screen: Screens) {
    switch (screen) {
      case Screens.WELCOME:
        getGraduationUiHandler().onScreenSwitched(GraduationScreen.kWelcome);
        break;
      case Screens.TAKEOUT_UI:
        getGraduationUiHandler().onScreenSwitched(GraduationScreen.kTakeoutUi);
        break;
      case Screens.ERROR:
        getGraduationUiHandler().onScreenSwitched(GraduationScreen.kError);
        break;
    }
    this.shadowRoot!.querySelector(screen)!.dispatchEvent(
        new CustomEvent(ScreenSwitchedEvent));
  }
}
customElements.define(GraduationApp.is, GraduationApp);
ColorChangeUpdater.forDocument().start();
