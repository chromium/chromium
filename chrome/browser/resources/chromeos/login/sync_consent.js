// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design Sync Consent
 * screen.
 */

Polymer({
  is: 'sync-consent',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

  /** @override */
  ready: function() {
    this.updateLocalizedContent();
  },

  focus: function() {
    let activeScreen = this.getActiveScreen_();
    if (activeScreen)
      activeScreen.focus();
  },

  /**
   * Hides all screens to help switching from one screen to another.
   * @private
   */
  hideAllScreens_: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens)
      screen.hidden = true;
  },

  /**
   * Returns active screen or null if none.
   * @private
   */
  getActiveScreen_: function() {
    var screens = Polymer.dom(this.root).querySelectorAll('oobe-dialog');
    for (let screen of screens) {
      if (!screen.hidden)
        return screen;
    }
    return null;
  },

  /**
   * Shows given screen.
   * @param id String Screen ID.
   * @private
   */
  showScreen_: function(id) {
    this.hideAllScreens_();

    var screen = this.$[id];
    assert(screen);
    screen.hidden = false;
    screen.show();
    screen.focus();
  },

  /**
   * Reacts to changes in loadTimeData.
   */
  updateLocalizedContent: function() {
    if (loadTimeData.getBoolean('splitSettingsSync')) {
      // SplitSettingsSync version.
      this.showScreen_('osSyncConsentDialog');
    } else {
      // Regular version.
      this.showScreen_('syncConsentOverviewDialog');
    }
    this.i18nUpdateLocale();
  },

  /**
   * This is 'on-tap' event handler for 'AcceptAndContinue' button.
   * @private
   */
  onSettingsSaveAndContinue_: function(e) {
    if (this.$.reviewSettingsBox.checked) {
      chrome.send('login.SyncConsentScreen.continueAndReview', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    } else {
      chrome.send('login.SyncConsentScreen.continueWithDefaults', [
        this.getConsentDescription_(), this.getConsentConfirmation_(e.path)
      ]);
    }
  },

  /**
   * @param {!Event} event
   * @private
   */
  onOsSyncAcceptAndContinue_: function(event) {
    assert(loadTimeData.getBoolean('splitSettingsSync'));
    assert(event.path);
    let enableOsSync = !!this.$.enableOsSyncToggle.checked;
    chrome.send('login.SyncConsentScreen.osSyncAcceptAndContinue', [
      this.getConsentDescription_(), this.getConsentConfirmation_(event.path),
      enableOsSync
    ]);
  },

  /**
   * @param {!Array<!HTMLElement>} path Path of the click event. Must contain
   *     a consent confirmation element.
   * @return {string} The text of the consent confirmation element.
   * @private
   */
  getConsentConfirmation_: function(path) {
    for (let element of path) {
      if (!element.hasAttribute)
        continue;

      if (element.hasAttribute('consent-confirmation'))
        return element.innerHTML.trim();

      // Search down in case of click on a button with description below.
      let labels = element.querySelectorAll('[consent-confirmation]');
      if (labels && labels.length > 0) {
        assert(labels.length == 1);

        let result = '';
        for (let label of labels) {
          result += label.innerHTML.trim();
        }
        return result;
      }
    }
    assertNotReached('No consent confirmation element found.');
    return '';
  },

  /** @return {!Array<string>} Text of the consent description elements. */
  getConsentDescription_: function() {
    let consentDescription =
        Array.from(this.shadowRoot.querySelectorAll('[consent-description]'))
            .filter(element => element.clientWidth * element.clientHeight > 0)
            .map(element => element.innerHTML.trim());
    assert(consentDescription);
    return consentDescription;
  },
});
