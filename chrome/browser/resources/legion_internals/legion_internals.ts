// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let legionClient: LegionClient;

export interface LegionServerInfoProvider {
  getServerUrl(): Promise<{url: string}>;
  getApiKey(): Promise<{apiKey: string}>;
}

export class LegionServerInfoProviderStub implements LegionServerInfoProvider {
  getServerUrl(): Promise<{url: string}> {
    return Promise.resolve({url: 'http://localhost:8080'});
  }

  getApiKey(): Promise<{apiKey: string}> {
    return Promise.resolve({apiKey: 'test-api-key'});
  }
}

export interface LegionClient {
  sendRequest(request: string): Promise<{response: string}>;
}

export class LegionClientStub implements LegionClient {
  sendRequest(request: string): Promise<{response: string}> {
    console.info(`LegionClientStub received request: ${request}`);
    return new Promise((resolve) => {
      setTimeout(() => {
        resolve({response: `Stub response for: ${request}`});
      }, 1000);
    });
  }
}

export interface LegionClientFactory {
  createLegionClient(url: string, apiKey: string): LegionClient;
}

export class LegionClientFactoryStub implements LegionClientFactory {
  createLegionClient(url: string, apiKey: string): LegionClient {
    console.info(`LegionClientFactoryStub creating client for URL: ${
        url}, API Key: ${apiKey}`);
    return new LegionClientStub();
  }
}

function prefillURLAndAPIKey() {
  const serverInfoProvider: LegionServerInfoProvider =
      new LegionServerInfoProviderStub();

  // Prefill legion server URL and API key if available on a Chrome side.
  serverInfoProvider.getServerUrl().then((response) => {
    const legionServerUrl =
        document.getElementById('legionServerUrl') as HTMLInputElement;
    legionServerUrl.value = response.url;
  });
  serverInfoProvider.getApiKey().then((response) => {
    const legionServerApiKey =
        document.getElementById('legionServerApiKey') as HTMLInputElement;
    legionServerApiKey.value = response.apiKey;
  });
}

function registerOnCreateConnectionButtonListener() {
  const connectionConsole = document.getElementById('console');
  const createConnectionButton =
      document.getElementById('create-connection-button');

  if (connectionConsole === null) {
    console.error('connectionConsole is null');
    return;
  }
  if (createConnectionButton === null) {
    console.error('createConnectionButton is null');
    return;
  }

  createConnectionButton.addEventListener('click', () => {
    connectionConsole.classList.remove('hidden');
    createConnectionButton.classList.add('hidden');

    const legionClientFactory: LegionClientFactory =
        new LegionClientFactoryStub();

    // Create legion client to send requests and get responses.
    legionClient =
        legionClientFactory.createLegionClient(getServerURL(), getAPIKey());
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
  // const legionClient: LegionClient = globalThis.legionClient;
  legionClient.sendRequest(request).then((response) => {
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

window.onload = function() {
  console.info('window.onload');

  prefillURLAndAPIKey();

  registerOnCreateConnectionButtonListener();

  registerOnSendButtonListener();
};
