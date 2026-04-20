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
  const proxyUrl =
      document.getElementById('privateAiProxyUrl') as HTMLInputElement;


  privateAiServerUrl.disabled = connected;
  privateAiServerApiKey.disabled = connected;
  useTokenAttestationCheckbox.disabled = connected;
  proxyUrl.disabled = connected;

  connectionConsole.classList.toggle('hidden', !connected);
  createConnectionButton.classList.toggle('hidden', connected);
  disconnectButton.classList.toggle('hidden', !connected);

  if (connected) {
    const consoleContainer =
        document.getElementById('console-container') as HTMLElement;
    consoleContainer.textContent = '';

    const logsContainer =
        document.getElementById('logs-container') as HTMLElement;
    logsContainer.textContent = '';
  }
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
  const sendZssButton = document.getElementById('send-zss-button');
  sendZssButton?.addEventListener('click', () => {
    onZssRequestSend();
  });
  const sendFormsAiButton = document.getElementById('send-forms-ai-button');
  sendFormsAiButton?.addEventListener('click', () => {
    onFormsAiRequestSend();
  });
}

function addMsgToConsoleContainer(msg: string) {
  const consoleContainer = document.getElementById('console-container');

  const msgElement = document.createElement('div');
  msgElement.textContent = msg;
  consoleContainer?.appendChild(msgElement);
}

function addErrorToConsoleContainer(error: string) {
  const consoleContainer = document.getElementById('console-container');

  const errorElement = document.createElement('div');
  errorElement.textContent = error;
  errorElement.style.color = 'red';
  consoleContainer?.appendChild(errorElement);
}

function onRequestSend() {
  const requestInput =
      document.getElementById('request-input') as HTMLInputElement;

  const request = requestInput.value;
  if (request.trim() === '') {
    return;
  }

  // Display user's request.
  addMsgToConsoleContainer('Request: ' + request);

  // Send request to the Private AI client and get a response.
  proxy.sendRequest(loadTimeData.getString('default_feature_name'), request)
      .then((response) => {
        if (response.response) {
          addMsgToConsoleContainer('Response: ' + response.response);
        } else {
          addErrorToConsoleContainer('Error: ' + response.error);
        }
      });

  // Clear the input field and refocus.
  requestInput.value = '';
  requestInput.focus();
}

function onZssRequestSend() {
  const requestInput =
      document.getElementById('request-input') as HTMLInputElement;

  const text = requestInput.value;
  if (text.trim() === '') {
    return;
  }

  // Display user's request.
  addMsgToConsoleContainer('ZSS Request: ' + text);

  // Send ZSS request to the Private AI client and get a response.
  proxy.sendZssRequest(text).then((response) => {
    if (response.response) {
      addMsgToConsoleContainer('ZSS Response: ' + response.response);
    } else {
      addErrorToConsoleContainer('Error: ' + response.error);
    }
  });

  // Clear the input field and refocus.
  requestInput.value = '';
  requestInput.focus();
}

function onFormsAiRequestSend() {
  const requestInput =
      document.getElementById('request-input') as HTMLInputElement;

  const url = requestInput.value;
  if (url.trim() === '') {
    return;
  }

  // Display user's request.
  addMsgToConsoleContainer('FormsAI Request (URL): ' + url);

  // Send FormsAI request to the Private AI client and get a response.
  proxy.sendFormsAiRequest(url).then((response) => {
    if (response.response) {
      addMsgToConsoleContainer('FormsAI Response: ' + response.response);
    } else {
      addErrorToConsoleContainer('Error: ' + response.error);
    }
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
