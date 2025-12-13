// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {LegionInternalsBrowserProxyImpl} from './legion_internals_browser_proxy.js';

const proxy = LegionInternalsBrowserProxyImpl.getInstance();

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

    proxy.connect(getServerURL(), getAPIKey()).then(() => {
      console.info('connected');
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

window.onload = function() {
  console.info('window.onload');

  registerOnCreateConnectionButtonListener();

  registerOnSendButtonListener();
};
