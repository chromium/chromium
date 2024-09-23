// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CustomElement} from 'chrome://resources/js/custom_element.js';

import {BrowserProxy} from './browser_proxy.js';
import type {ClientCertificateState, ClientIdentity, PageHandlerInterface} from './connectors_internals.mojom-webui.js';
import * as utils from './connectors_utils.js';
import {getTemplate} from './managed_client_certificate.html.js';

export class ManagedClientCertificateElement extends CustomElement {
  static get is() {
    return 'managed-client-certificate';
  }

  static override get template() {
    return getTemplate();
  }

  private get pageHandler(): PageHandlerInterface {
    return BrowserProxy.getInstance().handler;
  }

  private set policyEnabledLevels(policyLevels: string[]) {
    if (policyLevels.length === 0) {
      this.setValueToElement('#policy-enabled-levels', 'None');
      return;
    }

    this.setValueToElement('#policy-enabled-levels', `${policyLevels}`);
  }

  private get managedIdentitiesSection(): HTMLElement|null {
    return this.$('#managed-identities');
  }

  constructor() {
    super();
    this.fetchClientCertificateValues();
  }

  private fetchClientCertificateValues() {
    this.pageHandler.getClientCertificateState().then(
        (response: {state: ClientCertificateState}) =>
            this.updateState(response?.state),
        err => console.error(
            `Failed to fetch client cert state: ${JSON.stringify(err)}`));
  }

  private updateState(state: ClientCertificateState|undefined) {
    const managedIdentitiesSection = this.managedIdentitiesSection;
    if (!managedIdentitiesSection) {
      // A critical element is missing from the page, so fail early.
      this.policyEnabledLevels = ['Error'];
      return;
    }

    // Clear managed identities.
    while (managedIdentitiesSection.firstChild) {
      managedIdentitiesSection.removeChild(managedIdentitiesSection.firstChild);
    }

    if (!state) {
      this.policyEnabledLevels = ['Error'];
      return;
    }

    this.policyEnabledLevels = state.policyEnabledLevels;

    if (state.managedProfileIdentity) {
      managedIdentitiesSection.appendChild(this.createManagedIdentityElement(
          'Profile', state.managedProfileIdentity));
    }

    if (state.managedBrowserIdentity) {
      managedIdentitiesSection.appendChild(this.createManagedIdentityElement(
          'Browser', state.managedBrowserIdentity));
    }
  }

  private setValueToElement(elementId: string, stringValue: string) {
    const htmlElement = this.$<HTMLElement>(elementId);
    if (htmlElement) {
      htmlElement.innerText = stringValue;
    } else {
      console.error(`Could not find ${elementId} element.`);
    }
  }

  private createManagedIdentityElement(
      prefix: string, managedIdentity: ClientIdentity): HTMLDivElement {
    const identityElement = document.createElement('div') as HTMLDivElement;

    let keyValuePairs =
        [{key: `${prefix} Identity Name`, value: managedIdentity.identityName}];

    if (managedIdentity.loadedKeyInfo) {
      keyValuePairs = keyValuePairs.concat([
        {
          key: 'Key Trust Level',
          value: utils.trustLevelToString(
              managedIdentity.loadedKeyInfo.trustLevel),
        },
        {
          key: 'Key Type',
          value: utils.keyTypeToString(managedIdentity.loadedKeyInfo.keyType),
        },
        {
          key: 'Public Key Hash',
          value: managedIdentity.loadedKeyInfo.encodedSpkiHash,
        },
      ]);

      if (managedIdentity.loadedKeyInfo.keyUploadStatus) {
        if (managedIdentity.loadedKeyInfo.keyUploadStatus.uploadClientError) {
          keyValuePairs.push({
            key: 'Key Upload Client Error',
            value:
                managedIdentity.loadedKeyInfo.keyUploadStatus.uploadClientError,
          });
        } else {
          keyValuePairs.push({
            key: 'Key Upload Response',
            value: utils.keySyncCodeToString(
                managedIdentity.loadedKeyInfo.keyUploadStatus
                    .syncKeyResponseCode),
          });
        }
      }
    }

    if (managedIdentity.certificateMetadata) {
      keyValuePairs = keyValuePairs.concat([
        {
          key: 'Subject',
          value: managedIdentity.certificateMetadata.subjectDisplayName,
        },
        {
          key: 'Issuer',
          value: managedIdentity.certificateMetadata.issuerDisplayName,
        },
        {
          key: 'Serial Number',
          value: managedIdentity.certificateMetadata.serialNumber,
        },
        {
          key: 'SHA-256 Fingerprint',
          value: managedIdentity.certificateMetadata.fingerprint,
        },
        {
          key: 'Creation Date',
          value: managedIdentity.certificateMetadata.creationDateString,
        },
        {
          key: 'Expiration Date',
          value: managedIdentity.certificateMetadata.expirationDateString,
        },
      ]);
    }

    for (const pair of keyValuePairs) {
      identityElement.appendChild(
          this.createLabelledValueElement(pair.key, pair.value));
    }

    return identityElement;
  }

  private createLabelledValueElement(label: string, text: string): HTMLElement {
    const nameSpan = document.createElement('span') as HTMLSpanElement;
    nameSpan.classList.add('bold');
    nameSpan.textContent = text;

    const containerElement = document.createElement('div') as HTMLDivElement;
    containerElement.appendChild(document.createTextNode(`${label}: `));
    containerElement.appendChild(nameSpan);
    return containerElement;
  }
}

customElements.define(
    ManagedClientCertificateElement.is, ManagedClientCertificateElement);
