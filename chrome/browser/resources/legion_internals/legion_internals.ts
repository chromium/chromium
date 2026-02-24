// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LogLevel} from './legion_internals.mojom-webui.js';
import {LegionInternalsBrowserProxyImpl} from './legion_internals_browser_proxy.js';

const proxy = LegionInternalsBrowserProxyImpl.getInstance();

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
  const legionServerUrl =
      document.getElementById('legionServerUrl') as HTMLInputElement;
  const legionServerApiKey =
      document.getElementById('legionServerApiKey') as HTMLInputElement;
  const useTokenAttestationCheckbox =
      document.getElementById('use-token-attestation-checkbox') as
      HTMLInputElement;

  legionServerUrl.disabled = connected;
  legionServerApiKey.disabled = connected;
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

  // Send request to the legion client and get a response.
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
  const legionServerUrl =
      document.getElementById('legionServerUrl') as HTMLInputElement;
  return legionServerUrl.value;
}

function getAPIKey() {
  const legionServerApiKey =
      document.getElementById('legionServerApiKey') as HTMLInputElement;
  return legionServerApiKey.value;
}

function getProxyUrl() {
  const legionProxyUrl =
      document.getElementById('legionProxyUrl') as HTMLInputElement;
  return legionProxyUrl.value;
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
