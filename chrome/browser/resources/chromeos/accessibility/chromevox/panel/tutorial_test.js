// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['panel_test_base.js']);
GEN_INCLUDE(['../testing/mock_feedback.js']);

/**
 * Test fixture for the interactive tutorial.
 */
ChromeVoxTutorialTest = class extends ChromeVoxPanelTestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    globalThis.Gesture = chrome.accessibilityPrivate.Gesture;
  }

  assertActiveLessonIndex(expectedIndex) {
    assertEquals(expectedIndex, this.getTutorial().activeLessonIndex);
  }

  assertActiveScreen(expectedScreen) {
    assertEquals(expectedScreen, this.getTutorial().activeScreen);
  }

  async launchAndWaitForTutorial() {
    new PanelCommand(PanelCommandType.TUTORIAL).send();
    await this.waitForTutorial();
  }

  /** Waits for the tutorial to load. */
  async waitForTutorial() {
    return new Promise(resolve => {
      const doc = this.getPanelWindow().document;
      if (doc.getElementById('chromevox-tutorial-container')) {
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
                if (node.id === 'chromevox-tutorial-container') {
                  // Once the tutorial has been added to the document, we need
                  // to wait for the lesson templates to load.
                  const panel = this.getPanel();
                  if (panel.instance.tutorialReadyForTesting_) {
                    resolve();
                  } else {
                    panel.instance.tutorial_.addEventListener(
                        'readyfortesting', () => resolve());
                  }
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

  getTutorial() {
    return this.getPanel().instance.tutorial_;
  }

  disableRestartNudges() {
    this.getPanel().instance.tutorial_.restartNudges = null;
  }

  get simpleDoc() {
    return `
      <p>Some web content</p>
    `;
  }
};

// TODO(crbug.com/40941587): Flaky on ChromeOS.
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_BasicTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  mockFeedback
      .expectSpeech(
          'ChromeVox tutorial', 'Heading 1',
          'Press Search + Right Arrow, or Search + Left Arrow to browse' +
              ' topics')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Essential keys', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Navigation', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Command references', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Sounds and settings', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Resources', 'Link')
      .call(doCmd('nextObject'))
      .expectSpeech('Exit tutorial', 'Button');
  await mockFeedback.replay();
});

// Tests that different lessons are shown when choosing an experience from the
// main menu.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_LessonSetTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech(/Quick Orientation Tutorial, [0-9]+ Lessons/)
      .expectSpeech(
          'Press Search + Right Arrow, or Search + Left Arrow to browse ' +
          'lessons for this topic')
      .call(doCmd('nextObject'))
      .expectSpeech('Welcome to ChromeVox!')
      .call(() => {
        // Call from the tutorial directly, instead of navigating to and
        // clicking on the main menu button.
        tutorial.showMainMenu_();
      })
      .expectSpeech('ChromeVox tutorial')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation')
      .call(doCmd('nextObject'))
      .expectSpeech('Essential keys', 'Link')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
      .call(doCmd('nextObject'))
      .expectSpeech('On, Off, and Stop');
  await mockFeedback.replay();
});

// Tests that a static lesson does not show the 'Practice area' button.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_NoPracticeAreaTest', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(doCmd('nextObject'))
          .expectSpeech('Quick orientation')
          .call(doCmd('nextObject'))
          .expectSpeech('Essential keys')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
          .call(() => {
            tutorial.showLesson_(0);
          })
          .expectSpeech(
              'On, Off, and Stop', 'Heading 1',
              'Press Search + Right Arrow, or Search + Left Arrow to ' +
                  'navigate this lesson')
          .call(doCmd('nextButton'))
          .expectSpeech('Next lesson');
      await mockFeedback.replay();
    });

// Tests that an interactive lesson shows the 'Practice area' button.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_HasPracticeAreaTest', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(doCmd('nextObject'))
          .expectSpeech('Quick orientation')
          .call(doCmd('nextObject'))
          .expectSpeech('Essential keys')
          .call(doCmd('nextObject'))
          .expectSpeech('Navigation')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Navigation Tutorial, [0-9]+ Lessons/)
          .call(() => {
            tutorial.showLesson_(1);
          })
          .expectSpeech('Jump Commands', 'Heading 1')
          .call(doCmd('nextButton'))
          .expectSpeech('Practice area');
      await mockFeedback.replay();
    });

// Tests nudges given in the general tutorial context.
// The first three nudges should read the current item with full context.
// Afterward, general hints will be given about using ChromeVox. Lastly,
// we will give a hint for exiting the tutorial.
AX_TEST_F('ChromeVoxTutorialTest', 'GeneralNudgesTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  this.disableRestartNudges();
  const tutorial = this.getTutorial();
  const giveNudge = () => {
    tutorial.giveNudge();
  };
  mockFeedback.expectSpeech('ChromeVox tutorial');
  for (let i = 0; i < 3; ++i) {
    mockFeedback.call(giveNudge).expectSpeech(
        'ChromeVox tutorial', 'Heading 1');
  }
  mockFeedback.call(giveNudge)
      .expectSpeech('Hint: Hold Search and press the arrow keys to navigate.')
      .call(giveNudge)
      .expectSpeech('Hint: Press Search + Space to activate the current item.')
      .call(giveNudge)
      .expectSpeech(
          'Hint: Press Escape if you would like to exit this tutorial.');
  await mockFeedback.replay();
});

// Tests nudges given in the practice area context. Note, each practice area
// can have different nudge messages; this test confirms that nudges given in
// the practice area differ from those given in the general tutorial context.
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_PracticeAreaNudgesTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      const giveNudge = () => {
        tutorial.giveNudge();
      };
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(doCmd('nextObject'))
          .expectSpeech('Quick orientation')
          .call(doCmd('nextObject'))
          .expectSpeech('Essential keys')
          .call(doCmd('nextObject'))
          .expectSpeech('Navigation')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Navigation Tutorial, [0-9]+ Lessons/)
          .call(() => {
            tutorial.showLesson_(0);
          })
          .expectSpeech('Basic Navigation', 'Heading 1')
          .call(doCmd('nextButton'))
          .expectSpeech('Practice area')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Try using basic navigation to navigate/)
          .call(giveNudge)
          .expectSpeech(
              'Try pressing Search + left/right arrow. The search key is ' +
              'directly above the shift key')
          .call(giveNudge)
          .expectSpeech('Press Search + Space to activate the current item.');
      await mockFeedback.replay();
    });

// Tests that the tutorial closes when the 'Exit tutorial' button is clicked.
// TODO(crbug.com/1332510): Failing on ChromeOS.
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_ExitButtonTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doCmd('previousButton'))
      .expectSpeech('Exit tutorial')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech('Some web content');
  await mockFeedback.replay();
});

// Tests that the tutorial closes when Escape is pressed.
// TODO(crbug.com/1332510): Failing on ChromeOS.
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_EscapeTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(() => {
        // Press Escape.
        tutorial.onKeyDown({
          key: 'Escape',
          preventDefault: () => {},
          stopPropagation: () => {},
        });
      })
      .expectSpeech('Some web content');
  await mockFeedback.replay();
});

// Tests that the main menu button navigates the user to the main menu screen.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_MainMenuButton', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
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
      .expectSpeech('ChromeVox tutorial')
      .call(this.assertActiveScreen.bind(this, 'main_menu'));
  await mockFeedback.replay();
});

// Tests that the all lessons button navigates the user to the lesson menu
// screen.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_AllLessonsButton', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(this.assertActiveScreen.bind(this, 'main_menu'))
          .call(doCmd('nextObject'))
          .expectSpeech('Quick orientation')
          .call(doCmd('nextObject'))
          .expectSpeech('Essential keys')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
          .call(this.assertActiveScreen.bind(this, 'lesson_menu'))
          .call(doCmd('nextObject'))
          .expectSpeech('On, Off, and Stop')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech('On, Off, and Stop', 'Heading 1')
          .call(this.assertActiveScreen.bind(this, 'lesson'))
          .call(doCmd('nextButton'))
          .expectSpeech('Next lesson')
          .call(doCmd('nextButton'))
          .expectSpeech('All lessons')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
          .call(this.assertActiveScreen.bind(this, 'lesson_menu'));
      await mockFeedback.replay();
    });

// Tests that the next and previous lesson buttons navigate properly.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_NextPreviousButtons', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(() => {
            tutorial.curriculum = 'essential_keys';
            tutorial.showLesson_(0);
            this.assertActiveLessonIndex(0);
            this.assertActiveScreen('lesson');
          })
          .expectSpeech('On, Off, and Stop', 'Heading 1')
          .call(doCmd('nextButton'))
          .expectSpeech('Next lesson')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech('The ChromeVox modifier key', 'Heading 1')
          .call(this.assertActiveLessonIndex.bind(this, 1))
          .call(doCmd('nextButton'))
          .expectSpeech('Previous lesson')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech('On, Off, and Stop', 'Heading 1')
          .call(this.assertActiveLessonIndex.bind(this, 0));
      await mockFeedback.replay();
    });

// Tests that the title of an interactive lesson is read when shown.
AX_TEST_F('ChromeVoxTutorialTest', 'AutoReadTitle', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech(/Quick Orientation Tutorial, [0-9]+ Lessons/)
      .call(() => {
        tutorial.showFirstLesson_();
      })
      .expectSpeech('Welcome to ChromeVox!')
      .expectSpeech(
          'Welcome to the ChromeVox tutorial. To exit this tutorial at any ' +
          'time, press the Escape key on the top left corner of the ' +
          'keyboard. To turn off ChromeVox, hold Control and Alt, and ' +
          `press Z. When you're ready, use the spacebar to move to the ` +
          'next lesson.');
  await mockFeedback.replay();
});

// Tests that we read a hint for navigating a lesson when it is shown.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_LessonHint', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation')
      .call(doCmd('nextObject'))
      .expectSpeech('Essential keys')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech(/Essential Keys Tutorial, [0-9]+ Lessons/)
      .call(() => {
        tutorial.showLesson_(0);
      })
      .expectSpeech('On, Off, and Stop', 'Heading 1')
      .expectSpeech(
          'Press Search + Right Arrow, or Search + Left Arrow to navigate' +
          ' this lesson');
  await mockFeedback.replay();
});

// Tests for correct speech and earcons on the earcons lesson.
AX_TEST_F('ChromeVoxTutorialTest', 'EarconLesson', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  const nextObjectAndExpectSpeechAndEarcon = (speech, earcon) => {
    mockFeedback.call(doCmd('nextObject'))
        .expectSpeech(speech)
        .expectEarcon(earcon);
  };
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(() => {
        // Show the lesson.
        tutorial.curriculum = 'sounds_and_settings';
        tutorial.showLesson_(0);
      })
      .expectSpeech('Sounds')
      .call(doCmd('nextObject'))
      .expectSpeech(new RegExp(
          'ChromeVox uses sounds to give you essential and additional ' +
          'information.'));
  nextObjectAndExpectSpeechAndEarcon('A modal alert', EarconId.ALERT_MODAL);
  nextObjectAndExpectSpeechAndEarcon(
      'A non modal alert', EarconId.ALERT_NONMODAL);
  // TODO(anastasi): Identify why the button is not present in the tutorial.
  // nextObjectAndExpectSpeechAndEarcon('A button', EarconId.BUTTON);
  await mockFeedback.replay();
});

// Tests that a lesson from the quick orientation blocks ChromeVox execution
// until the specified keystroke is pressed.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_QuickOrientationLessonTest',
    async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      const keyboardHandler = ChromeVoxState.instance.keyboardHandler_;

      // Helper functions. For this test, activate commands by hooking into
      // the BackgroundKeyboardHandler. This is necessary because
      // ForcedActionPath intercepts key sequences before they are routed to
      // CommandHandler.
      const getRangeStartNode = () => ChromeVoxRange.current.start.node;

      const simulateKeyPress = (keyCode, opt_modifiers) => {
        const keyEvent = TestUtils.createMockKeyEvent(keyCode, opt_modifiers);
        keyboardHandler.onKeyDown(keyEvent);
        keyboardHandler.onKeyUp(keyEvent);
      };

      let firstLessonNode;
      await mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(doCmd('nextObject'))
          .expectSpeech('Quick orientation')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Quick Orientation Tutorial, [0-9]+ Lessons/)
          .call(doCmd('nextObject'))
          .expectSpeech('Welcome to ChromeVox!')
          .call(doCmd('forceClickOnCurrentItem'))
          .expectSpeech(/Welcome to the ChromeVox tutorial./)
          .call(() => {
            assertEquals(0, tutorial.activeLessonId);
            firstLessonNode = getRangeStartNode();
          })
          .call(
              simulateKeyPress.bind(this, KeyCode.RIGHT, {searchKeyHeld: true}))
          .call(() => {
            assertEquals(firstLessonNode, getRangeStartNode());
            assertEquals(0, tutorial.activeLessonId);
          })
          .call(
              simulateKeyPress.bind(this, KeyCode.LEFT, {searchKeyHeld: true}))
          .call(() => {
            assertEquals(firstLessonNode, getRangeStartNode());
            assertEquals(0, tutorial.activeLessonId);
          })
          // Pressing space, which is the desired key sequence, should move us
          // to the next lesson.
          .call(simulateKeyPress.bind(this, KeyCode.SPACE, {}))
          .expectSpeech('Essential Keys: Control')
          .expectSpeech(/Let's start with a few keys you'll use regularly./)
          .call(() => {
            assertEquals(1, tutorial.activeLessonId);
            assertNotEquals(firstLessonNode, getRangeStartNode());
          })
          // Pressing control, which is the desired key sequence, should move
          // us to the next lesson.
          .call(simulateKeyPress.bind(this, KeyCode.CONTROL, {}))
          .expectSpeech('Essential Keys: Shift')
          .call(() => {
            assertEquals(2, tutorial.activeLessonId);
          });
      await mockFeedback.replay();
    });

// Tests that tutorial nudges are restarted whenever the current range changes.
AX_TEST_F('ChromeVoxTutorialTest', 'RestartNudges', async function() {
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  // Swap in below function to track when nudges get restarted.
  const reset = () => new Promise(resolve => tutorial.restartNudges = resolve);

  let nudgesHaveRestarted = reset();
  CommandHandlerInterface.instance.onCommand('nextObject');
  await nudgesHaveRestarted;

  // Show a lesson.
  tutorial.curriculum = 'essential_keys';
  tutorial.showLesson_(0);
  nudgesHaveRestarted = reset();
  CommandHandlerInterface.instance.onCommand('nextObject');
  await nudgesHaveRestarted;

  nudgesHaveRestarted = reset();
  CommandHandlerInterface.instance.onCommand('nextObject');
  await nudgesHaveRestarted;
});

// Tests that the tutorial closes and ChromeVox navigates to a resource link.
//
// Flaky. See crbug.com/336702956.
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_ResourcesTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(() => {
        tutorial.curriculum = 'resources';
        tutorial.showLesson_(0);
      })
      .expectSpeech('Learn More')
      .call(doCmd('nextObject'))
      .expectSpeech(/Congratulations/)
      .call(doCmd('nextObject'))
      .expectSpeech('ChromeVox Command Reference', 'Link')
      .call(doCmd('forceClickOnCurrentItem'))
      // Expect the support page to be pulled up; it is read differently
      // depending on if this browsertest's browser has network access.
      .expectSpeech(/(support.google.com)|(Chromebook Help)/);
  await mockFeedback.replay();
});

// Tests that choosing a curriculum with only 1 lesson automatically opens the
// lesson.
AX_TEST_F('ChromeVoxTutorialTest', 'OnlyLessonTest', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doCmd('nextObject'))
      .expectSpeech('Quick orientation')
      .call(doCmd('nextObject'))
      .expectSpeech('Essential keys')
      .call(doCmd('nextObject'))
      .expectSpeech('Navigation')
      .call(doCmd('nextObject'))
      .expectSpeech('Command references')
      .call(doCmd('nextObject'))
      .expectSpeech('Sounds and settings')
      .call(doCmd('nextObject'))
      .expectSpeech('Resources')
      .call(doCmd('forceClickOnCurrentItem'))
      .expectSpeech('Learn More', 'Heading 1')
      .expectSpeech(
          'Press Search + Right Arrow, or Search + Left Arrow to' +
          ' navigate this lesson')
      // The 'All lessons' button should be hidden since this is the only
      // lesson for the curriculum.
      .call(doCmd('nextButton'))
      .expectSpeech('Main menu')
      .call(doCmd('nextButton'))
      .expectSpeech('Exit tutorial');
  await mockFeedback.replay();
});

// Tests that interactive mode and ForcedActionPath are properly set when
// showing different screens in the tutorial.
AX_TEST_F(
    'ChromeVoxTutorialTest', 'StartStopInteractiveMode', async function() {
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      let userActionMonitorCreatedCount = 0;
      let userActionMonitorDestroyedCount = 0;
      let isForcedActionPathActive = false;
      // Expose the correct BackgroundBridge so we can override the functions
      this.getPanel().exportBackgroundBridgeForTesting();
      // Swap in functions below so we can track the number of times
      // ForcedActionPath is created and destroyed.
      this.getPanelWindow().BackgroundBridge.ForcedActionPath.listenFor =
          () => {
            userActionMonitorCreatedCount += 1;
            isForcedActionPathActive = true;
          };
      this.getPanelWindow().BackgroundBridge.ForcedActionPath.stopListening =
          () => {
            userActionMonitorDestroyedCount += 1;
            isForcedActionPathActive = false;
          };

      // A helper to make assertions on four variables of interest.
      const makeAssertions = expectedVars => {
        assertEquals(expectedVars.createdCount, userActionMonitorCreatedCount);
        assertEquals(
            expectedVars.destroyedCount, userActionMonitorDestroyedCount);
        assertEquals(expectedVars.interactiveMode, tutorial.interactiveMode_);
        // Note: Interactive mode and ForcedActionPath should always be in
        // sync in the context of the tutorial.
        assertEquals(expectedVars.interactiveMode, isForcedActionPathActive);
      };

      makeAssertions(
          {createdCount: 0, destroyedCount: 0, interactiveMode: false});
      // Show the first lesson of the quick orientation, which is interactive.
      tutorial.curriculum = 'quick_orientation';
      tutorial.showLesson_(0);
      makeAssertions(
          {createdCount: 1, destroyedCount: 0, interactiveMode: true});

      // Move to the next lesson in the quick orientation. This lesson is also
      // interactive, so ForcedActionPath should be destroyed and re-created.
      tutorial.showNextLesson();
      makeAssertions(
          {createdCount: 2, destroyedCount: 1, interactiveMode: true});

      // Leave the quick orientation by navigating to the lesson menu. This
      // should stop interactive mode and destroy ForcedActionPath.
      tutorial.showLessonMenu_();
      makeAssertions(
          {createdCount: 2, destroyedCount: 2, interactiveMode: false});
    });

// Tests that gestures can be used in the tutorial to navigate.
// TODO(crbug.com/1332510): Failing on ChromeOS.
AX_TEST_F('ChromeVoxTutorialTest', 'DISABLED_Gestures', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  const tutorial = this.getTutorial();
  mockFeedback.expectSpeech('ChromeVox tutorial')
      .call(doGesture(Gesture.SWIPE_RIGHT1))
      .expectSpeech('Quick orientation', 'Link')
      .call(doGesture(Gesture.SWIPE_RIGHT1))
      .expectSpeech('Essential keys', 'Link')
      .call(doGesture(Gesture.SWIPE_LEFT1))
      .expectSpeech('Quick orientation', 'Link')
      .call(doGesture(Gesture.SWIPE_LEFT2))
      .expectSpeech('Some web content');
  await mockFeedback.replay();
});

// Tests that touch orientation loads properly. Tests string content, but does
// not test interactivity of lessons.
// TODO(crbug.com/1193799): fix ax node errors causing console spew and
// breaking tests
AX_TEST_F(
    'ChromeVoxTutorialTest', 'DISABLED_TouchOrientation', async function() {
      const mockFeedback = this.createMockFeedback();
      const root = await this.runWithLoadedTree(this.simpleDoc);
      await this.launchAndWaitForTutorial();
      const tutorial = this.getTutorial();
      mockFeedback.expectSpeech('ChromeVox tutorial')
          .call(() => {
            tutorial.curriculum = 'touch_orientation';
            tutorial.medium = 'touch';
            tutorial.showLesson_(0);
            this.assertActiveLessonIndex(0);
            this.assertActiveScreen('lesson');
          })
          .expectSpeech('ChromeVox touch tutorial')
          .expectSpeech(/Welcome to the ChromeVox tutorial/)
          .call(doGesture(Gesture.CLICK))
          .expectSpeech('Activate an item')
          .expectSpeech(/To continue, double-tap now/)
          .call(doGesture(Gesture.CLICK))
          .expectSpeech('Move to the next or previous item')
          .call(() => {
            // Jump to the penultimate lesson.
            tutorial.showLesson_(6);
          })
          .expectSpeech('Move to the next or previous section')
          .expectSpeech(/swipe from left to right with four fingers/)
          .call(doGesture(Gesture.SWIPE_RIGHT4))
          .expectSpeech(/swiping with four fingers from right to left/)
          .call(doGesture(Gesture.SWIPE_LEFT4))
          .expectSpeech('Touch tutorial complete');
      await mockFeedback.replay();
    });

AX_TEST_F('ChromeVoxTutorialTest', 'GeneralTouchNudges', async function() {
  const mockFeedback = this.createMockFeedback();
  const root = await this.runWithLoadedTree(this.simpleDoc);
  await this.launchAndWaitForTutorial();
  this.disableRestartNudges();
  const tutorial = this.getTutorial();
  const giveNudge = () => {
    tutorial.giveNudge();
  };
  mockFeedback.expectSpeech('ChromeVox tutorial');
  mockFeedback.call(() => {
    tutorial.medium = 'touch';
    tutorial.initializeNudges('general');
  });
  for (let i = 0; i < 3; ++i) {
    mockFeedback.call(giveNudge).expectSpeech(
        'ChromeVox tutorial', 'Heading 1');
  }
  mockFeedback.call(giveNudge)
      .expectSpeech('Hint: Swipe left or right with one finger to navigate.')
      .call(giveNudge)
      .expectSpeech(
          'Hint: Double-tap with one finger to activate the current item.')
      .call(giveNudge)
      .expectSpeech(
          'Hint: Swipe from right to left with two fingers if you would ' +
          'like to exit this tutorial.');
  await mockFeedback.replay();
});
