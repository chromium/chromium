// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {IwaDevModeLocation} from './web_app_internals.mojom-webui.js';
import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

const iwaDevProxyInstallButton =
    getRequiredElement('iwa-dev-install-proxy-button') as HTMLButtonElement;
const iwaDevProxyInstallUrl =
    getRequiredElement('iwa-dev-install-proxy-url') as HTMLInputElement;

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

/**
 * Converts a mojo representation of `base::FilePath` into a user-readable
 * string.
 * @param filePath File path to convert
 */
function filePathToText(filePath: FilePath): string {
  if (typeof filePath.path === 'string') {
    return filePath.path;
  }

  const decoder = new TextDecoder('utf-16');
  const buffer = new Uint16Array(filePath.path);
  return decoder.decode(buffer);
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

function updateDevProxyInstallButtonState() {
  iwaDevProxyInstallButton.disabled =
      (iwaDevProxyInstallUrl.value.length === 0);
}

async function iwaDevProxyInstall() {
  iwaDevProxyInstallButton.disabled = true;

  const iwaInstallMessageDiv = getRequiredElement('iwa-dev-install-message');

  // Validate the provided URL.
  let valid = false;
  try {
    // We don't need the result of this, only to verify it doesn't throw an
    // exception.
    new URL(iwaDevProxyInstallUrl.value);
    valid =
        (iwaDevProxyInstallUrl.value.startsWith('http:') ||
         iwaDevProxyInstallUrl.value.startsWith('https:'));
  } catch (_) {
    // Fall-through.
  }
  if (!valid) {
    iwaInstallMessageDiv.innerText =
        `Installing IWA: ${iwaDevProxyInstallUrl.value} is not a valid URL`;
    updateDevProxyInstallButtonState();
    return;
  }

  iwaInstallMessageDiv.innerText =
      `Installing IWA: ${iwaDevProxyInstallUrl.value}...`;

  const location: Url = {url: iwaDevProxyInstallUrl.value};

  const installFromDevProxy =
      await webAppInternalsHandler.installIsolatedWebAppFromDevProxy(location);
  if (installFromDevProxy.result.success) {
    iwaInstallMessageDiv.innerText = `Installing IWA: ${
        iwaDevProxyInstallUrl.value} successfully installed.`;
    iwaDevProxyInstallUrl.value = '';
    updateDevProxyInstallButtonState();
    refreshDevModeAppList();
    return;
  }

  iwaInstallMessageDiv.innerText =
      `Installing IWA: ${iwaDevProxyInstallUrl.value} failed to install: ${
          installFromDevProxy.result.error}`;
  updateDevProxyInstallButtonState();
}
iwaDevProxyInstallUrl.addEventListener('enter', iwaDevProxyInstall);
iwaDevProxyInstallButton.addEventListener('click', iwaDevProxyInstall);

iwaDevProxyInstallUrl.addEventListener('keyup', (event: KeyboardEvent) => {
  if (event.key === 'Enter') {
    event.preventDefault();
    iwaDevProxyInstall();
    return;
  }
  updateDevProxyInstallButtonState();
});
updateDevProxyInstallButtonState();

getRequiredElement('iwa-dev-install-bundle-selector')
    .addEventListener('click', async () => {
      const iwaInstallMessageDiv =
          getRequiredElement('iwa-dev-install-message');

      iwaInstallMessageDiv.innerText = `Installing IWA from bundle...`;

      const installFromDevBundle =
          await webAppInternalsHandler
              .selectFileAndInstallIsolatedWebAppFromDevBundle();
      if (installFromDevBundle.result.success) {
        iwaInstallMessageDiv.innerText =
            `Installing IWA: successfully installed.`;
        refreshDevModeAppList();
        return;
      }

      iwaInstallMessageDiv.innerText = `Installing IWA: failed to install: ${
          installFromDevBundle.result.error}`;
    });

getRequiredElement('iwa-updates-search-button')
    .addEventListener('click', async () => {
      const messageDiv = getRequiredElement('iwa-updates-message');

      messageDiv.innerText = `Queueing update discovery tasks...`;
      const {result} =
          await webAppInternalsHandler.searchForIsolatedWebAppUpdates();
      messageDiv.innerText = result;
    });

function formatDevModeLocation(location: IwaDevModeLocation): string {
  if (location.proxyOrigin) {
    return originToText(location.proxyOrigin);
  }
  if (location.bundlePath) {
    return filePathToText(location.bundlePath);
  }
  assertNotReached();
}

function showIwaSection(containerId: string) {
  getRequiredElement(containerId).style.display = '';
  getRequiredElement('iwa-container').style.display = '';
}

async function refreshDevModeAppList() {
  const devModeUpdatesMessage = getRequiredElement('iwa-dev-updates-message');
  devModeUpdatesMessage.innerText = 'Loading...';
  const {apps: devModeApps} =
      await webAppInternalsHandler.getIsolatedWebAppDevModeAppInfo();
  const devModeAppList = getRequiredElement('iwa-dev-updates-app-list');
  devModeAppList.replaceChildren();
  if (devModeApps.length === 0) {
    devModeUpdatesMessage.innerText = 'None';
  } else {
    devModeUpdatesMessage.innerText = '';
    for (const devModeApp of devModeApps) {
      const li = document.createElement('li');

      li.innerText = `${devModeApp.name} (${devModeApp.installedVersion}) → ${
          formatDevModeLocation(devModeApp.location)}`;

      const updateMsg = document.createElement('p');

      const updateBtn = document.createElement('button');
      updateBtn.className = 'iwa-dev-update-button';
      updateBtn.innerText = 'Perform update now';
      updateBtn.onclick = async () => {
        const oldText = updateBtn.innerText;
        try {
          updateBtn.disabled = true;
          updateBtn.innerText =
              'Performing update... (close the IWA if it is currently open!)';

          if (devModeApp.location.bundlePath) {
            const {result} =
                await webAppInternalsHandler
                    .selectFileAndUpdateIsolatedWebAppFromDevBundle(
                        devModeApp.appId);
            updateMsg.innerText = result;
          } else if (devModeApp.location.proxyOrigin) {
            const {result} =
                await webAppInternalsHandler.updateDevProxyIsolatedWebApp(
                    devModeApp.appId);
            updateMsg.innerText = result;
          } else {
            assertNotReached();
          }
        } finally {
          updateBtn.innerText = oldText;
          updateBtn.disabled = false;
        }
      };

      li.appendChild(updateBtn);
      li.appendChild(updateMsg);

      devModeAppList.appendChild(li);
    }
  }
}

document.addEventListener('DOMContentLoaded', async () => {
  getRequiredElement('json').innerText = await debugInfoAsJsonString;

  if (loadTimeData.getBoolean('isIwaPolicyInstallEnabled')) {
    showIwaSection('iwa-updates-container');
  }

  if (loadTimeData.getBoolean('isIwaDevModeEnabled')) {
    // Unhide the IWA dev mode UI.
    showIwaSection('iwa-dev-container');
    refreshDevModeAppList();
  }
});
