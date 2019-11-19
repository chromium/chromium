// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

// Install a global error handler so stack traces are included in logs.
window.onerror = function(message, file, line, column, error) {
  console.error(error.stack);
};

// <include src="../hd-iron-icon.js">
// <include src="../oobe_buttons.js">
// <include src="../oobe_dialog.js">
// <include src="../oobe_dialog_host_behavior.js">
// <include src="discover_components.js">

/**
 * @fileoverview Discover UI based on a stripped down OOBE controller.
 */
cr.define('cr.ui.Oobe', function() {
  return {
    /**
     * Reports that JS side is ready.
     */
    initialize: function() {
      chrome.send('screenStateInitialize');
      $('discoverUI').onBeforeShow();
    },

    // Dummy Oobe functions not present with stripped login UI.
    enableKeyboardFlow: function(data) {},
    refreshA11yInfo: function(data) {},
    setClientAreaSize: function(data) {},
    setLabelText: function(data) {},
    setShelfHeight: function(data) {},
    setDialogPaddingMode: function(data) {},
    setVirtualKeyboardShown: function(data) {},
    showAPIKeysNotice: function(data) {},
    showOobeUI: function(data) {},
    showVersion: function(data) {},
    updateDeviceRequisition: function(data) {},
    updateOobeConfiguration: function(data) {},


    /**
     * Reloads content of the page.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      loadTimeData.overrideValues(data);
      i18nTemplate.process(document, loadTimeData);
      $('discoverUI').updateLocalizedContent();
    },
  };
});

var Oobe = cr.ui.Oobe;

document.addEventListener('DOMContentLoaded', function() {
  Oobe.initialize();
});

// In App mode Discover should not exit once last module is completed.
// It should go back to "Welcome" screen instead.
$('discoverUI').addEventListener('discover-done', function() {
  $('discoverUI').onBeforeShow();
});
