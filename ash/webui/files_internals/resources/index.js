// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler} from '/ash/webui/files_internals/mojom/files_internals.mojom-webui.js';

const pageHandler = PageHandler.getRemote();

document.addEventListener('DOMContentLoaded', async () => {
  // SMB
  const verboseElem = document.getElementById('smb-verbose-logging-toggle');
  verboseElem.checked =
      (await pageHandler.getSmbfsEnableVerboseLogging()).enabled;
  verboseElem.addEventListener('change', (e) => {
    pageHandler.setSmbfsEnableVerboseLogging(e.target.checked);
  });

  // Office file handlers
  const clearSetupButton =
      document.getElementById('clear-office-setup-complete');
  clearSetupButton.addEventListener('click', () => {
    pageHandler.setOfficeSetupComplete(false);
  });
  const officeSetupStatus =
      document.getElementById('office-setup-complete-status');
  const officeSetupComplete =
      (await pageHandler.getOfficeSetupComplete()).complete;
  officeSetupStatus.innerText = officeSetupComplete ? 'Yes' : 'No';

  const clearAlwaysMoveButton =
      document.getElementById('clear-office-always-move');
  clearAlwaysMoveButton.addEventListener('click', () => {
    pageHandler.setAlwaysMoveOfficeFiles(false);
  });
  const officeAlwaysMoveStatus =
      document.getElementById('office-always-move-status');
  const officeAlwaysMove =
      (await pageHandler.getAlwaysMoveOfficeFiles()).alwaysMove;
  officeAlwaysMoveStatus.innerText = officeAlwaysMove ? 'Yes' : 'No';
});
