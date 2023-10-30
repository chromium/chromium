// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

const iwaInstallButton =
    getRequiredElement('iwa-install-button') as HTMLButtonElement;
const iwaInstallUrl = getRequiredElement('iwa-install-url') as HTMLInputElement;
const iwaSelectFileButton =
    getRequiredElement('iwa-select-bundle') as HTMLButtonElement;
const iwaSearchForUpdatesButton = getRequiredElement('iwa-search-for-updates');

/**
 * Converts a mojo origin into a user-readable string, omitting default ports.
 * @param origin Origin to convert
 *
 * TODO(b/304717391): Extract origin serialization logic from here and use it
 * everywhere `url.mojom.Origin` is serialized in JS/TS.
 */
function originToText(origin: Origin): string {
  if (origin.host.length === 0) {
    return 'null';
  }

  let result = origin.scheme + '://' + origin.host;
  if (!(origin.scheme === 'https' && origin.port === 443) &&
      !(origin.scheme === 'http' && origin.port === 80)) {
    result += ':' + origin.port;
  }
  return result;
}

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

  const location: Url = {url: iwaInstallUrl.value};

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

async function iwaSelectFile() {
  const iwaInstallMessageDiv = getRequiredElement('iwa-install-message-div');

  iwaInstallMessageDiv.innerText = `Installing IWA from bundle...`;

  const installFromDevBundle =
      await webAppInternalsHandler
          .selectFileAndInstallIsolatedWebAppFromDevBundle();
  if (installFromDevBundle.result.success) {
    iwaInstallMessageDiv.innerText = `Installing IWA: successfully installed.`;
    return;
  }

  iwaInstallMessageDiv.innerText =
      `Installing IWA: failed to install: ${installFromDevBundle.result.error}`;
}
iwaSelectFileButton.addEventListener('click', iwaSelectFile);

async function iwaSearchForUpdates() {
  const messageDiv = getRequiredElement('iwa-update-discovery-message-div');

  messageDiv.innerText = `Queueing update discovery tasks...`;
  const {result} =
      await webAppInternalsHandler.searchForIsolatedWebAppUpdates();
  messageDiv.innerText = result;
}
iwaSearchForUpdatesButton.addEventListener('click', iwaSearchForUpdates);

document.addEventListener('DOMContentLoaded', async () => {
  getRequiredElement('json').innerText = await debugInfoAsJsonString;

  if (loadTimeData.getBoolean('experimentalAreIwasEnabled')) {
    // Unhide the IWA div.
    getRequiredElement('iwa-div').style.display = '';

    if (loadTimeData.getBoolean('experimentalIsIwaDevModeEnabled')) {
      // Unhide the IWA install div.
      getRequiredElement('iwa-install-div').style.display = '';

      const devModeProxyAppList =
          getRequiredElement('iwa-dev-mode-proxy-app-list');
      const {apps: devModeProxyApps} =
          await webAppInternalsHandler.getIsolatedWebAppDevModeProxyAppInfo();
      for (const devModeProxyApp of devModeProxyApps) {
        const li = document.createElement('li');

        li.innerText =
            `${devModeProxyApp.name} (${devModeProxyApp.installedVersion}) -> ${
                originToText(devModeProxyApp.proxyOrigin)}`;

        const updateMsg = document.createElement('p');

        const updateBtn = document.createElement('button');
        updateBtn.className = 'iwa-update-btn';
        updateBtn.innerText = 'Perform update now';
        updateBtn.onclick = async () => {
          const oldText = updateBtn.innerText;
          try {
            updateBtn.disabled = true;
            updateBtn.innerText =
                'Performing update... (close the IWA if it is currently open!)';

            const {result} =
                await webAppInternalsHandler.updateDevProxyIsolatedWebApp(
                    devModeProxyApp.appId);
            updateMsg.innerText = result;
          } finally {
            updateBtn.innerText = oldText;
            updateBtn.disabled = false;
          }
        };

        li.appendChild(updateBtn);
        li.appendChild(updateMsg);

        devModeProxyAppList.appendChild(li);
      }

      // Unhide the div that hides the list of dev mode proxy apps.
      getRequiredElement('iwa-dev-mode-proxy-updates').style.display = '';
    }
  }
});
