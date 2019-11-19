// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview First user log in Recommend Apps screen implementation.
 */

login.createScreen('RecommendAppsScreen', 'recommend-apps', function() {
  return {
    EXTERNAL_API:
        ['loadAppList', 'setThrobberVisible', 'setWebview', 'showError'],

    /**
     * Returns the control which should receive initial focus.
     */
    get defaultControl() {
      return $('recommend-apps-screen');
    },

    /**
     * Returns requested element from related part of HTML.
     * @param {string} id Id of an element to find.
     *
     * @private
     */
    getElement_: function(id) {
      return $('recommend-apps-screen').getElement(id);
    },

    /**
     * Adds new class to the list of classes of root OOBE style.
     * @param {string} className class to remove.
     *
     * @private
     */
    addClass_: function(className) {
      $('recommend-apps-screen')
          .getElement('recommend-apps-dialog')
          .classList.add(className);
    },

    /**
     * Removes class from the list of classes of root OOBE style.
     * @param {string} className class to remove.
     *
     * @private
     */
    removeClass_: function(className) {
      $('recommend-apps-screen')
          .getElement('recommend-apps-dialog')
          .classList.remove(className);
    },

    /**
     * Makes sure that UI is initialized.
     *
     * @private
     */
    ensureInitialized_: function() {
      $('recommend-apps-screen').screen = this;
      window.addEventListener('message', this.onMessage);
    },

    /**
     * Shows error UI when it fails to load the recommended app list.
     */
    showError: function() {
      this.ensureInitialized_();

      // Hide the loading throbber and show the error message.
      this.setThrobberVisible(false);
      this.removeClass_('recommend-apps-loading');
      this.removeClass_('recommend-apps-loaded');
      this.addClass_('error');

      this.getElement_('recommend-apps-retry-button').focus();
    },

    setWebview: function(contents) {
      const appListView = this.getElement_('app-list-view');
      appListView.src =
          'data:text/html;charset=utf-8,' + encodeURIComponent(contents);
    },

    /**
     * Generate the contents in the webview.
     */
    loadAppList: function(appList) {
      this.ensureInitialized_();

      // Hide the loading throbber and show the recommend app list.
      this.setThrobberVisible(false);

      // Disable install button until the webview reports that some apps are
      // selected.
      $('recommend-apps-screen')
          .getElement('recommend-apps-install-button')
          .disabled = true;

      const appListView = this.getElement_('app-list-view');
      const subtitle = this.getElement_('subtitle');
      subtitle.innerText = loadTimeData.getStringF(
          'recommendAppsScreenDescription', appList.length);
      appListView.addEventListener('contentload', () => {
        appListView.executeScript({file: 'recommend_app_list_view.js'}, () => {
          appListView.contentWindow.postMessage('initialMessage', '*');

          appList.forEach(function(app, index) {
            let generateItemScript = 'generateContents("' + app.icon + '", "' +
                app.name + '", "' + app.package_name + '");';
            const generateContents = {code: generateItemScript};
            appListView.executeScript(generateContents);
          });
          const addScrollShadowEffectScript = 'addScrollShadowEffect();';
          appListView.executeScript({code: addScrollShadowEffectScript});

          const getNumOfSelectedAppsScript = 'sendNumberOfSelectedApps();';
          appListView.executeScript({code: getNumOfSelectedAppsScript});

          this.onGenerateContents();
        });
      });
    },

    /**
     * Handles event when contents in the webview is generated.
     */
    onGenerateContents() {
      this.removeClass_('recommend-apps-loading');
      this.removeClass_('error');
      this.addClass_('recommend-apps-loaded');
      this.getElement_('recommend-apps-install-button').focus();
    },

    /**
     * Handles Skip button click.
     */
    onSkip: function() {
      chrome.send('recommendAppsSkip');
    },

    /**
     * Handles Install button click.
     */
    onInstall: function() {
      // Only start installation if the button is not disabled.
      if (!this.getElement_('recommend-apps-install-button').disabled) {
        const appListView = this.getElement_('app-list-view');
        appListView.executeScript(
            {code: 'getSelectedPackages();'}, function(result) {
              console.log(result[0]);
              chrome.send('recommendAppsInstall', result[0]);
            });
      }
    },

    /**
     * Handles Retry button click.
     */
    onRetry: function() {
      this.setThrobberVisible(true);
      this.removeClass_('recommend-apps-loaded');
      this.removeClass_('error');
      this.addClass_('recommend-apps-loading');

      chrome.send('recommendAppsRetry');
    },

    /**
     * Handles the message sent from the WebView.
     */
    onMessage: function(event) {
      if (event.data.type && (event.data.type === 'NUM_OF_SELECTED_APPS')) {
        const numOfSelected = event.data.numOfSelected;
        $('recommend-apps-screen')
            .getElement('recommend-apps-install-button')
            .disabled = (numOfSelected === 0);
      }
    },

    /**
     * This is called to show/hide the loading UI.
     * @param {boolean} visible whether to show loading UI.
     */
    setThrobberVisible: function(visible) {
      $('recommend-apps-loading').hidden = !visible;
      $('recommend-apps-screen').hidden = visible;
    },
  };
});
