// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandler} from '/ash/webui/files_internals/mojom/files_internals.mojom-webui.js';

const pageHandler = PageHandler.getRemote();

const refreshOfficeSetupComplete = async () => {
  const officeSetupStatus =
      document.getElementById('office-setup-complete-status');
  const officeSetupComplete =
      (await pageHandler.getOfficeSetupComplete()).complete;
  officeSetupStatus.innerText = officeSetupComplete ? 'Yes' : 'No';
};

const refreshMoveConfirmationShownForDrive = async () => {
  const moveConfirmationShownForDriveStatus =
      document.getElementById('move-confirmation-shown-for-drive-status');
  const moveConfirmationShownForDrive =
      (await pageHandler.getMoveConfirmationShownForDrive()).confirmationShown;
  moveConfirmationShownForDriveStatus.innerText =
      moveConfirmationShownForDrive ? 'Yes' : 'No';
};

const refreshMoveConfirmationShownForOneDrive = async () => {
  const moveConfirmationShownForOneDriveStatus =
      document.getElementById('move-confirmation-shown-for-onedrive-status');
  const moveConfirmationShownForOneDrive =
      (await pageHandler.getMoveConfirmationShownForOneDrive())
          .confirmationShown;
  moveConfirmationShownForOneDriveStatus.innerText =
      moveConfirmationShownForOneDrive ? 'Yes' : 'No';
};

const refreshAlwaysMoveToDriveStatus = async () => {
  const officeAlwaysMoveToDriveStatus =
      document.getElementById('office-always-move-to-drive-status');
  const officeAlwaysMoveToDrive =
      (await pageHandler.getAlwaysMoveOfficeFilesToDrive()).alwaysMove;
  officeAlwaysMoveToDriveStatus.innerText =
      officeAlwaysMoveToDrive ? 'Yes' : 'No';
};

const refreshAlwaysMoveToOneDriveStatus = async () => {
  const officeAlwaysMoveToOneDriveStatus =
      document.getElementById('office-always-move-to-onedrive-status');
  const officeAlwaysMoveToOneDrive =
      (await pageHandler.getAlwaysMoveOfficeFilesToOneDrive()).alwaysMove;
  officeAlwaysMoveToOneDriveStatus.innerText =
      officeAlwaysMoveToOneDrive ? 'Yes' : 'No';
};

document.addEventListener('DOMContentLoaded', async () => {
  // SMB
  const verboseElem = document.getElementById('smb-verbose-logging-toggle');
  verboseElem.checked =
      (await pageHandler.getSmbfsEnableVerboseLogging()).enabled;
  verboseElem.addEventListener('change', (e) => {
    pageHandler.setSmbfsEnableVerboseLogging(e.target.checked);
  });

  // Office file handlers
  refreshOfficeSetupComplete();
  refreshMoveConfirmationShownForDrive();
  refreshMoveConfirmationShownForOneDrive();
  refreshAlwaysMoveToDriveStatus();
  refreshAlwaysMoveToOneDriveStatus();

  const clearSetupButton =
      document.getElementById('clear-office-setup-complete');
  clearSetupButton.addEventListener('click', () => {
    pageHandler.setOfficeSetupComplete(false);
    refreshOfficeSetupComplete();
    refreshMoveConfirmationShownForDrive();
    refreshMoveConfirmationShownForOneDrive();
  });

  const clearAlwaysMoveToDriveButton =
      document.getElementById('clear-office-always-move-to-drive');
  clearAlwaysMoveToDriveButton.addEventListener('click', () => {
    pageHandler.setAlwaysMoveOfficeFilesToDrive(false);
    refreshAlwaysMoveToDriveStatus();
  });

  const clearAlwaysMoveToOneDriveButton =
      document.getElementById('clear-office-always-move-to-onedrive');
  clearAlwaysMoveToOneDriveButton.addEventListener('click', () => {
    pageHandler.setAlwaysMoveOfficeFilesToOneDrive(false);
    refreshAlwaysMoveToOneDriveStatus();
  });
});
