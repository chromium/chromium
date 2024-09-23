// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App install/launch splash screen implementation.
 */

import '//resources/js/action_link.js';
import '../../components/throbber_notice.js';

import {assert} from '//resources/js/assert.js';
import {ensureTransitionEndEvent} from '//resources/js/util.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './app_launch_splash.html.js';

const AppLaunchSplashBase = LoginScreenMixin(OobeI18nMixin(PolymerElement));

interface AppData {
  name: string;
  iconURL: string;
  url: string;
}

interface AppLaunchSplashScreenData {
  shortcutEnabled: boolean;
  appInfo: AppData;
}

enum UserAction {
  CONFIGURE_NETWORK = 'configure-network',
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
    return ['toggleNetworkConfig', 'setAppData', 'updateMessage'];
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
    this.userActed(UserAction.CONFIGURE_NETWORK);
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
  override onBeforeShow(data?: AppLaunchSplashScreenData): void {
    super.onBeforeShow(data);
    assert(this.shadowRoot);
    this.shadowRoot.getElementById('configNetwork')!.hidden = true;
    this.toggleNetworkConfig(false);
    // If the screen is reshown from the ErrorScreen using the default callback
    // data might be undefined.
    if (data) {
      this.setAppData(data);
    }
  }

  setAppData(data: AppLaunchSplashScreenData): void {
    const appInfo: AppData = data['appInfo'];
    this.appName = appInfo.name;
    this.appUrl = appInfo.url;
    const header = this.shadowRoot!.getElementById('header');
    assert(header instanceof HTMLElement);
    header.style.backgroundImage = 'url(' + appInfo.iconURL + ')';

    const shortcutInfo = this.shadowRoot!.getElementById('shortcutInfo');
    assert(shortcutInfo instanceof HTMLElement);
    shortcutInfo.hidden = !data['shortcutEnabled'];
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
