// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_migration_welcome', function() {
  'use strict';

  let userEmail;

  /**
   * Initializes the UI.
   */
  function initialize() {
    setWelcomeTextContent();

    $('cancel-button').addEventListener('click', closeDialog);
    $('migrate-button').addEventListener('click', reauthenticateAccount);
  }

  function setWelcomeTextContent() {
    const dialogArgs = chrome.getVariableValue('dialogArguments');
    if (!dialogArgs) {
      // Only if the user navigates to the URL
      // chrome://account-migration-welcome to debug.
      console.warn('No arguments were provided to the dialog.');
      return;
    }
    const args = JSON.parse(dialogArgs);
    assert(args);
    assert(args.email);
    userEmail = args.email;

    $('welcome-title').textContent =
        loadTimeData.getStringF('welcomeTitle', userEmail);
    $('welcome-message').innerHTML = loadTimeData.getStringF(
        'welcomeMessage', userEmail,
        loadTimeData.getString('accountManagerLearnMoreUrl'));
  }

  /**
   * @return {AccountMigrationBrowserProxy}
   */
  function getBrowserProxy() {
    return account_migration.AccountMigrationBrowserProxyImpl.getInstance();
  }

  function closeDialog() {
    getBrowserProxy().closeDialog();
  }

  function reauthenticateAccount() {
    getBrowserProxy().reauthenticateAccount(userEmail);
  }

  return {
    initialize: initialize,
  };
});

document.addEventListener(
    'DOMContentLoaded', account_migration_welcome.initialize);
