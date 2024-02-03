// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// strings.m.js is generated when we enable it via UseStringsJs() in webUI
// controller. When loading it, it will populate data such as localized strings
// into |loadTimeData|.
import './strings.m.js';
import './parent_access_after.js';
import './parent_access_before.js';
import './parent_access_disabled.js';
import './parent_access_error.js';
import './parent_access_offline.js';
import './parent_access_ui.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './parent_access_app.html.js';
import {ParentAccessParams_FlowType, ParentAccessResult} from './parent_access_ui.mojom-webui.js';
import {getParentAccessParams, getParentAccessUiHandler} from './parent_access_ui_handler.js';

export interface ParentAccessApp {
  $: {
    viewManager: CrViewManagerElement,
  };
}

export enum Screens {
  AUTHENTICATION_FLOW = 'parent-access-ui',
  BEFORE_FLOW = 'parent-access-before',
  AFTER_FLOW = 'parent-access-after',
  DISABLED = 'parent-access-disabled',
  ERROR = 'parent-access-error',
  OFFLINE = 'parent-access-offline',
}

export enum ParentAccessEvent {
  SHOW_AFTER = 'show-after',
  SHOW_AUTHENTICATION_FLOW = 'show-authentication-flow',
  SHOW_ERROR = 'show-error',
  // Individual screens can listen for this event to be notified when the screen
  // becomes active.
  ON_SCREEN_SWITCHED = 'on-screen-switched',
}

/**
 * Returns true if the Parent Access Jelly feature flag is enabled.
 * @return {boolean}
 */
export function isParentAccessJellyEnabled() {
  return loadTimeData.valueExists('isParentAccessJellyEnabled') &&
      loadTimeData.getBoolean('isParentAccessJellyEnabled');
}

export class ParentAccessApp extends PolymerElement {
  static get is() {
    return 'parent-access-app';
  }

  static get template() {
    return getTemplate();
  }

  private currentScreen: Screens;

  override ready() {
    super.ready();

    // TODO (b/297564545): Clean up Jelly flag logic after Jelly is enabled.
    if (isParentAccessJellyEnabled()) {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = 'chrome://theme/colors.css?sets=legacy,sys';
      document.head.appendChild(link);
      document.body.classList.add('jelly-enabled');
      /** @suppress {checkTypes} */
      (function() {
        ColorChangeUpdater.forDocument().start();
      })();
    }

    this.addEventListeners();
    this.getInitialScreen().then((initialScreen: Screens) => {
      this.switchScreen(navigator.onLine ? initialScreen : Screens.OFFLINE);
    });
  }

  getCurrentScreenForTest(): Screens {
    return this.currentScreen;
  }

  private addEventListeners() {
    this.addEventListener(ParentAccessEvent.SHOW_AFTER, () => {
      this.switchScreen(Screens.AFTER_FLOW);
    });

    this.addEventListener(ParentAccessEvent.SHOW_AUTHENTICATION_FLOW, () => {
      this.switchScreen(Screens.AUTHENTICATION_FLOW);
      getParentAccessUiHandler().onBeforeScreenDone();
    });

    this.addEventListener(ParentAccessEvent.SHOW_ERROR, () => {
      this.onError();
    });

    window.addEventListener('online', () => {
      // If the app comes back online, start from the initial screen.
      this.getInitialScreen().then((initialScreen: Screens) => {
        this.switchScreen(initialScreen);
      });
    });

    window.addEventListener('offline', () => {
      this.switchScreen(Screens.OFFLINE);
    });
  }

  private async getInitialScreen() {
    const response = await getParentAccessParams();
    if (response!.params.isDisabled) {
      return Screens.DISABLED;
    }
    switch (response!.params.flowType) {
      case ParentAccessParams_FlowType.kExtensionAccess:
        return Screens.BEFORE_FLOW;
      case ParentAccessParams_FlowType.kWebsiteAccess:
      default:
        return Screens.AUTHENTICATION_FLOW;
    }
  }

  /** Shows an error screen, which is a terminal state for the flow. */
  private onError() {
    this.switchScreen(Screens.ERROR);
    getParentAccessUiHandler().onParentAccessDone(ParentAccessResult.kError);
  }

  private switchScreen(screen: Screens) {
    if (this.isAppInTerminalState()) {
      return;
    }
    this.currentScreen = screen;
    this.$.viewManager.switchView(this.currentScreen);
    this.shadowRoot!.querySelector(screen)!.dispatchEvent(
        new CustomEvent(ParentAccessEvent.ON_SCREEN_SWITCHED));
  }

  /** Returns if the app can navigate away from the current screen. */
  private isAppInTerminalState(): boolean {
    return this.currentScreen === Screens.ERROR ||
        this.currentScreen === Screens.DISABLED;
  }
}
customElements.define(ParentAccessApp.is, ParentAccessApp);
