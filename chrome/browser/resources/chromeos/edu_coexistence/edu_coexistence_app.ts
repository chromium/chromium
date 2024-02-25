// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './common.css.js';
import './edu_coexistence_template.js';
import './edu_coexistence_button.js';
import './edu_coexistence_error.js';
import './edu_coexistence_offline.js';
import './edu_coexistence_ui.js';
import '../arc_account_picker/arc_account_picker_app.js';
import 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';

import {ArcAccountPickerAppElement} from 'chrome://chrome-signin/arc_account_picker/arc_account_picker_app.js';
import {getAccountAdditionOptionsFromJSON} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {CrViewManagerElement} from 'chrome://resources/ash/common/cr_elements/cr_view_manager/cr_view_manager.js';
import {WebUiListenerMixin} from 'chrome://resources/ash/common/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './edu_coexistence_app.html.js';
import {EduCoexistenceBrowserProxyImpl} from './edu_coexistence_browser_proxy.js';

export enum Screens {
  ONLINE_FLOW = 'edu-coexistence-ui',
  ERROR = 'edu-coexistence-error',
  OFFLINE = 'edu-coexistence-offline',
  ARC_ACCOUNT_PICKER = 'arc-account-picker',
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

      /*
       * True if `kArcAccountRestrictions` feature is enabled.
       */
      isArcAccountRestrictionsEnabled: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isArcAccountRestrictionsEnabled');
        },
        readOnly: true,
      },
    };
  }

  isArcAccountRestrictionsEnabled: boolean;
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
      if (this.currentScreen !== Screens.ERROR &&
          this.currentScreen !== Screens.ARC_ACCOUNT_PICKER) {
        this.switchToScreen(Screens.ONLINE_FLOW);
      }
    });

    window.addEventListener('offline', () => {
      if (this.currentScreen !== Screens.ERROR &&
          this.currentScreen !== Screens.ARC_ACCOUNT_PICKER) {
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
    if (this.isArcAccountRestrictionsEnabled) {
      const options = getAccountAdditionOptionsFromJSON(
          EduCoexistenceBrowserProxyImpl.getInstance().getDialogArguments());
      if (!!options && options.showArcAvailabilityPicker) {
        const arcAccountPicker =
            this.shadowRoot!.querySelector<ArcAccountPickerAppElement>(
                'arc-account-picker-app')!;
        arcAccountPicker.loadAccounts().then(
            (accountsFound: boolean) => {
              this.switchToScreen(
                  accountsFound ? Screens.ARC_ACCOUNT_PICKER : initialScreen);
            },
            () => {
              this.switchToScreen(initialScreen);
            });
        return;
      }
    }
    this.switchToScreen(initialScreen);
  }

  /** Switches to 'Add account' flow. */
  private showAddAccount() {
    this.switchToScreen(
        navigator.onLine ? Screens.ONLINE_FLOW : Screens.OFFLINE);
  }

  /** Attempts to close the dialog. */
  private closeDialog() {
    EduCoexistenceBrowserProxyImpl.getInstance().dialogClose();
  }
}

customElements.define(EduCoexistenceApp.is, EduCoexistenceApp);
