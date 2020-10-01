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

  assertActiveLessonIndex(expectedIndex) {
    assertEquals(expectedIndex, this.getPanel().iTutorial.activeLessonIndex);
  }

  assertActiveScreen(expectedScreen) {
    assertEquals(expectedScreen, this.getPanel().iTutorial.activeScreen);
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

  async launchAndWaitForTutorial() {
    assertTrue(this.getPanel().iTutorialEnabled_);
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
    return new Promise(resolve => {
      resolve();
    });
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
      <p>Some web content</p>
    `;
  }
};

TEST_F('ChromeVoxTutorialTest', 'BasicTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Navigation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Command references', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Sounds and settings', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Resources', 'Button')
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
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Quick Orientation Tutorial, [0-9]+ Lessons/)
        .call(doCmd('nextObject'))
        .expectSpeech('Welcome to ChromeVox!')
        .call(() => {
          // Call from the tutorial directly, instead of navigating to and
          // clicking on the main menu button.
          tutorial.showMainMenu();
        })
        .expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
        .call(doCmd('nextObject'))
        .expectSpeech('On, Off, and Stop')
        .replay();
  });
});

// Tests that a static lesson does not show the 'Practice area' button.
TEST_F('ChromeVoxTutorialTest', 'NoPracticeAreaTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
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
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Navigation', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Navigation Tutorial, [0-9]+ Lessons/)
        .call(() => {
          tutorial.showLesson(0);
        })
        .expectSpeech('Basic Navigation', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Practice Area')
        .replay();
  });
});

// Tests nudges given in the general tutorial context.
// The first three nudges should read the current item with full context.
// Afterward, general hints will be given about using ChromeVox. Lastly,
// we will give a hint for exiting the tutorial.
TEST_F('ChromeVoxTutorialTest', 'GeneralNudgesTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    const giveNudge = () => {
      tutorial.giveNudge();
    };
    mockFeedback.expectSpeech('Choose your tutorial experience');
    for (let i = 0; i < 3; ++i) {
      mockFeedback.call(giveNudge).expectSpeech(
          'Choose your tutorial experience', 'Heading 1');
    }
    mockFeedback.call(giveNudge)
        .expectSpeech('Hint: Hold Search and press the arrow keys to navigate.')
        .call(giveNudge)
        .expectSpeech(
            'Hint: Press Search + Space to activate the current item.')
        .call(giveNudge)
        .expectSpeech(
            'Hint: Press Escape if you would like to exit this tutorial.')
        .replay();
  });
});

// Tests nudges given in the practice area context. Note, each practice area
// can have different nudge messages; this test confirms that nudges given in
// the practice area differ from those given in the general tutorial context.
TEST_F('ChromeVoxTutorialTest', 'PracticeAreaNudgesTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    const giveNudge = () => {
      tutorial.giveNudge();
    };
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Navigation', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Navigation Tutorial, [0-9]+ Lessons/)
        .call(() => {
          tutorial.showLesson(0);
        })
        .expectSpeech('Basic Navigation', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Practice Area')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Basic Navigation Practice')
        .call(giveNudge)
        .expectSpeech(
            'Try pressing Search + left/right arrow. The search key is ' +
            'directly above the shift key')
        .call(giveNudge)
        .expectSpeech('Press Search + Space to activate the current item.')
        .replay();
  });
});

// Tests that the tutorial closes when the 'Exit tutorial' button is clicked.
TEST_F('ChromeVoxTutorialTest', 'ExitButtonTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('previousButton'))
        .expectSpeech('Exit tutorial')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Some web content')
        .replay();
  });
});

// Tests that the tutorial closes when Escape is pressed.
TEST_F('ChromeVoxTutorialTest', 'EscapeTest', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(() => {
          // Press Escape.
          tutorial.onKeyDown({
            key: 'Escape',
            preventDefault: () => {},
            stopPropagation: () => {}
          });
        })
        .expectSpeech('Some web content')
        .replay();
  });
});

// Tests that the main menu button navigates the user to the main menu screen.
TEST_F('ChromeVoxTutorialTest', 'MainMenuButton', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(this.assertActiveScreen.bind(this, 'main_menu'))
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
        .call(this.assertActiveScreen.bind(this, 'lesson_menu'))
        .call(doCmd('previousButton'))
        .expectSpeech('Exit tutorial')
        .call(doCmd('previousButton'))
        .expectSpeech('Main menu')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Choose your tutorial experience')
        .call(this.assertActiveScreen.bind(this, 'main_menu'))
        .replay();
  });
});

// Tests that the all lessons button navigates the user to the lesson menu
// screen.
TEST_F('ChromeVoxTutorialTest', 'AllLessonsButton', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(this.assertActiveScreen.bind(this, 'main_menu'))
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
        .call(this.assertActiveScreen.bind(this, 'lesson_menu'))
        .call(doCmd('nextObject'))
        .expectSpeech('On, Off, and Stop', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .call(this.assertActiveScreen.bind(this, 'lesson'))
        .expectSpeech('On, Off, and Stop', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Next lesson')
        .call(doCmd('nextButton'))
        .expectSpeech('All lessons')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
        .call(this.assertActiveScreen.bind(this, 'lesson_menu'))
        .replay();
  });
});

// Tests that the next and previous lesson buttons navigate properly.
TEST_F('ChromeVoxTutorialTest', 'NextPreviousButtons', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(() => {
          tutorial.curriculum = 'essential_keys';
          tutorial.showLesson(0);
          this.assertActiveLessonIndex(0);
          this.assertActiveScreen('lesson');
        })
        .expectSpeech('On, Off, and Stop', 'Heading 1')
        .call(doCmd('nextButton'))
        .expectSpeech('Next lesson')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('The ChromeVox Modifier Key', 'Heading 1')
        .call(this.assertActiveLessonIndex.bind(this, 1))
        .call(doCmd('nextButton'))
        .expectSpeech('Previous lesson')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('On, Off, and Stop', 'Heading 1')
        .call(this.assertActiveLessonIndex.bind(this, 0))
        .replay();
  });
});

// Tests that the title of an interactive lesson is read when shown.
TEST_F('ChromeVoxTutorialTest', 'AutoReadTitle', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Quick Orientation Tutorial, [0-9]+ Lessons/)
        .call(doCmd('nextObject'))
        .expectSpeech('Welcome to ChromeVox!', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech('Welcome to ChromeVox!')
        .expectSpeech(
            'Welcome to the ChromeVox tutorial. To exit this tutorial at any ' +
            'time, press the Escape key on the top left corner of the ' +
            'keyboard. To turn off ChromeVox, hold Control and Alt, and ' +
            `press Z. When you're ready, use the spacebar to move to the ` +
            'next lesson.')
        .replay();
  });
});

// Tests that the content of a non-interactive lesson is read when shown.
TEST_F('ChromeVoxTutorialTest', 'AutoReadLesson', function() {
  const mockFeedback = this.createMockFeedback();
  this.runWithLoadedTree(this.simpleDoc, async function(root) {
    await this.launchAndWaitForTutorial();
    const tutorial = this.getPanel().iTutorial;
    mockFeedback.expectSpeech('Choose your tutorial experience')
        .call(doCmd('nextObject'))
        .expectSpeech('Quick orientation', 'Button')
        .call(doCmd('nextObject'))
        .expectSpeech('Essential keys', 'Button')
        .call(doCmd('forceClickOnCurrentItem'))
        .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
        .call(() => {
          tutorial.showLesson(0);
        })
        .expectSpeech('On, Off, and Stop', 'Heading 1')
        .expectSpeech(
            'To temporarily stop ChromeVox from speaking, ' +
            'press the Control key.')
        .expectSpeech('To turn ChromeVox on or off, use Control+Alt+Z.')
        .replay();
  });
});
