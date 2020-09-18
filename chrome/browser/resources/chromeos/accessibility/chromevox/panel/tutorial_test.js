// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_next_e2e_test_base.js']);
GEN_INCLUDE(['../testing/mock_feedback.js']);

/**
 * Test fixture for the interactive tutorial.
 */
ChromeVoxTutorialTest = class extends ChromeVoxNextE2ETest {
  /** @override */
  testGenCppIncludes() {
    super.testGenCppIncludes();
    GEN(`
  #include "base/command_line.h"
  #include "ui/accessibility/accessibility_switches.h"
      `);
  }

  /** @override */
  testGenPreamble() {
    GEN(`
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ::switches::kEnableExperimentalAccessibilityChromeVoxTutorial);
      `);
    super.testGenPreamble();
  }

  /** @override */
  setUp() {
    window.doCmd = this.doCmd;
  }

  getPanelWindow() {
    let panelWindow = null;
    while (!panelWindow) {
      panelWindow = chrome.extension.getViews().find(function(view) {
        return view.location.href.indexOf('chromevox/panel/panel.html') > 0;
      });
    }
    return panelWindow;
  }

  getPanel() {
    return this.getPanelWindow().Panel;
  }

  /**
   * Waits for the interactive tutorial to load.
   */
  async waitForTutorial() {
    return new Promise(resolve => {
      const doc = this.getPanelWindow().document;
      if (doc.getElementById('i-tutorial-container')) {
        resolve();
      } else {
        /**
         * @param {Array<MutationRecord>} mutationsList
         * @param {MutationObserver} observer
         */
        const onMutation = (mutationsList, observer) => {
          for (const mutation of mutationsList) {
            if (mutation.type === 'childList') {
              for (const node of mutation.addedNodes) {
                if (node.id === 'i-tutorial-container') {
                  // Resolve once the tutorial has been added to the document.
                  resolve();
                  observer.disconnect();
                }
              }
            }
          }
        };

        const observer = new MutationObserver(onMutation);
        observer.observe(
            doc.body /* target */, {childList: true} /* options */);
      }
    });
  }

  get simpleDoc() {
    return `
      <p>Simple</p>
    `;
  }
};

TEST_F('ChromeVoxTutorialTest', 'BasicTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    const Panel = this.getPanel();
    assertTrue(Panel.iTutorialEnabled_);
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('New user', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Experienced user', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Developer', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Exit tutorial', 'Button')
        .replay();
  });
});

// Tests that different lessons are shown when choosing an experience from the
// main menu.
TEST_F('ChromeVoxTutorialTest', 'LessonSetTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    const Panel = this.getPanel();
    assertTrue(Panel.iTutorialEnabled_);
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
    const tutorial = Panel.iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('New user', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('New User Tutorial, 8 Lessons')
        .call(doCmd('nextObject'))
        .expectSpeech('On, Off, and Stop')
        .call(() => {
          // Call from the tutorial directly, instead of navigating to and
          // clicking on the main menu button.
          tutorial.showMainMenu();
        })
        .expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('New user', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Experienced user', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Experienced User Tutorial, 2 Lessons')
        .call(doCmd('nextObject'))
        .expectSpeech('Text fields')
        .replay();
  });
});

// Tests that a static lesson does not show the 'Practice area' button.
TEST_F('ChromeVoxTutorialTest', 'NoPracticeAreaTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    const Panel = this.getPanel();
    assertTrue(Panel.iTutorialEnabled_);
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
    const tutorial = Panel.iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('New user', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('New User Tutorial, 8 Lessons')
        .call(() => {
          tutorial.showLesson(0);
        })
        .expectSpeech('On, Off, and Stop', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Next lesson')
        .replay();
  });
});

// Tests that an interactive lesson shows the 'Practice area' button.
TEST_F('ChromeVoxTutorialTest', 'HasPracticeAreaTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    const Panel = this.getPanel();
    assertTrue(Panel.iTutorialEnabled_);
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
    const tutorial = Panel.iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('New user', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('New User Tutorial, 8 Lessons')
        .call(() => {
          tutorial.showLesson(2);
        })
        .expectSpeech('Basic Navigation', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Practice Area')
        .replay();
  });
});