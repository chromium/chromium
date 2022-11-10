// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {DeviceTrustState, Int32Value, KeyInfo, KeyManagerInitializedValue, KeyTrustLevel, KeyType, PageHandler, PageHandlerInterface} from './connectors_internals.mojom-webui.js';
import {getTemplate} from './device_trust_connector.html.js';

const TrustLevelStringMap = {
  [KeyTrustLevel.UNSPECIFIED]: 'Unspecified',
  [KeyTrustLevel.HW]: 'HW',
  [KeyTrustLevel.OS]: 'OS',
};

const KeyTypeStringMap = {
  [KeyType.UNKNOWN]: 'Unknown',
  [KeyType.RSA]: 'RSA',
  [KeyType.EC]: 'EC',
};

export class DeviceTrustConnectorElement extends CustomElement {
  static get is() {
    return 'device-trust-connector';
  }

  static override get template() {
    return getTemplate();
  }

  public set enabledString(str: string) {
    const strEl = (this.$('#enabled-string') as HTMLElement);
    if (strEl) {
      strEl.innerText = str;
    } else {
      console.error('Could not find #enabled-string element.');
    }
  }

  public set keyInfo(keyInfo: KeyInfo) {
    const initRowEl = (this.$('#key-manager-row') as HTMLElement);
    const initStateEl = (this.$('#key-manager-state') as HTMLElement);

    const metadataRowEl = (this.$('#key-metadata-row') as HTMLElement);
    const trustLevelStateEl = (this.$('#key-trust-level') as HTMLElement);
    const keyTypeStateEl = (this.$('#key-type') as HTMLElement);
    const spkiHashStateEl = (this.$('#spki-hash') as HTMLElement);
    const keySyncStateEl = (this.$('#key-sync') as HTMLElement);

    const initializedValue = keyInfo.isKeyManagerInitialized;
    if (initializedValue === KeyManagerInitializedValue.UNSUPPORTED) {
      this.hideElement(initRowEl);
      this.hideElement(metadataRowEl);
    } else {
      const keyLoaded =
          initializedValue === KeyManagerInitializedValue.KEY_LOADED;
      initStateEl.innerText = keyLoaded ? 'true' : 'false';
      this.showElement(initRowEl);

      if (keyLoaded) {
        trustLevelStateEl.innerText =
            this.trustLevelToString(keyInfo.trustLevel);
        keyTypeStateEl.innerText = this.keyTypeToString(keyInfo.keyType);
        spkiHashStateEl.innerText = keyInfo.encodedSpkiHash;
        keySyncStateEl.innerText =
            this.keySyncCodeToString(keyInfo.syncKeyResponseCode);

        this.showElement(metadataRowEl);
      } else {
        this.hideElement(metadataRowEl);
      }
    }
  }

  private signalsString_: string = '';
  public set signalsString(str: string) {
    const signalsEl = (this.$('#signals') as HTMLElement);
    if (signalsEl) {
      signalsEl.innerText = str;
      this.signalsString_ = str;
    } else {
      console.error('Could not find #signals element.');
    }
  }

  public get copyButton(): HTMLButtonElement|undefined {
    return this.$('#copy-signals') as HTMLButtonElement;
  }

  public get signalsString(): string {
    return this.signalsString_;
  }

  private readonly pageHandler: PageHandlerInterface;

  constructor() {
    super();
    this.pageHandler = PageHandler.getRemote();

    this.fetchDeviceTrustValues()
        .then(state => this.setDeviceTrustValues(state))
        .then(() => {
          const copyButton = this.copyButton;
          if (copyButton) {
            copyButton.addEventListener(
                'click', () => this.copySignals(copyButton));
          }
        });
  }

  private setDeviceTrustValues(state: DeviceTrustState|undefined) {
    if (!state) {
      this.enabledString = 'error';
      return;
    }

    this.enabledString = `${state.isEnabled}`;

    this.keyInfo = state.keyInfo;

    this.signalsString = state.signalsJson;
  }

  private async fetchDeviceTrustValues(): Promise<DeviceTrustState|undefined> {
    return this.pageHandler.getDeviceTrustState().then(
        (response: {state: DeviceTrustState}) => response && response.state,
        (e: object) => {
          console.warn(`fetchDeviceTrustValues failed: ${JSON.stringify(e)}`);
          return undefined;
        });
  }

  private async copySignals(copyButton: HTMLButtonElement): Promise<void> {
    copyButton.disabled = true;
    navigator.clipboard.writeText(this.signalsString)
        .finally(() => copyButton.disabled = false);
  }

  private showElement(element: Element) {
    element?.classList.remove('hidden');
  }

  private hideElement(element: HTMLElement) {
    element?.classList.add('hidden');
  }

  private trustLevelToString(trustLevel: KeyTrustLevel): string {
    return TrustLevelStringMap[trustLevel] || 'invalid';
  }

  private keyTypeToString(keyType: KeyType): string {
    return KeyTypeStringMap[keyType] || 'invalid';
  }

  private keySyncCodeToString(syncKeyResponseCode: Int32Value|
                              undefined): string {
    if (!syncKeyResponseCode) {
      return 'Undefined';
    }

    const value = syncKeyResponseCode.value;
    if (value / 100 === 2) {
      return `Success (${value})`;
    }
    return `Failure (${value})`;
  }
}

customElements.define(
    DeviceTrustConnectorElement.is, DeviceTrustConnectorElement);
