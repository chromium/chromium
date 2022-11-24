// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cloud_upload_dialog.js';
import './drive_upload_page.js';
import './file_handler_page.js';

import {assert} from 'chrome://resources/js/assert_ts.js';

import {DialogPage} from './cloud_upload.mojom-webui.js';
import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from './move_confirmation_page.js';

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
  case DialogPage.kGoogleDriveSetup: {
    document.body.append(document.createElement('drive-upload-page'));
    break;
  }
  case DialogPage.kMoveConfirmationOneDrive: {
    const movePage = new MoveConfirmationPageElement();
    movePage.setCloudProvider(CloudProvider.ONE_DRIVE);
    document.body.append(movePage);
    break;
  }
  case DialogPage.kMoveConfirmationGoogleDrive: {
    const movePage = new MoveConfirmationPageElement();
    movePage.setCloudProvider(CloudProvider.GOOGLE_DRIVE);
    document.body.append(movePage);
    break;
  }
}