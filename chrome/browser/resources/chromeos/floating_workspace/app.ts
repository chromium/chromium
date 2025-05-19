// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '/components/oobe_cr_lottie.js';
import '/components/oobe_icons.html.js';
import '/components/oobe_illo_icons.html.js';
import '/components/oobe_network_icons.html.js';
import '/components/common_styles/oobe_dialog_host_styles.css.js';
import '/components/dialogs/oobe_adaptive_dialog.js';
import '/components/buttons/oobe_text_button.js';
import 'chrome://resources/ash/common/network/network_select.js';

import {NetworkList} from '//resources/ash/common/network/network_list_types.js';
import {$} from '//resources/ash/common/util.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {OobeAdaptiveDialog} from '/components/dialogs/oobe_adaptive_dialog.js';
import {OobeCrLottie} from '/components/oobe_cr_lottie.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {OncMojo} from 'chrome://resources/ash/common/network/onc_mojo.js';
import type {CrosNetworkConfigRemote} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {CrosNetworkConfig, StartConnectResult} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';

import {getTemplate} from './app.html.js';

const EXPECTED_LOAD_TIME_MILLISEC = 10000;

interface NetworkCustomItem {
  customItemType: NetworkList.CustomItemType;
  customItemName: string;
  polymerIcon: string;
  showBeforeNetworksList: boolean;
}

export interface FloatingWorkspace {
  $: {
    defaultDialog: OobeAdaptiveDialog,
    networkErrorDialog: OobeAdaptiveDialog,
    generalErrorDialog: OobeAdaptiveDialog
  };
}

const FloatingWorkspaceBase = I18nMixin(PolymerElement);

export class FloatingWorkspace extends FloatingWorkspaceBase {
  static get is() {
    return 'floating-workspace' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  private networkConfig: CrosNetworkConfigRemote =
      CrosNetworkConfig.getRemote();

  // Main string of the default dialog. It is expected to change after
  // EXPECTED_LOAD_TIME_MILLISEC of time.
  private defaultDialogTitleString: string;

  constructor() {
    super();
    this.defaultDialogTitleString =
        this.i18n('floatingWorkspaceStartupDialogTitle');
  }

  override ready(): void {
    super.ready();

    // Needed to set the right size of <oobe-adaptive-dialog/> when the dialog
    // is shown and when the window resolution changes (e.g. switching to
    // landscape mode).
    window.addEventListener('orientationchange', () => {
      this.onWindowResolutionChange_();
    });
    window.addEventListener('resize', () => {
      this.onWindowResolutionChange_();
    });
    this.onWindowResolutionChange_();

    // This would open one of the 3 screens depending on the state
    // provided in the message handler.
    chrome.send('initialize');
  }

  showDefaultScreen(): void {
    this.hideAllScreens();
    this.$.defaultDialog.hidden = false;

    this.defaultDialogTitleString =
        this.i18n('floatingWorkspaceStartupDialogTitle');
    // Change title when floating workspace takes too long.
    setTimeout(this.onNoResponse.bind(this), EXPECTED_LOAD_TIME_MILLISEC);
    this.$.defaultDialog.onBeforeShow();
    this.$.defaultDialog.show();
    this.playAnimation();
  }

  showNetworkScreen(): void {
    this.hideAllScreens();
    this.$.networkErrorDialog.hidden = false;

    this.$.networkErrorDialog.onBeforeShow();
    this.$.networkErrorDialog.show();
  }

  showErrorScreen(): void {
    this.hideAllScreens();
    this.$.generalErrorDialog.hidden = false;

    this.$.generalErrorDialog.onBeforeShow();
    this.$.generalErrorDialog.show();
  }

  private hideAllScreens(): void {
    this.$.networkErrorDialog.hidden = true;
    this.$.defaultDialog.hidden = true;
    this.$.generalErrorDialog.hidden = true;
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

  // Responsible for "Add WiFi" button.
  private getNetworkCustomItems(): NetworkCustomItem[] {
    return [{
      customItemType: NetworkList.CustomItemType.OOBE,
      customItemName: 'addWiFiListItemName',
      polymerIcon: 'oobe-network-20:add-wifi',
      showBeforeNetworksList: false,
    }];
  }

  private onNetworkItemSelected(
      event: CustomEvent<OncMojo.NetworkStateProperties>) {
    const networkState = event.detail;
    // If the network is already connected, show network details.
    if (OncMojo.connectionStateIsConnected(networkState.connectionState)) {
      chrome.send('showNetworkDetails', [networkState.guid]);
      return;
    }
    // If the network is not connectable, show a configuration dialog.
    if (networkState.connectable === false || networkState.errorState) {
      chrome.send('showNetworkConfig', [networkState.guid]);
      return;
    }
    // Otherwise, connect.
    this.networkConfig.startConnect(networkState.guid).then(response => {
      if (response.result === StartConnectResult.kSuccess) {
        return;
      }
      chrome.send('showNetworkConfig', [networkState.guid]);
    });
  }
  private onCustomItemSelected(event: CustomEvent<{customData: string}>) {
    chrome.send('addNetwork', [event.detail.customData]);
  }

  private onNoResponse(): void {
    this.defaultDialogTitleString =
        this.i18n('floatingWorkspaceStartupDialogLongResponseTitle');
  }

  private onCancelButtonClick_(): void {
    chrome.send('dialogClose', ['stopRestoringSession']);
  }

  private playAnimation(): void {
    const animation = this.shadowRoot?.querySelector('#checkingAnimation');
    if (animation instanceof OobeCrLottie) {
      animation.playing = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FloatingWorkspace.is]: FloatingWorkspace;
  }
}
function initialize() {
  // '$(id)' is an alias for 'document.getElementById(id)'. It is defined
  // in chrome://resources/ash/common/util.js. If this function is not exposed
  // via the global object, it would not be available to tests that inject
  // JavaScript directly into the renderer.
  (window as any).$ = $;
}
customElements.define(FloatingWorkspace.is, FloatingWorkspace);
initialize();
