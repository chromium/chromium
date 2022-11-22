/* Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Frontend for chrome://reset-password/ WebUI page.

import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';

import {getRequiredElement} from 'chrome://resources/js/util_ts.js';

import {ResetPasswordHandler, ResetPasswordHandlerRemote} from './reset_password.mojom-webui.js';

let pageHandler: ResetPasswordHandlerRemote;

document.addEventListener('DOMContentLoaded', function() {
  pageHandler = ResetPasswordHandler.getRemote();

  const resetPasswordButton = getRequiredElement('reset-password-button');
  resetPasswordButton.addEventListener('click', function() {
    pageHandler.handlePasswordReset();
  });
});
