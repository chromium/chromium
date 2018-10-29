/* Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

cr.define('signin.emailConfirmation', function() {
  'use strict';

  function initialize() {
    var args = JSON.parse(chrome.getVariableValue('dialogArguments'));
    var lastEmail = args.lastEmail;
    var newEmail = args.newEmail;
    $('dialogTitle').textContent =
        loadTimeData.getStringF('signinEmailConfirmationTitle', lastEmail);
    $('createNewUserRadioButtonSubtitle').textContent = loadTimeData.getStringF(
        'signinEmailConfirmationCreateProfileButtonSubtitle', newEmail);
    $('startSyncRadioButtonSubtitle').textContent = loadTimeData.getStringF(
        'signinEmailConfirmationStartSyncButtonSubtitle', newEmail);

    document.addEventListener('keydown', onKeyDown);
    $('confirmButton').addEventListener('click', onConfirm);
    $('closeButton').addEventListener('click', onCancel);
  }

  function onKeyDown(e) {
    // If the currently focused element isn't something that performs an action
    // on "enter" being pressed and the user hits "enter", perform the default
    // action of the dialog, which is "OK".
    if (e.key == 'Enter' &&
        !/^(A|PAPER-BUTTON)$/.test(document.activeElement.tagName)) {
      $('confirmButton').click();
      e.preventDefault();
    }
  }

  function onConfirm(e) {
    const action = document.querySelector('cr-radio-group').selected;
    chrome.send('dialogClose', [JSON.stringify({'action': action})]);
  }

  function onCancel(e) {
    chrome.send('dialogClose', [JSON.stringify({'action': 'cancel'})]);
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener(
    'DOMContentLoaded', signin.emailConfirmation.initialize);
