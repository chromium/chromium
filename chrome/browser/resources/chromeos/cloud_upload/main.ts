// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cloud_upload_dialog.js';
import './connect_onedrive.js';
import './file_handler_page.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert} from 'chrome://resources/js/assert_ts.js';

import {DialogPage} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from './move_confirmation_page.js';


window.addEventListener('load', () => {
  const jellyEnabled = loadTimeData.getBoolean('isJellyEnabled');
  const theme = jellyEnabled ? 'refresh23' : 'legacy';
  document.documentElement.setAttribute('theme', theme);
  ColorChangeUpdater.forDocument().start();
});

const dialogArgs =
    await CloudUploadBrowserProxy.getInstance().handler.getDialogArgs();
assert(dialogArgs.args);

switch (dialogArgs.args.dialogPage) {
  case DialogPage.kFileHandlerDialog: {
    document.body.append(document.createElement('file-handler-page'));
    break;
  }
  case DialogPage.kOneDriveSetup: {
    document.body.append(document.createElement('cloud-upload'));
    break;
  }
  case DialogPage.kMoveConfirmationOneDrive: {
    const movePage = new MoveConfirmationPageElement();
    await movePage.setDialogAttributes(
        dialogArgs.args.fileNames.length, dialogArgs.args.operationType,
        CloudProvider.ONE_DRIVE);
    document.body.append(movePage);
    break;
  }
  case DialogPage.kMoveConfirmationGoogleDrive: {
    const movePage = new MoveConfirmationPageElement();
    await movePage.setDialogAttributes(
        dialogArgs.args.fileNames.length, dialogArgs.args.operationType,
        CloudProvider.GOOGLE_DRIVE);
    document.body.append(movePage);
    break;
  }
  case DialogPage.kConnectToOneDrive: {
    document.body.append(document.createElement('connect-onedrive'));
    break;
  }
}
