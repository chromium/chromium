// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

HTMLImports.whenReady(() => {
  // <include src="../login/components/behaviors/multi_step_behavior.js">
  // Need the display manager to imitate the oobe flow.
  // <include src="../login/components/display_manager_types.js">
  // <include src="../login/display_manager.js">
  // <include src="../login/components/oobe_types.js">
  // <include src="browser_proxy.js">

  cr.define('login.AssistantOptInFlowScreen', function() {
    return {

      /**
       * Starts the assistant opt-in flow.
       */
      show() {
        var url = new URL(document.URL);
        $('assistant-optin-flow-card').onBeforeShow();
        $('assistant-optin-flow-card')
            .onShow(
                url.searchParams.get('flow-type'),
                url.searchParams.get('caption-bar-height'));
      },

      /**
       * Reloads localized strings.
       * @param {!Object} data New dictionary with i18n values.
       */
      reloadContent(data) {
        $('assistant-optin-flow-card').reloadContent(data);
      },

      /**
       * Add a setting zippy object in the corresponding screen.
       * @param {string} type type of the setting zippy.
       * @param {!Object} data String and url for the setting zippy.
       */
      addSettingZippy(type, data) {
        $('assistant-optin-flow-card').addSettingZippy(type, data);
      },

      /**
       * Show the next screen in the flow.
       */
      showNextScreen() {
        $('assistant-optin-flow-card').showNextScreen();
      },

      /**
       * Called when the Voice match state is updated.
       * @param {string} state the voice match state.
       */
      onVoiceMatchUpdate(state) {
        $('assistant-optin-flow-card').onVoiceMatchUpdate(state);
      },

      /**
       * Called to show the next settings when there are multiple unbundled
       * activity control settings in the Value prop screen.
       */
      onValuePropUpdate() {
        $('assistant-optin-flow-card').onValuePropUpdate();
      },

      /**
       * Called when the flow finished and close the dialog.
       */
      closeDialog() {
        assistant.BrowserProxyImpl.getInstance().dialogClose();
      },
    };
  });

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', function() {
      login.AssistantOptInFlowScreen.show();
    });
  } else {
    login.AssistantOptInFlowScreen.show();
  }
});
