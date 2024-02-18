// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen implementation.
 */

import '//resources/js/action_link.js';
import '../../components/throbber_notice.js';

import {ensureTransitionEndEvent} from '//resources/js/util.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';

import {getTemplate} from './app_launch_splash.html.js';

const AppLaunchSplashBase =
    mixinBehaviors([OobeI18nBehavior, LoginScreenBehavior], PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface,
    };

interface AppData {
  name: string;
  iconURL: string;
  url: string;
}

interface AppLaunchSplashScreenData {
  shortcutEnabled: boolean;
  appInfo: AppData;
}

class AppLaunchSplash extends AppLaunchSplashBase {
  static get is() {
    return 'app-launch-splash-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      appName: {
        type: String,
        value: '',
      },
      appUrl: {
        type: String,
        value: '',
      },
      launchText: {
        type: String,
        value: '',
      },
    };
  }

  private appName: string;
  private appUrl: string;
  private launchText: string;

  override get EXTERNAL_API(): string[] {
    return ['toggleNetworkConfig', 'updateApp', 'updateMessage'];
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AppLaunchSplashScreen');

    const networkContainer =
        this.shadowRoot!.getElementById('configNetworkContainer')!;
    networkContainer.addEventListener(
        'transitionend', this.onConfigNetworkTransitionend.bind(this));

    // Ensure the transitionend event gets called after a wait time.
    // The wait time should be inline with the transition duration time
    // defined in css file. The current value in css is 1000ms. To avoid
    // the emulated transitionend firing before real one, a 1050ms
    // delay is used.
    ensureTransitionEndEvent((networkContainer), 1050);
  }

  /** Initial UI State for screen */
  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.KIOSK;
  }

  private onConfigNetwork(): void {
    chrome.send('configureNetwork');
  }

  private onConfigNetworkTransitionend(): void {
    if (this.shadowRoot!.getElementById('configNetworkContainer')!.classList
            .contains('faded')) {
      this.shadowRoot!.getElementById('configNetwork')!.hidden = true;
    }
  }

  /**
   * Event handler that is invoked just before the frame is shown.
   * @param data Screen init payload.
   */
  onBeforeShow(data: AppLaunchSplashScreenData): void {
    this.shadowRoot!.getElementById('configNetwork')!.hidden = true;
    this.toggleNetworkConfig(false);
    this.updateApp(data['appInfo']);

    this.shadowRoot!.getElementById('shortcutInfo')!.hidden =
        !data['shortcutEnabled'];
  }

  /**
   * Toggles visibility of the network configuration option.
   * @param visible Whether to show the option.
   */
  toggleNetworkConfig(visible: boolean): void {
    const currVisible =
        !this.shadowRoot!.getElementById('configNetworkContainer')!.classList
             .contains('faded');
    if (currVisible === visible) {
      return;
    }

    if (visible) {
      this.shadowRoot!.getElementById('configNetwork')!.hidden = false;
      this.shadowRoot!.getElementById(
                          'configNetworkContainer')!.classList.remove('faded');
    } else {
      this.shadowRoot!.getElementById('configNetworkContainer')!.classList.add(
          'faded');
    }
  }

  /**
   * Updates the app name and icon.
   * @param app Details of app being launched.
   */
  updateApp(app: AppData): void {
    this.appName = app.name;
    this.appUrl = app.url;
    this.shadowRoot!.getElementById('header')!.style.backgroundImage =
        'url(' + app.iconURL + ')';
  }

  /**
   * Updates the message for the current launch state.
   * @param message Description for current launch state.
   */
  updateMessage(message: string): void {
    this.launchText = message;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AppLaunchSplash.is]: AppLaunchSplash;
  }
}

customElements.define(AppLaunchSplash.is, AppLaunchSplash);
