// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';
import type {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import type {Origin} from 'chrome://resources/mojo/url/mojom/origin.mojom-webui.js';
import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import type {InstallIsolatedWebAppResult, IwaDevModeAppInfo, IwaDevModeLocation, ParseUpdateManifestFromUrlResult, UpdateInfo, UpdateManifest, VersionEntry} from './web_app_internals.mojom-webui.js';
import {WebAppInternalsHandler} from './web_app_internals.mojom-webui.js';

const webAppInternalsHandler = WebAppInternalsHandler.getRemote();

const debugInfoAsJsonString: Promise<string> =
    webAppInternalsHandler.getDebugInfoAsJsonString().then(
        response => response.result);

const iwaDevProxyInstallButton =
    getRequiredElement<HTMLButtonElement>('iwa-dev-install-proxy-button');

const iwaDevProxyInstallUrl =
    getRequiredElement<HTMLInputElement>('iwa-dev-install-proxy-url');

const iwaDevUpdateManifestUrl =
    getRequiredElement<HTMLInputElement>('iwa-dev-update-manifest-url');

const iwaDevUpdateManifestDialog =
    getRequiredElement<HTMLDialogElement>('iwa-update-manifest-dialog');

const iwaSwitchChannelDialog =
    getRequiredElement<HTMLDialogElement>('iwa-switch-channel-input-dialog');

const switchChannelButton =
    getRequiredElement<HTMLButtonElement>('iwa-switch-channel-dialog-switch');

const closeSwitchChannelDialogButton =
    getRequiredElement<HTMLButtonElement>('iwa-switch-channel-dialog-close');

const iwaPinnedVersionDialog =
    getRequiredElement<HTMLDialogElement>('iwa-pinned-version-input-dialog');

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

function setDevInstallMessageText(
    message: string,
) {
  setTimeout(() => {
    getRequiredElement('iwa-dev-install-message').innerText = message;
  }, 0);
}

async function iwaDevProxyInstall() {
  iwaDevProxyInstallButton.disabled = true;

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
    setDevInstallMessageText(
        `Installing IWA: ${iwaDevProxyInstallUrl.value} is not a valid URL`);
    updateDevProxyInstallButtonState();
    return;
  }

  setDevInstallMessageText(`Installing IWA: ${iwaDevProxyInstallUrl.value}...`);

  const location: Url = {url: iwaDevProxyInstallUrl.value};

  const result: InstallIsolatedWebAppResult =
      (await webAppInternalsHandler.installIsolatedWebAppFromDevProxy(location))
          .result;
  if (result.success) {
    setDevInstallMessageText(`Installing IWA: ${
        iwaDevProxyInstallUrl.value} successfully installed.`);
    iwaDevProxyInstallUrl.value = '';
    updateDevProxyInstallButtonState();
    refreshDevModeAppList();
    return;
  }

  setDevInstallMessageText(`Installing IWA: ${
      iwaDevProxyInstallUrl.value} failed to install: ${result.error}`);
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
      setDevInstallMessageText(`Installing IWA from bundle...`);

      const result: InstallIsolatedWebAppResult =
          (await webAppInternalsHandler
               .selectFileAndInstallIsolatedWebAppFromDevBundle())
              .result;
      if (result.success) {
        setDevInstallMessageText(
            `Installing IWA: successfully installed (Web Bundle ID: ${
                result.success.webBundleId}).`);
        refreshDevModeAppList();
        return;
      }

      setDevInstallMessageText(
          `Installing IWA: failed to install: ${result.error}`);
    });

async function iwaDevFetchUpdateManifest() {
  // Validate the provided URL.
  try {
    // We don't need the result of this, only to verify it doesn't throw an
    // exception.
    new URL(iwaDevUpdateManifestUrl.value);
  } catch (_) {
    setDevInstallMessageText(`Fetching the update manifest: ${
        iwaDevUpdateManifestUrl.value} is not a valid URL`);
    return;
  }

  setDevInstallMessageText(
      `Fetching the update manifest at ${iwaDevUpdateManifestUrl.value}...`);

  const updateManifestUrl: Url = {url: iwaDevUpdateManifestUrl.value};

  const result: ParseUpdateManifestFromUrlResult =
      (await webAppInternalsHandler.parseUpdateManifestFromUrl(
           updateManifestUrl))
          .result;
  if (result.error) {
    setDevInstallMessageText(`Installing IWA from update manifest: ${
        iwaDevUpdateManifestUrl.value} failed to install: ${result.error}`);
    return;
  }

  // `result` is a mojo union where there's always one of `error` or
  // `updateManifest` defined.
  const manifest: UpdateManifest = result.updateManifest!;
  const versions: VersionEntry[] = manifest.versions;

  const select = getRequiredElement<HTMLSelectElement>(
      'iwa-update-manifest-version-select');
  select.replaceChildren();

  function compareStringVersions(v1: string, v2: string): number {
    const parts1 = v1.split('.').map(Number);
    const parts2 = v2.split('.').map(Number);

    for (let i = 0; i < Math.max(parts1.length, parts2.length); i++) {
      const part1 = parts1[i] || 0;
      const part2 = parts2[i] || 0;

      if (part1 < part2) {
        return -1;
      } else if (part1 > part2) {
        return 1;
      }
    }

    return 0;
  }

  // Sort versions in descending order
  versions.sort((a, b) => -compareStringVersions(a.version, b.version));

  for (const versionEntry of versions) {
    const option = document.createElement('option');
    option.value = versionEntry.version;
    option.textContent = versionEntry.version;
    select.appendChild(option);
  }

  const installButton = getRequiredElement<HTMLButtonElement>(
      'iwa-update-manifest-dialog-install');

  const closeButton =
      getRequiredElement<HTMLButtonElement>('iwa-update-manifest-dialog-close');

  closeButton.addEventListener('click', () => {
    iwaDevUpdateManifestDialog.close();
  }, {once: true});

  const installEventListener = async () => {
    installButton.removeEventListener('click', installEventListener);

    const selectedVersion = select.value;
    iwaDevUpdateManifestDialog.close();

    setDevInstallMessageText(`Installing version ${selectedVersion} from ${
        updateManifestUrl.url}...`);
    const selectedVersionEntry: VersionEntry|null =
        versions.find(
            versionEntry => versionEntry.version === selectedVersion) ||
        null;

    if (!selectedVersionEntry) {
      setDevInstallMessageText(`Installing version ${selectedVersion} from ${
          updateManifestUrl.url} failed: no such version`);
      return;
    }

    const installResult: InstallIsolatedWebAppResult =
        (await webAppInternalsHandler.installIsolatedWebAppFromBundleUrl({
          webBundleUrl: selectedVersionEntry.webBundleUrl,
          updateInfo: {
            updateManifestUrl,
            // TODO(crbug.com/373396075): Allow selecting the channel.
            updateChannel: 'default',
            pinnedVersion: null,
            allowDowngrades: false,
          },
        })).result;
    if (installResult.success) {
      setDevInstallMessageText(`Installing version ${selectedVersion} from ${
          updateManifestUrl.url}: success!`);
    } else {
      setDevInstallMessageText(`Installing version ${selectedVersion} from ${
          updateManifestUrl.url} failed: ${installResult.error}`);
    }

    refreshDevModeAppList();
  };

  installButton.addEventListener('click', installEventListener);

  iwaDevUpdateManifestDialog.showModal();
}

// Logic for handling the channel switching dialog for IWAs.
function showSwitchChannelDialog(appId: string, name: string) {
  switchChannelButton.addEventListener('click', async () => {
    const updateChannel =
        getRequiredElement<HTMLInputElement>('iwa-update-channel');

    iwaSwitchChannelDialog.close();

    try {
      setDevInstallMessageText(
          `Switching channel to ${updateChannel.value} for ${name}...`);

      const {success} =
          await webAppInternalsHandler.setUpdateChannelForIsolatedWebApp(
              appId,
              updateChannel.value,
          );

      setDevInstallMessageText(
          success ? `Successful channel switch to ${updateChannel.value} for ${
                        name}.` :
                    `Failed to switch channel to ${updateChannel.value} for ${
                        name}.`);

      if (success) {
        refreshDevModeAppList();
      }

    } catch (error) {
      setDevInstallMessageText(
          `An error occurred while switching the update channel of ${name}.`);
      console.error(error);
    }

    updateChannel.value = '';
  }, {once: true});

  iwaSwitchChannelDialog.showModal();
}

closeSwitchChannelDialogButton.addEventListener('click', () => {
  iwaSwitchChannelDialog.close();
});

// Logic for handling the version pinning for IWAs.
function showPinnedVersionDialog(appId: string, name: string) {
  const pinButton =
      getRequiredElement<HTMLButtonElement>('iwa-pinned-version-dialog-pin');
  const unpinButton =
      getRequiredElement<HTMLButtonElement>('iwa-pinned-version-dialog-unpin');

  const pinnedVersion =
      getRequiredElement<HTMLInputElement>('iwa-pinned-version');

  pinButton.addEventListener('click', () => {
    const version = pinnedVersion.value;
    setDevInstallMessageText(`Pinning ${name} to version ${version}...`);

    iwaPinnedVersionDialog.close();

    setTimeout(async () => {
      const {success} =
          await webAppInternalsHandler.setPinnedVersionForIsolatedWebApp(
              appId, version);

      setDevInstallMessageText(
          success ?
              `Successfully pinned ${name} to version ${
                  version}; Version will be applied when an
          update is triggered.` :
              `Something went wrong while setting pinned version of ${name}
          to version ${version}.`);
      if (success) {
        refreshDevModeAppList();
      }
    }, 0);
  }, {once: true});

  unpinButton.addEventListener('click', () => {
    iwaPinnedVersionDialog.close();
    webAppInternalsHandler.resetPinnedVersionForIsolatedWebApp(appId);
  });

  iwaPinnedVersionDialog.showModal();
}

getRequiredElement('iwa-pinned-version-dialog-close')
    .addEventListener('click', () => {
      iwaPinnedVersionDialog.close();
      setDevInstallMessageText('');
    });

// Logic for downgrades
async function toggleAllowDowngrades(appId: string, isChecked: boolean) {
  try {
    await webAppInternalsHandler.setAllowDowngradesForIsolatedWebApp(
        isChecked, appId);
    setTimeout(refreshDevModeAppList, 0);
  } catch (error) {
    setDevInstallMessageText('Error toggling allowDowngrades');
  }
}

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
    getRequiredElement<HTMLButtonElement>('iwa-rotate-key-button');

iwaRotateKeyButton.addEventListener('click', () => {
  const webBundleId =
      getRequiredElement<HTMLInputElement>('iwa-kr-web-bundle-id');
  const publicKeyBase64 =
      getRequiredElement<HTMLInputElement>('iwa-kr-public-key-b64');

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

function formatDevModeLocation(location: IwaDevModeLocation): string|void {
  if (location.proxyOrigin) {
    return originToText(location.proxyOrigin);
  }
  if (location.bundlePath) {
    return filePathToText(location.bundlePath);
  }
  assertNotReached();
}

function describeIsolatedWebApp(
    name: string, installedVersion: string, location: IwaDevModeLocation,
    updateInfo: UpdateInfo|null): string {
  let updateMsg = `${name} (${installedVersion}) →`;
  if (updateInfo) {
    const pinnedVersionValue =
        updateInfo.pinnedVersion ? updateInfo.pinnedVersion : '-';
    updateMsg += ` ${updateInfo.updateManifestUrl.url} ( update_channel: ${
        updateInfo.updateChannel} | pinned_version: ${
        pinnedVersionValue} | allow_downgrades: ${updateInfo.allowDowngrades})`;
  } else {
    updateMsg += ` (${formatDevModeLocation(location)})`;
  }
  return updateMsg;
}

function showIwaSection(containerId: string) {
  getRequiredElement(containerId).style.display = '';
  getRequiredElement('iwa-container').style.display = '';
}

async function refreshDevModeAppList() {
  const devModeUpdatesMessage = getRequiredElement('iwa-dev-updates-message');
  devModeUpdatesMessage.innerText = 'Loading IWAs list...';

  const devModeApps: IwaDevModeAppInfo[] =
      (await webAppInternalsHandler.getIsolatedWebAppDevModeAppInfo()).apps;
  const devModeAppList = getRequiredElement('iwa-dev-updates-app-list');

  devModeAppList.replaceChildren();

  if (devModeApps.length === 0) {
    devModeUpdatesMessage.innerText = 'None';
  } else {
    devModeUpdatesMessage.innerText = '';
    for (const {appId, webBundleId, name, location,
      installedVersion, updateInfo} of devModeApps) {
      const li = document.createElement('li');
      li.innerText =
          describeIsolatedWebApp(name, installedVersion, location, updateInfo);
      li.className = 'iwa-dev-mode-list-item';


      const {updateMsg, buttonsSection} =
          prepareAppButtons(appId, webBundleId, name, location, updateInfo);

      li.appendChild(buttonsSection);
      li.appendChild(updateMsg);

      devModeAppList.appendChild(li);
    }
  }
}

function prepareAppButtons(
    appId: string,
    webBundleId: string,
    name: string,
    location: IwaDevModeLocation,
    updateInfo: UpdateInfo|null,
    ): {updateMsg: HTMLParagraphElement, buttonsSection: HTMLElement} {
  const updateMsg = document.createElement('p');
  const buttonsSection = document.createElement('div');
  buttonsSection.className = 'dev-iwa-buttons';

  const updateBtn = document.createElement('button');
  updateBtn.innerText = 'Perform update now';
  updateBtn.onclick = async () => {
    const oldText = updateBtn.innerText;
    try {
      updateBtn.disabled = true;
      updateBtn.innerText =
          'Performing update... (close the IWA if it is currently open!)';

      if (updateInfo) {
        const {result}: {result: string} =
            await webAppInternalsHandler.updateManifestInstalledIsolatedWebApp(
                appId);
        updateMsg.innerText = result;
      } else if (location.bundlePath) {
        const {result}: {result: string} =
            await webAppInternalsHandler
                .selectFileAndUpdateIsolatedWebAppFromDevBundle(appId);
        updateMsg.innerText = result;
      } else if (location.proxyOrigin) {
        const {result}: {result: string} =
            await webAppInternalsHandler.updateDevProxyIsolatedWebApp(appId);
        updateMsg.innerText = result;
      } else {
        assertNotReached();
      }
    } finally {
      updateBtn.innerText = oldText;
      updateBtn.disabled = false;
    }
  };
  buttonsSection.appendChild(updateBtn);

  const deleteBtn = document.createElement('button');
  deleteBtn.innerText = 'Delete IWA';
  deleteBtn.onclick = async () => {

    const oldText = deleteBtn.innerText;
    try {
      deleteBtn.disabled = true;
      deleteBtn.innerText = 'Deleting IWA...';
      const {success} =
          await webAppInternalsHandler.deleteIsolatedWebApp(appId);

      if (success) {
        await refreshDevModeAppList();
        setDevInstallMessageText(`Successfully uninstalled ${name} \
          (${webBundleId})`);
      } else {
        updateMsg.innerText =
            `Could not uninstall Isolated Web App "${name}" (${webBundleId})`;
        deleteBtn.disabled = false;
      }
    } catch (e) {
      updateMsg.innerText =
          `An error occurred during deletion of isolated Web App "${name}" \
          (${webBundleId})`;
      deleteBtn.disabled = false;
      console.error(e);
    } finally {
      if (!deleteBtn.disabled) {
          deleteBtn.innerText = oldText;
      }
    }
  };
  buttonsSection.appendChild(deleteBtn);

  if (updateInfo) {
    const switchChannelBtn = document.createElement('button');
    switchChannelBtn.innerText = 'Switch channel';
    switchChannelBtn.onclick = () => {
      showSwitchChannelDialog(appId, name);
    };
    buttonsSection.appendChild(switchChannelBtn);

    const pinnedVersionBtn = document.createElement('button');
    pinnedVersionBtn.innerText = 'Pin To Version';
    pinnedVersionBtn.onclick = () => {
      showPinnedVersionDialog(appId, name);
    };
    buttonsSection.appendChild(pinnedVersionBtn);

    const allowDowngradesToggle = document.createElement('input');
    allowDowngradesToggle.type = 'checkbox';
    allowDowngradesToggle.checked = updateInfo.allowDowngrades || false;
    allowDowngradesToggle.id = 'allow-downgrades-toggle';

    const allowDowngradesLabel = document.createElement('label');
    allowDowngradesLabel.htmlFor = 'allow-downgrades-toggle';
    allowDowngradesLabel.textContent = 'Allow downgrades';

    allowDowngradesToggle.addEventListener('change', () => {
      toggleAllowDowngrades(appId, allowDowngradesToggle.checked);
    });

    buttonsSection.appendChild(allowDowngradesToggle);
    buttonsSection.appendChild(allowDowngradesLabel);
  }

  return {updateMsg, buttonsSection};
}

document.addEventListener('DOMContentLoaded', () => {
  if (loadTimeData.getBoolean('isIwaPolicyInstallEnabled')) {
    showIwaSection('iwa-updates-container');
  }

  if (loadTimeData.getBoolean('isIwaDevModeEnabled')) {
    if (loadTimeData.getBoolean('isIwaKeyDistributionDevModeEnabled')) {
      showIwaSection('iwa-kr-container');
    }

    showIwaSection('iwa-dev-container');
    const devModeUpdatesMessage = getRequiredElement('iwa-dev-updates-message');
    devModeUpdatesMessage.innerText = 'Loading...';

    refreshDevModeAppList();
  }

  setTimeout(async () => {
    getRequiredElement('json').innerText = await debugInfoAsJsonString;
  }, 0);
});
