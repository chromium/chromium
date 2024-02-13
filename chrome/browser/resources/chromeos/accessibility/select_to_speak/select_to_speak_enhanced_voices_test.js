// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

SelectToSpeakEnhancedNetworkTtsVoicesTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
    this.confirmationDialogShowCount_ = 0;
    this.confirmationDialogResponse_ = true;

    chrome.accessibilityPrivate.showConfirmationDialog =
        (title, description, cancelName, callback) => {
          this.confirmationDialogShowCount_ += 1;
          callback(this.confirmationDialogResponse_);
        };

    chrome.i18n = {
      getMessage(msgid) {
        return msgid;
      },
    };
  }

  // Sets the policy to allow or disallow the network voices.
  // Waits for the setting to propagate.
  async setEnhancedNetworkVoicesPolicy(allowed) {
    chrome.settingsPrivate.setPref(
        PrefsManager.ENHANCED_VOICES_POLICY_KEY, allowed);
    await new Promise(
        resolve => selectToSpeak.prefsManager_
                       .updateSettingsPrefsCallbackForTest_ = () => {
          if (selectToSpeak.prefsManager_.enhancedNetworkVoicesAllowed_ ===
              allowed) {
            selectToSpeak.prefsManager_.updateSettingsPrefsCallbackForTest_ =
                null;
            resolve();
          }
        });
  }
};

AX_TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'EnablesVoicesIfConfirmedInDialog', async function() {
      this.confirmationDialogResponse_ = true;

      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Speech starts asynchronously.
        assertEquals(this.confirmationDialogShowCount_, 1);
        assertTrue(selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
        assertTrue(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });

AX_TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesVoicesIfCanceledInDialog', async function() {
      this.confirmationDialogResponse_ = false;

      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Speech starts asynchronously.
        assertEquals(this.confirmationDialogShowCount_, 1);
        assertTrue(selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
        assertFalse(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });

AX_TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesVoicesIfDisallowedByPolicy', async function() {
      this.confirmationDialogResponse_ = true;

      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      this.mockTts.setOnSpeechCallbacks([this.newCallback(async function(
          utterance) {
        // Network voices are enabled initially because of the
        // confirmation.
        assertEquals(this.confirmationDialogShowCount_, 1);
        assertTrue(selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
        assertTrue(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());

        // Sets the policy to disallow network voices.
        await this.setEnhancedNetworkVoicesPolicy(/* allowed= */ false);
        assertFalse(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });

AX_TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesDialogIfDisallowedByPolicy', async function() {
      await this.setEnhancedNetworkVoicesPolicy(/* allowed= */ false);

      // For some reason after setting enhanced network voices pref
      // we often lose mockTts on Select to Speak. Ensure it's set.
      chrome.tts = this.mockTts;

      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      assertFalse(this.mockTts.currentlySpeaking());
      assertEquals(this.mockTts.pendingUtterances().length, 0);
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Dialog was not shown.
        assertEquals(this.confirmationDialogShowCount_, 0);
        assertFalse(selectToSpeak.prefsManager_.enhancedVoicesDialogShown());

        // Speech proceeds without enhanced voices.
        assertFalse(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(
            this.mockTts.pendingUtterances()[0], 'This is some text');
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1,
      };
      this.triggerReadMouseSelectedText(event, event);
    });
