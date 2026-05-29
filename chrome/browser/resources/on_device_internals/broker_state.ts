// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './broker_state.css.js';
import {getHtml} from './broker_state.html.js';
import {ModelUnavailableReason} from './model_broker.mojom-webui.js';
import {BrokerAssetState, ModelBrokerDebugRemote} from './model_broker_debug.mojom-webui.js';
import type {BrokerStateInfo} from './model_broker_debug.mojom-webui.js';
import {browserProxyFactory} from './on_device_internals_page.mojom-webui.js';
import type {BrowserProxy} from './on_device_internals_page.mojom-webui.js';

export class OnDeviceInternalsBrokerStateElement extends CrLitElement {
  static get is() {
    return 'on-device-internals-broker-state';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      state_: {type: Object},
    };
  }

  protected accessor state_: BrokerStateInfo = {
    properties: [],
    assets: [],
    useCases: [],
    models: [],
  };

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();
  private brokerDebug_ = new ModelBrokerDebugRemote();

  constructor() {
    super();
    this.proxy_.handler.bindModelBrokerDebug(
        this.brokerDebug_.$.bindNewPipeAndPassReceiver());
    this.getBrokerState_();
  }

  private async getBrokerState_() {
    this.state_ = (await this.brokerDebug_.getStateInfo()).state;
  }

  protected async onUninstallModelsClick_() {
    await this.brokerDebug_.uninstallModels();
    this.getBrokerState_();
  }

  protected async onUseCaseRequestedChange_(e: Event) {
    const useCase = (e.currentTarget as HTMLElement).dataset['useCase']!;
    const requested = (e.currentTarget as HTMLInputElement).checked;
    await this.brokerDebug_.setUseCaseRequested(useCase, requested);
    this.getBrokerState_();
  }

  protected assetStateToString(state: BrokerAssetState): string {
    switch (state) {
      case BrokerAssetState.kNotInstalled:
        return 'Not Installed';
      case BrokerAssetState.kRegistering:
        return 'Registering';
      case BrokerAssetState.kBackgroundInstalling:
        return 'Background Installing';
      case BrokerAssetState.kForegroundInstalling:
        return 'Foreground Installing';
      case BrokerAssetState.kReady:
        return 'Ready';
      case BrokerAssetState.kUninstalling:
        return 'Uninstalling';
      default:
        return 'Unknown';
    }
  }

  protected unavailableReasonToString(reason: ModelUnavailableReason|null):
      string {
    if (reason === null) {
      return 'Available';
    }
    switch (reason) {
      case ModelUnavailableReason.kUnknown:
        return 'Unknown';
      case ModelUnavailableReason.kNotSupported:
        return 'Not Supported';
      case ModelUnavailableReason.kPendingAssets:
        return 'Pending Assets';
      case ModelUnavailableReason.kPendingUsage:
        return 'Pending Usage';
      default:
        return 'Unknown';
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'on-device-internals-broker-state': OnDeviceInternalsBrokerStateElement;
  }
}

customElements.define(
    OnDeviceInternalsBrokerStateElement.is,
    OnDeviceInternalsBrokerStateElement);
