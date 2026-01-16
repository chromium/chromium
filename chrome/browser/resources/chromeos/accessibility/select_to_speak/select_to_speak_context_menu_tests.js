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
  async setUpDeferred() {
    await super.setUpDeferred();
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
        // On Lacros, the context menu might be changed before we can click on
        // it, whereas on Ash we can usually click on it first. Easiest to
        // look for just node changed rather than created.
        if (change.type === chrome.automation.TreeChangeType.NODE_CHANGED &&
            change.target.name === menuName) {
          change.target.doDefault();
        }
      };
      this.mockTts.setOnSpeechCallbacks([this.newCallback((utterance) => {
        assertTrue(this.mockTts.currentlySpeaking());
        assertEquals(this.mockTts.pendingUtterances().length, 1);
        this.assertEqualsCollapseWhitespace(utterance, 'This is some text');
        this.mockTts.finishPendingUtterance();
        chrome.automation.removeTreeChangeObserver(listener);
      })]);
      chrome.automation.addTreeChangeObserver('allTreeChanges', listener);
      textNode.showContextMenu();
    });
