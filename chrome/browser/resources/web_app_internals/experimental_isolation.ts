// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();
const clearButton = getRequiredElement('clear-button') as HTMLButtonElement;
const messageDiv = getRequiredElement('message');

clearButton.addEventListener('click', async () => {
  clearButton.disabled = true;

  messageDiv.innerText = 'Clearing';
  if ((await webAppInternalsHandler.clearExperimentalWebAppIsolationData())
          .success) {
    messageDiv.innerText = 'Data cleared successfully';
    return;
  }

  messageDiv.innerText =
      'Failed to clear all data. You probably have launched some web apps, ' +
      'which prevents some of the data from being deleted. ';
  const restartChrome = document.createElement('a');
  restartChrome.href = 'chrome://restart';
  restartChrome.innerText = 'Restart chrome';
  messageDiv.appendChild(restartChrome);
  messageDiv.appendChild(document.createTextNode(' and try again.'));
});

document.addEventListener('DOMContentLoaded', () => {
  if (loadTimeData.getBoolean('experimentalIsolationEnabled')) {
    // Unhide the button.
    clearButton.style.display = '';
    return;
  }

  messageDiv.innerText = 'Experimental isolation not enabled';
});
