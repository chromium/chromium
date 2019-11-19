// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('account_manager_error', function() {
  'use strict';

  function initialize() {
    $('ok-button').addEventListener('click', closeDialog);
  }

  function closeDialog() {
    chrome.send('closeDialog');
  }

  return {
    initialize: initialize,
    closeDialog: closeDialog,
  };
});

document.addEventListener('DOMContentLoaded', account_manager_error.initialize);
