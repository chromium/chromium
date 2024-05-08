// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {BrowserProxy} from './browser_proxy.js';
import type {ConsentMetadata, DeviceTrustState, KeyInfo, PageHandlerInterface} from './connectors_internals.mojom-webui.js';
import {KeyManagerInitializedValue, KeyManagerPermanentFailure} from './connectors_internals.mojom-webui.js';
import * as utils from './connectors_utils.js';
import {getTemplate} from './device_trust_connector.html.js';


const KeyPermanentFailureMap = {
  [KeyManagerPermanentFailure.CREATION_UPLOAD_CONFLICT]:
      'A key already exists on the server for this device.',
  [KeyManagerPermanentFailure.INSUFFICIENT_PERMISSIONS]:
      'The browser is missing permissions and is unable to create a Device ' +
      'Trust key.',
  [KeyManagerPermanentFailure.OS_RESTRICTION]:
      'This device is missing a critical feature (e.g. no SecureEnclave ' +
      'support on Mac).',
  [KeyManagerPermanentFailure.INVALID_INSTALLATION]:
      'The browser is missing a critical installation dependency (e.g. is ' +
      'a user-level installation on Windows).',
};

export class DeviceTrustConnectorElement extends CustomElement {
  static get is() {
    return 'device-trust-connector';
  }

  static override get template() {
    return getTemplate();
  }

  get deleteKeyEnabled(): boolean {
    return loadTimeData.getBoolean('canDeleteDeviceTrustKey');
  }

  set enabledString(isEnabledString: string) {
    this.setValueToElement('#enabled-string', isEnabledString);
  }

  set policyEnabledLevels(policyLevels: string[]) {
    if (policyLevels.length === 0) {
      this.setValueToElement('#policy-enabled-levels', 'None');
      return;
    }

    this.setValueToElement('#policy-enabled-levels', `${policyLevels}`);
  }

  set consentMetadata(consentMetadata: ConsentMetadata|null) {
    const consentDetailsEl = this.getRequiredElement('#consent-details');
    const noConsentDetailsEl = this.getRequiredElement('#no-consent-details');
    if (!consentMetadata) {
      this.showElement(noConsentDetailsEl);
      this.hideElement(consentDetailsEl);
      return;
    }

    this.showElement(consentDetailsEl);
    this.hideElement(noConsentDetailsEl);

    this.setValueToElement(
        '#consent-received', `${consentMetadata.consentReceived}`);
    this.setValueToElement(
        '#can-collect', `${consentMetadata.canCollectSignals}`);
  }

  set keyInfo(keyInfo: KeyInfo) {
    const keySectionEl = this.getRequiredElement('#key-manager-section');
    const initStateEl = this.getRequiredElement('#key-manager-state');

    const keyPermanentErrorRowEl =
        this.getRequiredElement('#key-permanent-failure-row');
    const keyPermanentErrorValueEl =
        this.getRequiredElement('#key-permanent-failure');

    const keyLoadedRows = this.getRequiredElement('#key-loaded-rows');
    const trustLevelStateEl = this.getRequiredElement('#key-trust-level');
    const keyTypeStateEl = this.getRequiredElement('#key-type');
    const spkiHashStateEl = this.getRequiredElement('#spki-hash');
    const keySyncStateEl = this.getRequiredElement('#key-sync');

    const initializedValue = keyInfo.isKeyManagerInitialized;
    if (initializedValue === KeyManagerInitializedValue.UNSUPPORTED) {
      this.hideElement(keySectionEl);
    } else {
      const keyLoaded =
          initializedValue === KeyManagerInitializedValue.KEY_LOADED;
      initStateEl.innerText = keyLoaded ? 'true' : 'false';
      this.showElement(keySectionEl);

      if (keyInfo.permanentFailure === KeyManagerPermanentFailure.UNSPECIFIED) {
        this.hideElement(keyPermanentErrorRowEl);
      } else {
        const permanentFailureMessage =
            KeyPermanentFailureMap[keyInfo.permanentFailure] ||
            `Unknown: ${keyInfo.permanentFailure}`;
        keyPermanentErrorValueEl.innerText = permanentFailureMessage;

        this.showElement(keyPermanentErrorRowEl);
      }

      const keyMetadata = keyInfo.loadedKeyInfo;
      if (keyMetadata) {
        trustLevelStateEl.innerText =
            utils.trustLevelToString(keyMetadata.trustLevel);
        keyTypeStateEl.innerText = utils.keyTypeToString(keyMetadata.keyType);
        spkiHashStateEl.innerText = keyMetadata.encodedSpkiHash;
        keySyncStateEl.innerText = utils.keySyncCodeToString(
            keyMetadata.keyUploadStatus?.syncKeyResponseCode);

        this.showElement(keyLoadedRows);
      } else {
        this.hideElement(keyLoadedRows);
      }

      const deleteKeyButton = this.deleteKeyButton;
      if (deleteKeyButton) {
        this.deleteKeyEnabled ? this.showElement(deleteKeyButton) :
                                this.hideElement(deleteKeyButton);
      }
    }
  }

  private signalsString_: string = '';
  set signalsString(str: string) {
    const signalsEl = this.$<HTMLElement>('#signals');
    if (signalsEl) {
      signalsEl.innerText = str;
      this.signalsString_ = str;
    } else {
      console.error('Could not find #signals element.');
    }

    const signalsSection = this.$<HTMLElement>('#signals-section');
    if (signalsSection) {
      str === '' ? this.hideElement(signalsSection) :
                   this.showElement(signalsSection);
    } else {
      console.error('Could not find #signals-section element.');
    }
  }

  get copyButton(): HTMLButtonElement|undefined {
    return this.$('#copy-signals') as HTMLButtonElement;
  }

  get deleteKeyButton(): HTMLButtonElement|undefined {
    return this.$('#delete-key') as HTMLButtonElement;
  }

  get signalsString(): string {
    return this.signalsString_;
  }

  private get pageHandler(): PageHandlerInterface {
    return BrowserProxy.getInstance().handler;
  }

  constructor() {
    super();

    this.fetchDeviceTrustValues();

    if (this.deleteKeyEnabled) {
      const deleteKeyButton = this.deleteKeyButton;
      if (deleteKeyButton) {
        deleteKeyButton.addEventListener('click', () => this.deleteKey());
      }
    }
  }

  private setDeviceTrustValues(state: DeviceTrustState|undefined) {
    if (!state) {
      this.enabledString = 'error';
      return;
    }

    this.enabledString = `${state.isEnabled}`;
    this.policyEnabledLevels = state.policyEnabledLevels;
    this.consentMetadata = state.consentMetadata;
    this.keyInfo = state.keyInfo;
    this.signalsString = state.signalsJson;
  }

  private async fetchDeviceTrustValues(): Promise<void> {
    this.pageHandler.getDeviceTrustState()
        .then(
            (response: {state: DeviceTrustState}) => response && response.state,
            (e: object) => {
              console.warn(
                  `fetchDeviceTrustValues failed: ${JSON.stringify(e)}`);
              return undefined;
            })
        .then(state => this.setDeviceTrustValues(state))
        .then(() => {
          const copyButton = this.copyButton;
          if (copyButton) {
            copyButton.addEventListener(
                'click', () => this.copySignals(copyButton));
          }
        });
  }

  private async copySignals(copyButton: HTMLButtonElement): Promise<void> {
    copyButton.disabled = true;
    navigator.clipboard.writeText(this.signalsString)
        .finally(() => copyButton.disabled = false);
  }

  private async deleteKey(): Promise<void> {
    await this.pageHandler.deleteDeviceTrustKey();
  }

  private showElement(element: Element) {
    element?.classList.remove('hidden');
  }

  private hideElement(element: HTMLElement) {
    element?.classList.add('hidden');
  }

  private setValueToElement(elementId: string, stringValue: string) {
    const htmlElement = (this.$(elementId) as HTMLElement);
    if (htmlElement) {
      htmlElement.innerText = stringValue;
    } else {
      console.error(`Could not find ${elementId} element.`);
    }
  }
}

customElements.define(
    DeviceTrustConnectorElement.is, DeviceTrustConnectorElement);
