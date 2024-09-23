// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/ash/common/cr_elements/localized_link/localized_link.js';
import 'chrome://resources/cros_components/button/button.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './components/android_apps_list.js';
import './components/common_styles/oobe_dialog_host_styles.css.js';
import './components/dialogs/oobe_adaptive_dialog.js';
import './icons.html.js';

import type {OobeAdaptiveDialog} from '//chromeos/login/components/dialogs/oobe_adaptive_dialog.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import type {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import type {App} from './extended_updates.mojom-webui.js';
import {ExtendedUpdatesBrowserProxy} from './extended_updates_browser_proxy.js';

export interface ExtendedUpdatesAppElement {
  $: {
    extendedUpdatesDialog: OobeAdaptiveDialog,
  };
}

export class ExtendedUpdatesAppElement extends PolymerElement {
  static get is() {
    return 'extended-updates-app' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * True if confirmation dialog backdrop should be hidden.
       */
      shouldHideBackdrop_: {
        type: Boolean,
        value: true,
      },
    };
  }

  override ready(): void {
    super.ready();

    // Needed to set the right size of <oobe-adaptive-dialog/> when the dialog
    // is shown and when the window resolution changes (e.g. switching to
    // landscape mode).
    this.onWindowResolutionChange_();
    window.addEventListener('orientationchange', () => {
      this.onWindowResolutionChange_();
    });
    window.addEventListener('resize', () => {
      this.onWindowResolutionChange_();
    });

    // Listen for changes to Jelly dynamic colors when a user switches between
    // dark and light modes.
    ColorChangeUpdater.forDocument().start();

    // Initialize and show <oobe-adaptive-dialog/>.
    this.$.extendedUpdatesDialog.onBeforeShow();
    this.$.extendedUpdatesDialog.show();

    this.browserProxy_.getInstalledAndroidApps().then((apps: App[]) => {
      this.apps = apps;
    });
  }

  private shouldHideBackdrop_: boolean;
  // Shows the confirmation popup when true.
  private showPopup_: boolean;

  private apps: App[];

  private browserProxy_: ExtendedUpdatesBrowserProxy;

  constructor() {
    super();

    this.apps = [];
    this.browserProxy_ = ExtendedUpdatesBrowserProxy.getInstance();
  }

  private onWindowResolutionChange_(): void {
    if (!document.documentElement.hasAttribute('screen')) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');
      if (window.innerWidth > window.innerHeight) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    }
  }

  private onEnableButtonClick_(): void {
    this.showPopup_ = true;
  }

  private onCancelButtonClick_(): void {
    this.browserProxy_.closeDialog();
  }

  private onPopupConfirmButtonClick_(): void {
    this.browserProxy_.optInToExtendedUpdates();
    this.browserProxy_.closeDialog();
  }

  private onPopupCancelButtonClick_(): void {
    this.showPopup_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ExtendedUpdatesAppElement.is]: ExtendedUpdatesAppElement;
  }
}

customElements.define(ExtendedUpdatesAppElement.is, ExtendedUpdatesAppElement);
