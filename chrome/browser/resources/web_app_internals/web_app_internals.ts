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

import type {InstallIsolatedWebAppResult, IwaDevModeAppInfo, IwaDevModeLocation, ParseUpdateManifestFromUrlResult, UpdateManifest, VersionEntry} from './web_app_internals.mojom-webui.js';
import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

const iwaDevProxyInstallButton =
    getRequiredElement('iwa-dev-install-proxy-button') as HTMLButtonElement;
const iwaDevProxyInstallUrl =
    getRequiredElement('iwa-dev-install-proxy-url') as HTMLInputElement;

const iwaDevUpdateManifestUrl =
    getRequiredElement('iwa-dev-update-manifest-url') as HTMLInputElement;

const iwaDevUpdateManifestDialog =
    getRequiredElement('iwa-update-manifest-dialog') as HTMLDialogElement;

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
  iwaDevProxyInstallButton.disabled = iwaDevProxyInstallUrl.value.length === 0;
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

  const result: InstallIsolatedWebAppResult =
      (await webAppInternalsHandler.installIsolatedWebAppFromDevProxy(location))
          .result;
  if (result.success) {
    iwaInstallMessageDiv.innerText = `Installing IWA: ${
        iwaDevProxyInstallUrl.value} successfully installed.`;
    iwaDevProxyInstallUrl.value = '';
    updateDevProxyInstallButtonState();
    refreshDevModeAppList();
    return;
  }

  iwaInstallMessageDiv.innerText = `Installing IWA: ${
      iwaDevProxyInstallUrl.value} failed to install: ${result.error}`;
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

      const result: InstallIsolatedWebAppResult =
          (await webAppInternalsHandler
               .selectFileAndInstallIsolatedWebAppFromDevBundle())
              .result;
      if (result.success) {
        iwaInstallMessageDiv.innerText =
            `Installing IWA: successfully installed (Web Bundle ID: ${
                result.success.webBundleId}).`;
        refreshDevModeAppList();
        return;
      }

      iwaInstallMessageDiv.innerText =
          `Installing IWA: failed to install: ${result.error}`;
    });

async function iwaDevFetchUpdateManifest() {
  const iwaInstallMessageDiv = getRequiredElement('iwa-dev-install-message');

  // Validate the provided URL.
  try {
    // We don't need the result of this, only to verify it doesn't throw an
    // exception.
    new URL(iwaDevUpdateManifestUrl.value);
  } catch (_) {
    iwaInstallMessageDiv.innerText = `Fetching the update manifest: ${
        iwaDevUpdateManifestUrl.value} is not a valid URL`;
    return;
  }

  iwaInstallMessageDiv.innerText =
      `Fetching the update manifest at ${iwaDevUpdateManifestUrl.value}...`;

  const updateManifestUrl: Url = {url: iwaDevUpdateManifestUrl.value};

  const result: ParseUpdateManifestFromUrlResult =
      (await webAppInternalsHandler.parseUpdateManifestFromUrl(
           updateManifestUrl))
          .result;
  if (result.error) {
    iwaInstallMessageDiv.innerText = `Installing IWA from update manifest: ${
        iwaDevUpdateManifestUrl.value} failed to install: ${result.error}`;
    return;
  }

  // `result` is a mojo union where there's always one of `error` or
  // `updateManifest` defined.
  const manifest: UpdateManifest = result.updateManifest!;
  const versions: VersionEntry[] = manifest.versions;

  const select = getRequiredElement('iwa-update-manifest-version-select') as
      HTMLSelectElement;
  select.replaceChildren();

  for (const versionEntry of versions) {
    const option = document.createElement('option');
    option.value = versionEntry.version;
    option.textContent = versionEntry.version;
    select.appendChild(option);
  }

  const installButton =
      getRequiredElement('iwa-update-manifest-dialog-install') as
      HTMLButtonElement;

  const installEventListener = async () => {
    installButton.removeEventListener('click', installEventListener);

    const selectedVersion = select.value;
    iwaDevUpdateManifestDialog.close();

    iwaInstallMessageDiv.innerText = `Installing version ${
        selectedVersion} from ${updateManifestUrl.url}...`;
    const selectedVersionEntry: VersionEntry|null =
        versions.find(
            versionEntry => versionEntry.version === selectedVersion) ||
        null;

    if (!selectedVersionEntry) {
      iwaInstallMessageDiv.innerText =
          `Installing version ${selectedVersion} from ${
              updateManifestUrl.url} failed: no such version`;
      return;
    }

    const installResult: InstallIsolatedWebAppResult =
        (await webAppInternalsHandler.installIsolatedWebAppFromBundleUrl({
          webBundleUrl: selectedVersionEntry.webBundleUrl,
          updateManifestUrl,
        })).result;
    if (installResult.success) {
      iwaInstallMessageDiv.innerText = `Installing version ${
          selectedVersion} from ${updateManifestUrl.url}: success!`;
    } else {
      iwaInstallMessageDiv.innerText =
          `Installing version ${selectedVersion} from ${
              updateManifestUrl.url} failed: ${installResult.error}`;
    }

    refreshDevModeAppList();
  };

  installButton.addEventListener('click', installEventListener);

  iwaDevUpdateManifestDialog.showModal();
}

getRequiredElement('iwa-update-manifest-dialog-close')
    .addEventListener('click', () => {
      getRequiredElement('iwa-dev-install-message').innerText = '';
      iwaDevUpdateManifestDialog.close();
    });

iwaDevUpdateManifestUrl.addEventListener('enter', iwaDevFetchUpdateManifest);
getRequiredElement('iwa-dev-update-manifest-fetch-button')
    .addEventListener('click', iwaDevFetchUpdateManifest);

getRequiredElement('iwa-updates-search-button')
    .addEventListener('click', async () => {
      const messageDiv = getRequiredElement('iwa-updates-message');

      messageDiv.innerText = `Queueing update discovery tasks...`;
      const result: string =
          (await webAppInternalsHandler.searchForIsolatedWebAppUpdates())
              .result;
      messageDiv.innerText = result;
    });

const iwaRotateKeyButton =
    getRequiredElement('iwa-rotate-key-button') as HTMLButtonElement;

iwaRotateKeyButton.addEventListener('click', async () => {
  const webBundleId =
      getRequiredElement('iwa-kr-web-bundle-id') as HTMLInputElement;
  const publicKeyBase64 =
      getRequiredElement('iwa-kr-public-key-b64') as HTMLInputElement;

  const keyRotationMessageDiv = getRequiredElement('iwa-kr-message');
  keyRotationMessageDiv.innerText = '';

  if (webBundleId.value.length === 0) {
    keyRotationMessageDiv.innerText = `web-bundle-id must not be empty.`;
    return;
  }

  let publicKeyBytes: number[]|null = null;
  if (publicKeyBase64.value.length > 0) {
    try {
      const pk = atob(publicKeyBase64.value);

      publicKeyBytes = [];
      for (let i = 0; i < pk.length; i++) {
        publicKeyBytes.push(pk.charCodeAt(i));
      }
    } catch (err) {
      // This block handles `atob()` errors.
      keyRotationMessageDiv.innerText =
          `${publicKeyBase64.value} is not a base64 encoded key.`;
      return;
    }
  }

  iwaRotateKeyButton.disabled = true;

  // If `publicKeyBytes` are `null`, the app with this `webBundleId` will be
  // disabled.
  webAppInternalsHandler.rotateKey(webBundleId.value, publicKeyBytes);

  // Improve end user experience by providing a delay of 1000 ms to enable the
  // key rotation button.
  setTimeout(() => {
    keyRotationMessageDiv.innerText = `Successfully rotated public key for ${
        webBundleId.value} to ${publicKeyBase64.value}!`;
    publicKeyBase64.value = '';
    webBundleId.value = '';
    iwaRotateKeyButton.disabled = false;
  }, 1000);
});

function formatDevModeLocation(location: IwaDevModeLocation): string {
  if (location.proxyOrigin) {
    return originToText(location.proxyOrigin);
  }
  if (location.bundlePath) {
    return filePathToText(location.bundlePath);
  }
  if (location.updateManifestUrl) {
    return location.updateManifestUrl.url;
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
  const devModeApps: IwaDevModeAppInfo[] =
      (await webAppInternalsHandler.getIsolatedWebAppDevModeAppInfo()).apps;
  const devModeAppList = getRequiredElement('iwa-dev-updates-app-list');
  devModeAppList.replaceChildren();
  if (devModeApps.length === 0) {
    devModeUpdatesMessage.innerText = 'None';
  } else {
    devModeUpdatesMessage.innerText = '';
    for (const {appId, name, location, installedVersion} of devModeApps) {
      const li = document.createElement('li');

      li.innerText =
          `${name} (${installedVersion}) → ${formatDevModeLocation(location)}`;

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

          if (location.bundlePath) {
            const {result}: {result: string} =
                await webAppInternalsHandler
                    .selectFileAndUpdateIsolatedWebAppFromDevBundle(appId);
            updateMsg.innerText = result;
          } else if (location.proxyOrigin) {
            const {result}: {result: string} =
                await webAppInternalsHandler.updateDevProxyIsolatedWebApp(
                    appId);
            updateMsg.innerText = result;
          } else if (location.updateManifestUrl) {
            const {result}: {result: string} =
                await webAppInternalsHandler
                    .updateManifestInstalledIsolatedWebApp(appId);
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
    if (loadTimeData.getBoolean('isIwaKeyDistributionDevModeEnabled')) {
      showIwaSection('iwa-kr-container');
    }
    showIwaSection('iwa-dev-container');
    refreshDevModeAppList();
  }
});
