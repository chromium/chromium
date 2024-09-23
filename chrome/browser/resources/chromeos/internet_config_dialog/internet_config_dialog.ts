// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/network/network_config.js';
import 'chrome://resources/ash/common/network/network_icon.js';
import 'chrome://resources/ash/common/network/network_shared.css.js';
import 'chrome://resources/ash/common/cr_elements/cros_color_overrides.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/cr_page_host_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {NetworkConfigElement} from 'chrome://resources/ash/common/network/network_config.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert.js';
import {ConfigProperties} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './internet_config_dialog.html.js';

/**
 * @fileoverview
 * 'internet-config-dialog' is used to configure a new or existing network
 * outside of settings (e.g. from the login screen or when configuring a
 * new network from the system tray).
 */

export interface InternetConfigDialogElement {
  $: {
    networkConfig: NetworkConfigElement,
    dialog: CrDialogElement,
  };
}

const InternetConfigDialogElementBase = I18nMixin(PolymerElement);

export class InternetConfigDialogElement extends
    InternetConfigDialogElementBase {
  static get is() {
    return 'internet-config-dialog' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      shareAllowEnable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shareNetworkAllowEnable');
        },
      },

      shareDefault_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('shareNetworkDefault');
        },
      },

      /**
       * The network GUID to configure, or empty when configuring a new network.
       */
      guid_: String,

      /**
       * The type of network to be configured as a string. May be set initially
       * or updated by network-config.
       */
      type_: String,

      /**
       * The network configuration which the network dialog will prefill. Can be
       * empty if nothing to prefill or the information will be synced based on
       * given guid.
       */
      prefilledProperties_: ConfigProperties,

      enableConnect_: Boolean,

      /**
       * Whether the connection has been attempted.
       */
      connectClicked_: Boolean,

      /**
       * Set by network-config when a configuration error occurs.
       */
      error_: {
        type: String,
        value: '',
      },
    };
  }

  private shareAllowEnable_: boolean;
  private shareDefault_: boolean;
  private guid_: string;
  private type_: string;
  private prefilledProperties_: ConfigProperties|null;
  private enableConnect_: boolean;
  private connectClicked_: boolean;
  private error_: string;

  override connectedCallback() {
    super.connectedCallback();

    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (dialogArgs) {
      const args = JSON.parse(dialogArgs);
      this.type_ = args.type;
      assert(this.type_);
      this.guid_ = args.guid || '';
      this.prefilledProperties_ = args.prefilledProperties || null;
    } else {
      // For debugging
      const params = new URLSearchParams(document.location.search.substring(1));
      this.type_ = params.get('type') || 'WiFi';
      this.guid_ = params.get('guid') || '';
      this.prefilledProperties_ = null;
    }
    this.connectClicked_ = false;

    ColorChangeUpdater.forDocument().start();

    this.$.networkConfig.init();

    this.$.dialog.showModal();
  }

  private close_(): void {
    chrome.send('dialogClose');
  }

  private getDialogTitle_(): string {
    const type = this.i18n('OncType' + this.type_);
    return this.i18n('internetJoinType', type);
  }

  private shouldShowError_(): boolean {
    // Do not show "out-of-range" error if the dialog is just opened.
    if (!this.connectClicked_ && this.error_ === 'out-of-range') {
      return false;
    }
    return !!this.error_;
  }

  private getError_(): string {
    if (this.i18nExists(this.error_)) {
      return this.i18n(this.error_);
    }
    return this.i18n('networkErrorUnknown');
  }

  private onCancelClick_(): void {
    this.close_();
  }

  private onConnectClick_(): void {
    this.$.networkConfig.connect();
    this.connectClicked_ = true;
  }

  setErrorForTesting(error: string): void {
    this.error_ = error;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [InternetConfigDialogElement.is]: InternetConfigDialogElement;
  }
}

customElements.define(
    InternetConfigDialogElement.is, InternetConfigDialogElement);
