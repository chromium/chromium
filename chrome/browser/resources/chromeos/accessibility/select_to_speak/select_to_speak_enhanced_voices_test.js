// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['../../../../../../ui/webui/resources/js/cr.js']);
GEN_INCLUDE(['fake_chrome_event.js']);
GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);
GEN_INCLUDE(['fake_settings_private.js']);

SelectToSpeakEnhancedNetworkTtsVoicesTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
    this.confirmationDialogShowCount_ = 0;
    this.confirmationDialogResponse_ = true;

    chrome.accessibilityPrivate.showConfirmationDialog =
        (title, description, callback) => {
          this.confirmationDialogShowCount_ += 1;
          callback(this.confirmationDialogResponse_);
        };

    this.enhancedNetworkVoicesPolicyKey_ =
        'settings.a11y.enhanced_network_voices_in_select_to_speak_allowed';
    this.mockSettingsPrivate_ = new settings.FakeSettingsPrivate([
      {type: 'number', key: 'settings.tts.speech_rate', value: 1.0},
      {type: 'number', key: 'settings.tts.speech_pitch', value: 1.0},
      {type: 'boolean', key: this.enhancedNetworkVoicesPolicyKey_, value: true}
    ]);
    this.mockSettingsPrivate_.allowSetPref();
    chrome.settingsPrivate = this.mockSettingsPrivate_;

    chrome.i18n = {
      getMessage(msgid) {
        return msgid;
      }
    };
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule(
        'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
    await importModule(
        'SelectToSpeakConstants',
        '/select_to_speak/select_to_speak_constants.js');
    await importModule('PrefsManager', '/select_to_speak/prefs_manager.js');

    selectToSpeak.prefsManager_.initPreferences();
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kEnhancedNetworkVoices']};
  }

  // Sets the policy to allow or disallow the network voices.
  setEnhancedNetworkVoicesPolicy(allowed) {
    const unused = () => {};
    this.mockSettingsPrivate_.setPref(
        this.enhancedNetworkVoicesPolicyKey_, allowed, '', unused);
  }
};

TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'EnablesVoicesIfConfirmedInDialog', async function() {
      this.confirmationDialogResponse_ = true;

      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
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
        screenY: textNode.location.top + 1
      };
      this.triggerReadMouseSelectedText(event, event);
    });

TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesVoicesIfCanceledInDialog', async function() {
      this.confirmationDialogResponse_ = false;
      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
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
        screenY: textNode.location.top + 1
      };
      this.triggerReadMouseSelectedText(event, event);
    });

TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesVoicesIfDisallowedByPolicy', async function() {
      this.confirmationDialogResponse_ = true;

      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
      this.mockTts.setOnSpeechCallbacks([this.newCallback(function(utterance) {
        // Network voices are enabled initially because of the
        // confirmation.
        assertEquals(this.confirmationDialogShowCount_, 1);
        assertTrue(selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
        assertTrue(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());

        // Sets the policy to disallow network voices.
        this.setEnhancedNetworkVoicesPolicy(/* allowed= */ false);
        assertFalse(selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
      })]);
      const textNode = this.findTextNode(root, 'This is some text');
      const event = {
        screenX: textNode.location.left + 1,
        screenY: textNode.location.top + 1
      };
      this.triggerReadMouseSelectedText(event, event);
    });

TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesDialogIfDisallowedByPolicy', async function() {
      this.setEnhancedNetworkVoicesPolicy(/* allowed= */ false);

      const root = await this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
          '<p>This is some text</p>');
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
        screenY: textNode.location.top + 1
      };
      this.triggerReadMouseSelectedText(event, event);
    });
