// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LogLevel} from './private_ai_internals.mojom-webui.js';
import {PrivateAiInternalsBrowserProxyImpl} from './private_ai_internals_browser_proxy.js';

const proxy = PrivateAiInternalsBrowserProxyImpl.getInstance();

function registerOnLogMessageListener() {
  const logsContainer = document.getElementById('logs-container');
  proxy.getCallbackRouter().onLogMessage.addListener(
      (level: LogLevel, message: string) => {
        const logElement = document.createElement('div');
        logElement.textContent = message;
        if (level === LogLevel.kError) {
          logElement.style.color = 'red';
        } else {
          logElement.style.color = 'blue';
        }
        logsContainer?.appendChild(logElement);
      });
}

function setConnectedState(connected: boolean) {
  const connectionConsole = document.getElementById('console')!;
  const createConnectionButton =
      document.getElementById('create-connection-button')!;
  const disconnectButton = document.getElementById('disconnect-button')!;
  const privateAiServerUrl =
      document.getElementById('privateAiServerUrl') as HTMLInputElement;
  const privateAiServerApiKey =
      document.getElementById('privateAiServerApiKey') as HTMLInputElement;
  const useTokenAttestationCheckbox =
      document.getElementById('use-token-attestation-checkbox') as
      HTMLInputElement;

  privateAiServerUrl.disabled = connected;
  privateAiServerApiKey.disabled = connected;
  useTokenAttestationCheckbox.disabled = connected;

  connectionConsole.classList.toggle('hidden', !connected);
  createConnectionButton.classList.toggle('hidden', connected);
  disconnectButton.classList.toggle('hidden', !connected);
}

function registerOnCreateConnectionButtonListener() {
  const createConnectionButton =
      document.getElementById('create-connection-button');
  const disconnectButton = document.getElementById('disconnect-button');

  if (createConnectionButton === null) {
    console.error('createConnectionButton is null');
    return;
  }
  if (disconnectButton === null) {
    console.error('disconnectButton is null');
    return;
  }

  createConnectionButton.addEventListener('click', () => {
    if (getAPIKey() === '') {
      console.info('API key missing');
      return;
    }

    setConnectedState(true);

    const useTokenAttestationCheckbox =
        document.getElementById('use-token-attestation-checkbox') as
        HTMLInputElement;
    const useTokenAttestation = useTokenAttestationCheckbox.checked;
    let proxyUrl = '';
    if (useTokenAttestation) {
      proxyUrl = getProxyUrl();
    }

    proxy.connect(getServerURL(), getAPIKey(), proxyUrl, useTokenAttestation)
        .then(() => {
          console.info('connected');
        });
  });

  disconnectButton.addEventListener('click', () => {
    proxy.close().then(() => {
      console.info('disconnected');
      setConnectedState(false);
    });
  });
}

function registerOnSendButtonListener() {
  const sendButton = document.getElementById('send-button');
  sendButton?.addEventListener('click', () => {
    onRequestSend();
  });
}

function onRequestSend() {
  const consoleContainer = document.getElementById('console-container');
  const requestInput =
      document.getElementById('request-input') as HTMLInputElement;

  const request = requestInput.value;
  if (request.trim() === '') {
    return;
  }

  // Display user's request.
  const userRequestElement = document.createElement('div');
  userRequestElement.textContent = 'Request: ' + request;
  consoleContainer?.appendChild(userRequestElement);

  // Send request to the Private AI client and get a response.
  proxy.sendRequest(loadTimeData.getString('default_feature_name'), request)
      .then((response) => {
        const serverResponseElement = document.createElement('div');
        serverResponseElement.textContent = 'Response: ' + response.response;
        consoleContainer?.appendChild(serverResponseElement);
      });

  // Clear the input field and refocus.
  requestInput.value = '';
  requestInput.focus();
}

function getServerURL() {
  const privateAiServerUrl =
      document.getElementById('privateAiServerUrl') as HTMLInputElement;
  return privateAiServerUrl.value;
}

function getAPIKey() {
  const privateAiServerApiKey =
      document.getElementById('privateAiServerApiKey') as HTMLInputElement;
  return privateAiServerApiKey.value;
}

function getProxyUrl() {
  const privateAiProxyUrl =
      document.getElementById('privateAiProxyUrl') as HTMLInputElement;
  return privateAiProxyUrl.value;
}

function registerOnTokenCheckboxListener() {
  const useTokenAttestationCheckbox =
      document.getElementById('use-token-attestation-checkbox');
  const proxyUrlContainer = document.getElementById('proxy-url-container');

  if (useTokenAttestationCheckbox === null || proxyUrlContainer === null) {
    console.error('useTokenAttestationCheckbox or proxyUrlContainer is null');
    return;
  }

  useTokenAttestationCheckbox.addEventListener('change', () => {
    if ((useTokenAttestationCheckbox as HTMLInputElement).checked) {
      proxyUrlContainer.classList.remove('hidden');
    } else {
      proxyUrlContainer.classList.add('hidden');
    }
  });
}

window.onload = function() {
  console.info('window.onload');

  registerOnLogMessageListener();

  registerOnCreateConnectionButtonListener();

  registerOnSendButtonListener();

  registerOnTokenCheckboxListener();

  const useTokenAttestationCheckbox =
      document.getElementById('use-token-attestation-checkbox') as
      HTMLInputElement;
  useTokenAttestationCheckbox.checked =
      loadTimeData.getBoolean('default_use_token_attestation');
  useTokenAttestationCheckbox.dispatchEvent(new Event('change'));
};
