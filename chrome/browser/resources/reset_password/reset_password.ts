/* Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Frontend for chrome://reset-password/ WebUI page.

import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';

import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {ResetPasswordHandlerRemote} from './reset_password.mojom-webui.js';
import {ResetPasswordHandler} from './reset_password.mojom-webui.js';

let pageHandler: ResetPasswordHandlerRemote;

document.addEventListener('DOMContentLoaded', function() {
  pageHandler = ResetPasswordHandler.getRemote();

  const resetPasswordButton = getRequiredElement('reset-password-button');
  resetPasswordButton.addEventListener('click', function() {
    pageHandler.handlePasswordReset();
  });
});
