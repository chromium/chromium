// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cloud_upload_dialog.js';
import './connect_onedrive.js';
import './file_handler_page.js';
import './strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';

import {CloudUploadBrowserProxy} from './cloud_upload_browser_proxy.js';
import {CloudProvider, MoveConfirmationPageElement} from './move_confirmation_page.js';


window.addEventListener('load', () => {
  ColorChangeUpdater.forDocument().start();
});

const dialogArgs =
    await CloudUploadBrowserProxy.getInstance().handler.getDialogArgs();
assert(dialogArgs.args);

const dialogSpecificArgs = dialogArgs.args.dialogSpecificArgs;
assert(dialogSpecificArgs);

if (dialogSpecificArgs.fileHandlerDialogArgs) {
  document.body.append(document.createElement('file-handler-page'));
} else if (dialogSpecificArgs.oneDriveSetupDialogArgs) {
  document.body.append(document.createElement('cloud-upload'));
} else if (dialogSpecificArgs.moveConfirmationOneDriveDialogArgs) {
  const movePage = new MoveConfirmationPageElement();
  await movePage.setDialogAttributes(
      dialogArgs.args.fileNames.length,
      dialogSpecificArgs.moveConfirmationOneDriveDialogArgs.operationType,
      CloudProvider.ONE_DRIVE);
  document.body.append(movePage);
} else if (dialogSpecificArgs.moveConfirmationGoogleDriveDialogArgs) {
  const movePage = new MoveConfirmationPageElement();
  await movePage.setDialogAttributes(
      dialogArgs.args.fileNames.length,
      dialogSpecificArgs.moveConfirmationGoogleDriveDialogArgs.operationType,
      CloudProvider.GOOGLE_DRIVE);
  document.body.append(movePage);
} else if (dialogSpecificArgs.connectToOneDriveDialogArgs) {
  document.body.append(document.createElement('connect-onedrive'));
} else {
  assertNotReached();
}
