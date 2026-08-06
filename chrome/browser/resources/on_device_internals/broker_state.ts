// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './broker_state.css.js';
import {getHtml} from './broker_state.html.js';
import {DownloadObserverReceiver} from './download_observer.mojom-webui.js';
import type {DownloadObserverInterface} from './download_observer.mojom-webui.js';
import {ModelUnavailableReason} from './model_broker.mojom-webui.js';
import {BrokerAssetState, ModelBrokerDebugObserverReceiver, ModelBrokerDebugRemote} from './model_broker_debug.mojom-webui.js';
import type {BrokerAssetInfo, BrokerStateInfo} from './model_broker_debug.mojom-webui.js';
import {browserProxyFactory} from './on_device_internals_page.mojom-webui.js';
import type {BrowserProxy} from './on_device_internals_page.mojom-webui.js';

const MANIFEST_CRITERIA_NAMES = [
  'Enabled by feature flag',
  'Enabled by enterprise policy',
  'Enabled by user setting',
  'Enough VRAM',
  'Device Capable',
  'Enough disk space to install',
  'Enough disk space to retain',
];

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
    modelCrashCount: null,
    maxModelCrashCount: null,
    isAssetManagerInitialized: false,
  };

  protected get manifestCriteria() {
    return MANIFEST_CRITERIA_NAMES
        .map(name => this.state_.properties.find(p => p.description === name))
        .filter(p => p !== undefined);
  }

  protected get otherProperties() {
    return this.state_.properties.filter(
        p => !MANIFEST_CRITERIA_NAMES.includes(p.description));
  }

  private proxy_: BrowserProxy = browserProxyFactory.getInstance();
  private brokerDebug_ = new ModelBrokerDebugRemote();
  private brokerObserverReceiver_ = new ModelBrokerDebugObserverReceiver(this);
  private downloadObservers_: Map<string, AssetDownloadObserverImpl> =
      new Map();

  constructor() {
    super();
    this.proxy_.handler.bindModelBrokerDebug(
        this.brokerDebug_.$.bindNewPipeAndPassReceiver());
    this.brokerDebug_.addObserver(
        this.brokerObserverReceiver_.$.bindNewPipeAndPassRemote());
    this.getBrokerState_();
  }

  onBrokerStateChanged() {
    this.getBrokerState_();
  }

  updateAssetProgress(
      assetName: string, downloadedBytes: bigint, totalBytes: bigint) {
    const asset = this.state_.assets.find(a => a.name === assetName);
    if (asset) {
      asset.bytesDownloaded = downloadedBytes;
      asset.bytesTotal = totalBytes;
      this.state_ = {...this.state_};
    }
  }

  private async getBrokerState_() {
    this.state_ = (await this.brokerDebug_.getStateInfo()).state;
    this.bindDownloadObservers_();
  }

  private bindDownloadObservers_() {
    for (const asset of this.state_.assets) {
      const isDownloading =
          asset.state === BrokerAssetState.kBackgroundInstalling ||
          asset.state === BrokerAssetState.kForegroundInstalling;

      if (isDownloading && !this.downloadObservers_.has(asset.name)) {
        const observer = new AssetDownloadObserverImpl(this, asset.name);
        this.downloadObservers_.set(asset.name, observer);
        this.brokerDebug_.addAssetDownloadObserver(
            asset.name, observer.getReceiver().$.bindNewPipeAndPassRemote());
      } else if (!isDownloading && this.downloadObservers_.has(asset.name)) {
        // Clean up connection once the asset is no longer actively downloading.
        this.downloadObservers_.get(asset.name)!.getReceiver().$.close();
        this.downloadObservers_.delete(asset.name);
      }
    }
  }

  protected async onUninstallModelsClick_() {
    await this.brokerDebug_.uninstallModels();
    this.getBrokerState_();
  }

  protected async onResetCrashCountClick_() {
    await this.brokerDebug_.resetModelCrashCount();
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

  protected formatBytes(bytes: bigint): string {
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    let val = Number(bytes);
    let pow = 0;
    while (val >= 1024 && pow < units.length - 1) {
      val /= 1024;
      pow++;
    }
    return `${val.toFixed(1)} ${units[pow]}`;
  }

  protected getProgressValue(asset: BrokerAssetInfo): number {
    if (asset.bytesTotal > 0n) {
      return Number(asset.bytesDownloaded);
    }
    return asset.state === BrokerAssetState.kReady ? 1 : 0;
  }

  protected getProgressMax(asset: BrokerAssetInfo): number {
    if (asset.bytesTotal > 0n) {
      return Number(asset.bytesTotal);
    }
    return 1;
  }

  protected getProgressText(asset: BrokerAssetInfo): string {
    if (asset.bytesTotal > 0n) {
      return `${this.formatBytes(asset.bytesDownloaded)} / ` +
          `${this.formatBytes(asset.bytesTotal)}`;
    }
    return asset.state === BrokerAssetState.kReady ? '100%' : '0%';
  }
}

class AssetDownloadObserverImpl implements DownloadObserverInterface {
  private receiver_: DownloadObserverReceiver;

  constructor(
      private element_: OnDeviceInternalsBrokerStateElement,
      private assetName_: string) {
    this.receiver_ = new DownloadObserverReceiver(this);
  }

  getReceiver(): DownloadObserverReceiver {
    return this.receiver_;
  }

  onDownloadProgressUpdate(downloadedBytes: bigint, totalBytes: bigint) {
    this.element_.updateAssetProgress(
        this.assetName_, downloadedBytes, totalBytes);
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
