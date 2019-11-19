/* Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

// Javascript for chrome://reset-password/ WebUI page.

import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-styles/color.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import './reset_password.mojom-lite.js';

import {$} from 'chrome://resources/js/util.m.js';

/** @type {mojom.ResetPasswordHandlerRemote} */
let pageHandler;

document.addEventListener('DOMContentLoaded', function() {
  pageHandler = mojom.ResetPasswordHandler.getRemote();

  /** @type {?HTMLElement} */
  const resetPasswordButton = $('reset-password-button');
  resetPasswordButton.addEventListener('click', function() {
    pageHandler.handlePasswordReset();
  });
});
