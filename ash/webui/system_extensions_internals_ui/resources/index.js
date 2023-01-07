// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {pageHandler} from './page_handler.js';

const installedStatus = document.querySelector('#installed-status');
async function updateIsInstallStatus() {
  const {isInstalled} = await pageHandler.isSystemExtensionInstalled();
  if (isInstalled) {
    installedStatus.textContent = 'System Extension is installed';
  } else {
    installedStatus.textContent = 'System Extension is not installed';
  }
}

updateIsInstallStatus();

const chooseDirButton = document.querySelector('#choose-directory');
const resultDialog = document.querySelector('#result-dialog');
chooseDirButton.addEventListener('click', async event => {
  const directory = await window.showDirectoryPicker({startIn: 'downloads'});
  const {success} = await pageHandler.installSystemExtensionFromDownloadsDir(
      {path: {path: directory.name}});
  updateIsInstallStatus();

  if (success) {
    resultDialog.textContent =
        `System Extension in '${directory.name}' was successfully installed.`;
  } else {
    resultDialog.textContent =
        `System Extension in '${directory.name}' failed to be installed.`;
  }
  resultDialog.showModal();
});

const uninstallButton = document.querySelector('#uninstall');
uninstallButton.addEventListener('click', async event => {
  await pageHandler.uninstallSystemExtension();
  updateIsInstallStatus();
});
