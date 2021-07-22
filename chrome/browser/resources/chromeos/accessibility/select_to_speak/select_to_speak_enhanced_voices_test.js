// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['mock_tts.js']);

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
  }

  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);

    (async () => {
      await importModule(
          'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
      await importModule(
          'SelectToSpeakConstants',
          '/select_to_speak/select_to_speak_constants.js');
      await importModule('PrefsManager', '/select_to_speak/prefs_manager.js');

      runTest();
    })();
  }

  /** @override */
  get featureList() {
    return {enabled: ['features::kEnhancedNetworkVoices']};
  }
};

TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'EnablesVoicesIfConfirmedInDialog', function() {
      this.confirmationDialogResponse_ = true;

      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks([this.newCallback(function(
                utterance) {
              // Speech starts asynchronously.
              assertEquals(this.confirmationDialogShowCount_, 1);
              assertTrue(
                  selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
              assertTrue(
                  selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
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
    });


TEST_F(
    'SelectToSpeakEnhancedNetworkTtsVoicesTest',
    'DisablesVoicesIfCanceledInDialog', function() {
      this.confirmationDialogResponse_ = false;
      this.runWithLoadedTree(
          'data:text/html;charset=utf-8,' +
              '<p>This is some text</p>',
          function(root) {
            assertFalse(this.mockTts.currentlySpeaking());
            assertEquals(this.mockTts.pendingUtterances().length, 0);
            this.mockTts.setOnSpeechCallbacks([this.newCallback(function(
                utterance) {
              // Speech starts asynchronously.
              assertEquals(this.confirmationDialogShowCount_, 1);
              assertTrue(
                  selectToSpeak.prefsManager_.enhancedVoicesDialogShown());
              assertFalse(
                  selectToSpeak.prefsManager_.enhancedNetworkVoicesEnabled());
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
    });
