// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../login/hd-iron-icon.js">
// <include src="../login/oobe_types.js">
// <include src="../login/oobe_buttons.js">
// <include src="../login/oobe_change_picture.js">
// <include src="../login/oobe_dialog_host_behavior.js">
// <include src="../login/oobe_dialog.js">
// <include src="assistant_optin_flow.js">

cr.define('login.AssistantOptInFlowScreen', function() {
  return {

    /**
     * Starts the assistant opt-in flow.
     */
    show: function() {
      var url = new URL(document.URL);
      $('assistant-optin-flow-card')
          .onShow(
              url.searchParams.get('flow-type'),
              url.searchParams.get('caption-bar-height'));
    },

    /**
     * Reloads localized strings.
     * @param {!Object} data New dictionary with i18n values.
     */
    reloadContent: function(data) {
      $('assistant-optin-flow-card').reloadContent(data);
    },

    /**
     * Add a setting zippy object in the corresponding screen.
     * @param {string} type type of the setting zippy.
     * @param {!Object} data String and url for the setting zippy.
     */
    addSettingZippy: function(type, data) {
      $('assistant-optin-flow-card').addSettingZippy(type, data);
    },

    /**
     * Show the next screen in the flow.
     */
    showNextScreen: function() {
      $('assistant-optin-flow-card').showNextScreen();
    },

    /**
     * Called when the Voice match state is updated.
     * @param {string} state the voice match state.
     */
    onVoiceMatchUpdate: function(state) {
      $('assistant-optin-flow-card').onVoiceMatchUpdate(state);
    },

    closeDialog: function() {
      chrome.send('dialogClose');
    },
  };
});

document.addEventListener('DOMContentLoaded', function() {
  login.AssistantOptInFlowScreen.show();
});
