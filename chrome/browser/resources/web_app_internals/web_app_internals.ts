// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util_ts.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

const iwaInstallButton =
    getRequiredElement('iwa-install-button') as HTMLButtonElement;
const iwaInstallUrl = getRequiredElement('iwa-install-url') as HTMLInputElement;

getRequiredElement('copy-button').addEventListener('click', async () => {
  navigator.clipboard.writeText(await debugInfoAsJsonString);
});

getRequiredElement('download-button').addEventListener('click', async () => {
  const url = URL.createObjectURL(new Blob([await debugInfoAsJsonString], {
    type: 'application/json',
  }));

  const a = document.createElement('a');
  a.href = url;
  a.download = 'web_app_internals.json';
  a.click();

  // Downloading succeeds even if the URL was revoked during downloading. See
  // the spec for details (https://w3c.github.io/FileAPI/#dfn-revokeObjectURL):
  //
  // "Note: ... Requests that were started before the url was revoked should
  // still succeed."
  URL.revokeObjectURL(url);
});

function iwaInstallStateUpdate() {
  iwaInstallButton.disabled = (iwaInstallUrl.value.length === 0);
}

async function iwaInstallSubmit() {
  iwaInstallButton.disabled = true;

  const iwaInstallMessageDiv = getRequiredElement('iwa-install-message-div');

  // Validate the provided URL.
  let valid = false;
  try {
    // We don't need the result of this, only to verify it doesn't throw an
    // exception.
    new URL(iwaInstallUrl.value);
    valid =
        (iwaInstallUrl.value.startsWith('http:') ||
         iwaInstallUrl.value.startsWith('https:'));
  } catch (_) {
    // Fall-through.
  }
  if (!valid) {
    iwaInstallMessageDiv.innerText =
        `Installing IWA: ${iwaInstallUrl.value} is not a valid URL`;
    iwaInstallStateUpdate();
    return;
  }

  iwaInstallMessageDiv.innerText = `Installing IWA: ${iwaInstallUrl.value}...`;

  const location = new Url();
  location.url = iwaInstallUrl.value;

  const installFromDevProxy =
      await webAppInternalsHandler.installIsolatedWebAppFromDevProxy(location);
  if (installFromDevProxy.result.success) {
    iwaInstallMessageDiv.innerText =
        `Installing IWA: ${iwaInstallUrl.value} successfully installed.`;
    iwaInstallUrl.value = '';
    iwaInstallStateUpdate();
    return;
  }

  iwaInstallMessageDiv.innerText =
      `Installing IWA: ${iwaInstallUrl.value} failed to install: ${
          installFromDevProxy.result.error}`;
  iwaInstallStateUpdate();
}

iwaInstallUrl.addEventListener('enter', iwaInstallSubmit);
iwaInstallButton.addEventListener('click', iwaInstallSubmit);

function updateIwaInstallButtonState(event: KeyboardEvent) {
  if (event.key === 'Enter') {
    event.preventDefault();
    iwaInstallSubmit();
    return;
  }
  iwaInstallStateUpdate();
}
iwaInstallUrl.addEventListener('keyup', updateIwaInstallButtonState);
iwaInstallStateUpdate();

document.addEventListener('DOMContentLoaded', async () => {
  getRequiredElement('json').innerText = await debugInfoAsJsonString;

  if (loadTimeData.getBoolean('experimentalIsIwaDevModeEnabled')) {
    // Unhide the IWA install div.
    getRequiredElement('iwa-install-div').style.display = '';
    return;
  }
});
