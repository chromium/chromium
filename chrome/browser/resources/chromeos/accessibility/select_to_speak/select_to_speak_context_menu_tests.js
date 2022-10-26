// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_tts.js']);

/**
 * Browser tests for select-to-speak's feature to speak text
 * using the context menu.
 */
SelectToSpeakContextMenuTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockTts = new MockTts();
    chrome.tts = this.mockTts;
  }

  /** @override */
  get featureList() {
    return {
      enabled: ['features::kAccessibilitySelectToSpeakContextMenuOption'],
    };
  }


  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('EventGenerator', '/common/event_generator.js');
    await importModule(
        'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
    await importModule(
        'SelectToSpeakConstants',
        '/select_to_speak/select_to_speak_constants.js');
    selectToSpeak.prefsManager_.enhancedVoicesDialogShown_ = true;
  }
};

AX_TEST_F(
    'SelectToSpeakContextMenuTest', 'SimpleSpeakingTest', async function() {
      const root = await this.runWithLoadedTree('<p>This is some text</p>');
      const textNode = this.findTextNode(root, 'This is some text');
      chrome.automation.setDocumentSelection({
        anchorObject: textNode,
        anchorOffset: 0,
        focusObject: textNode,
        focusOffset: 17,
      });
      const menuName = chrome.i18n.getMessage(
          'select_to_speak_listen_context_menu_option_text');
      const listener = (change) => {
        if (change.type === 'nodeCreated' && change.target.name === menuName) {
          chrome.automation.removeTreeChangeObserver(listener);
          change.target.doDefault();
        }
      };
      this.mockTts.setOnSpeechCallbacks([this.newCallback((utterance) => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(utterance, 'This is some text');
        this.mockTts.finishPendingUtterance();
      })]);
      chrome.automation.addTreeChangeObserver('allTreeChanges', listener);
      textNode.showContextMenu();
    });
