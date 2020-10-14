// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the ChromeVox
 * interactive tutorial engine.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TutorialCommon} from './tutorial_common.js';
import {TutorialLesson} from './tutorial_lesson.js';

/** @enum {string} */
const Curriculum = {
  NONE: 'none',
  QUICK_ORIENTATION: 'quick_orientation',
  ESSENTIAL_KEYS: 'essential_keys',
  NAVIGATION: 'navigation',
  COMMAND_REFERENCES: 'command_references',
  SOUNDS_AND_SETTINGS: 'sounds_and_settings',
  RESOURCES: 'resources',
};

/**
 * The user’s interaction medium. Influences tutorial content.
 * @enum {string}
 */
const InteractionMedium = {
  KEYBOARD: 'keyboard',
  TOUCH: 'touch',
  BRAILLE: 'braille',
};

/**
 * The various screens within the tutorial.
 * @enum {string}
 */
const Screen = {
  NONE: 'none',
  MAIN_MENU: 'main_menu',
  LESSON_MENU: 'lesson_menu',
  LESSON: 'lesson',
};

/**
 * The types of nudges given by the tutorial.
 * General nudges: announce the current item three times, then give two general
 * hints about how to navigate with ChromeVox, then a final nudge about how to
 * exit the tutorial.
 * Practice area nudges: specified by the |hints| array in lessonData. These
 * are nudges for the practice area and are only given when the practice area
 * is active.
 * @enum {string}
 */
const NudgeType = {
  GENERAL: 'general',
  PRACTICE_AREA: 'practice_area'
};

Polymer({
  is: 'i-tutorial',

  _template: html`{__html_template__}`,

  behaviors: [TutorialCommon],

  properties: {
    curriculum: {
      type: String,
      value: Curriculum.NONE,
      observer: 'updateIncludedLessons'
    },

    medium: {
      type: String,
      value: InteractionMedium.KEYBOARD,
      observer: 'updateIncludedLessons'
    },

    // Bookkeeping variables.

    // Not all lessons are included, some are filtered out based on the chosen
    // medium and curriculum.
    includedLessons: {type: Array},

    // An index into |includedLessons|.
    activeLessonIndex: {type: Number, value: -1},

    activeLessonNum: {type: Number, value: -1},

    numLessons: {type: Number, value: 0},

    numLoadedLessons: {type: Number, value: 0},

    activeScreen: {type: String, observer: 'onActiveScreenChanged'},

    interactiveMode: {type: Boolean, value: false},

    nudgeIntervalId: {type: Number},

    /** @const */
    NUDGE_INTERVAL_TIME_MS: {type: Number, value: 45 * 1000},

    nudgeCounter: {type: Number, value: 0},

    /** @type {Array<function(): void>} */
    nudgeArray: {type: Array},

    isPracticeAreaActive: {type: Boolean, value: false},

    // Labels and text content.

    chooseYourExperience: {
      type: String,
      value: 'Choose your tutorial experience',
    },
    quickOrientation: {type: String, value: 'Quick orientation'},

    essentialKeys: {type: String, value: 'Essential keys'},

    navigation: {type: String, value: 'Navigation'},

    commandReferences: {type: String, value: 'Command references'},

    soundsAndSettings: {type: String, value: 'Sounds and settings'},

    resources: {type: String, value: 'Resources'},

    continue: {type: String, value: 'Continue where I left off'},

    restartQuickOrientation: {type: String, value: 'Restart quick orientation'},

    previousLesson: {type: String, value: 'Previous lesson'},

    nextLesson: {type: String, value: 'Next lesson'},

    mainMenu: {type: String, value: 'Main menu'},

    lessonMenu: {type: String, value: 'All lessons'},

    exitTutorial: {type: String, value: 'Exit tutorial'},

    lessonData: {
      type: Array,
      value: [
        {
          title: 'Welcome to ChromeVox!',
          content: [`Welcome to the ChromeVox tutorial. To exit this tutorial
            at any time, press the Escape key on the top left corner
            of the keyboard. To turn off ChromeVox, hold Control and Alt, and
            press Z. When you're ready, use the spacebar to move to the next
            lesson.`],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [32 /* Space */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'Essential Keys: Control',
          content: [`Let's start with a few keys you'll use regularly: Control,
            Shift, Search, and the Arrow keys. Find the Control key on the
            bottom left corner of your keyboard. To continue, press the Control
            key.`],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [17 /* Ctrl */]}},
            shouldPropagate: false
          }],
          autoInteractive: true,
        },

        {
          title: 'Essential Keys: Shift',
          content: [`The Control key can be used at any time to temporarily stop
            ChromeVox from speaking. Now, find the left Shift key, which is
            directly above the Control key. To continue, press the left Shift
            key.`],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [16 /* Shift */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'Essential Keys: Search',
          content:
              [`We'll talk more about the Shift key in later lessons, as it is
            used in many ChromeVox commands. Next, you’ll learn about the
            Search key. The Search key is used in combination with other keys
            for ChromeVox commands. The Search key is immediately above the
            left Shift key. To continue, press the Search key.`],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {skipStripping: false, keys: {keyCode: [91 /* Search */]}},
            afterActionMsg: 'You found the search key!',
          }],
          autoInteractive: true,
        },

        {
          title: 'Basic navigation',
          content: [
            `Now you’ll learn some basic navigation. You can hold Search and
              press the arrow keys to move around the screen. To continue,
              press Search + Right arrow.`,
            `If you reach an item you want to click, press Search + Space.
              Try it now to continue.`,
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [39]}},
              afterActionMsg: 'Great! You pressed Search + right arrow.'
            },
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [32]}},
              afterActionMsg: 'Great! You pressed Search + space',
            }
          ],
          autoInteractive: true,
        },

        {
          title: 'Tab Navigation',
          content: [
            `You can also use the Tab key to move to the next interactive item
            on the screen. Find the Tab key, which is directly above the Search
            key. To continue, press the Tab key.`
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */]}},
            afterActionMsg: 'Great, you found the tab key!'
          }],
          autoInteractive: true,
        },

        {
          title: 'Tab Navigation Continued',
          content: [
            `You can use Shift + Tab to move to the previous interactive item.
            To continue, press Shift + Tab. `
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */], shiftKey: [true]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'Enter',
          content: [`You can also press Enter to activate items. For example,
            Enter can be used to submit text in a form. To continue, press
            Enter.`],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [13 /* Enter */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'Drop-Down Lists',
          content: [
            `There will be times when you need to select an item from a
              drop-down list. To do so, first expand the list by pressing Enter
              or Search + Space. Then use the Up and Down arrow keys to select
              an item. Finally, collapse the list by pressing Enter or Search +
              Space.`,
            `You can try this out in the Practice Area. To continue, use Search
              + Right or Tab to find the Next lesson button. Then press Search
              + Space or Enter to activate.`
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          practiceTitle: 'Practice area: Drop-down lists',
          practiceInstructions:
              `Try selecting your favorite season from the list.`,
          practiceFile: 'selects',
          practiceState: {},
          events: [],
          hints: []
        },

        {
          title: 'Quick orientation complete!',
          content: [
            `Well done! You’ve learned the ChromeVox basics. You can go through
              the tutorial again or exit this tutorial by finding and clicking
              on a button below.`,
            `After you set up your device, you can come back to this tutorial
              and view more lessons by pressing Search + O, then T.`,
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
        },

        {
          title: 'tutorial_on_off_heading',
          content: ['tutorial_control', 'tutorial_on_off'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.ESSENTIAL_KEYS],
        },

        {
          title: 'tutorial_modifier_heading',
          content: ['tutorial_modifier', 'tutorial_chromebook_search'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.ESSENTIAL_KEYS],
        },

        {
          title: 'tutorial_basic_navigation_heading',
          content: ['tutorial_basic_navigation'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NAVIGATION],
          practiceTitle: 'Basic Navigation Practice',
          practiceInstructions:
              'Try using basic navigation to navigate through the items ' +
              'below. Find the button titled "Click me" and use Search ' +
              '+ Space to click it. Then move to the next lesson.',
          practiceFile: 'basic_navigation',
          practiceState: {
            goal: {click: false},
          },
          events: ['click'],
          hints: [
            'Try pressing Search + left/right arrow. The search key is' +
                ' directly above the shift key',
            'Press Search + Space to activate the current item.'
          ],
        },

        {
          title: 'tutorial_jump_heading',
          content: [
            'tutorial_jump',
            'tutorial_jump_second_heading',
            'tutorial_jump_wrap_heading',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NAVIGATION],
          practiceTitle: 'Jump Commands Practice',
          practiceInstructions:
              'Try using what you have learned to navigate by element type. ' +
              'Notice that navigation wraps if you are on the first or ' +
              'last element and press previous element or next element, ' +
              'respectively.',
          practiceFile: 'jump_commands',
          practiceState: {
            'first-heading': {focus: false},
            'first-link': {focus: false},
            'first-button': {focus: false},
            'second-heading': {focus: false},
            'second-link': {focus: false},
            'second-button': {focus: false},
            'last-heading': {focus: false},
            'last-link': {focus: false},
            'last-button': {focus: false},
          },
          events: ['focus'],
          hints: [
            'Try using search + h to move by header',
            'Try using search + b to move by button',
            'Try using search + l to move by link'
          ],
        },

        {
          title: 'tutorial_menus_heading',
          content: [
            'tutorial_menus',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.COMMAND_REFERENCES]
        },

        {
          title: 'tutorial_chrome_shortcuts_heading',
          content: [
            'tutorial_chrome_shortcuts',
            'tutorial_chromebook_ctrl_forward',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.COMMAND_REFERENCES]
        },

        {
          title: 'tutorial_earcon_page_title',
          content: ['tutorial_earcon_page_body'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.SOUNDS_AND_SETTINGS]
        },

        {
          title: 'Text fields',
          content: ['Text content for text fields lesson'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NONE],
          practiceTitle: 'Edit fields practice',
          practiceInstructions:
              'Try using what you have learned about text fields and edit ' +
              'the text fields below.',
          practiceFile: 'text_fields',
          practiceState: {
            input: {focus: false, input: false},
            editable: {focus: false, input: false}
          },
          events: ['focus', 'input'],
          hints: [
            'Once you find an editable element, you can type normally.',
            'Try editing the content you entered'
          ]
        },

        {
          title: 'tutorial_learn_more_heading',
          content: [
            'tutorial_learn_more', 'next_command_reference',
            'chrome_keyboard_shortcuts', 'touchscreen_accessibility'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.RESOURCES],
        }
      ]
    }
  },

  /** @override */
  ready() {
    this.hideAllScreens();
    document.addEventListener('keydown', this.onKeyDown.bind(this));
    this.addEventListener('lessonready', () => {
      this.numLoadedLessons += 1;
      if (this.numLoadedLessons === this.lessonData.length) {
        this.buildEarconLesson();
        this.dispatchEvent(
            new CustomEvent('readyfortesting', {composed: true}));
      }
    });
    this.$.lessonTemplate.addEventListener('dom-change', (evt) => {
      // Executes once all lessons have been added to the dom.
      this.show();
    });
    this.$.tutorial.addEventListener('focus', this.onFocus.bind(this), true);
    this.addEventListener('startpractice', (evt) => {
      this.isPracticeAreaActive = true;
      this.startNudges(NudgeType.PRACTICE_AREA);
    });
    this.addEventListener('endpractice', (evt) => {
      this.isPracticeAreaActive = false;
      this.startNudges(NudgeType.GENERAL);
    });
  },

  /** Shows the tutorial */
  show() {
    if (this.curriculum === Curriculum.QUICK_ORIENTATION) {
      // If opening the tutorial from the OOBE, automatically show the first
      // lesson.
      this.updateIncludedLessons();
      this.showLesson(0);
    } else {
      this.showMainMenu();
    }
    this.startNudges(NudgeType.GENERAL);
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  chooseCurriculum(evt) {
    const id = evt.target.id;
    if (id === 'quickOrientationButton') {
      this.curriculum = Curriculum.QUICK_ORIENTATION;
    } else if (id === 'essentialKeysButton') {
      this.curriculum = Curriculum.ESSENTIAL_KEYS;
    } else if (id === 'navigationButton') {
      this.curriculum = Curriculum.NAVIGATION;
    } else if (id === 'commandReferencesButton') {
      this.curriculum = Curriculum.COMMAND_REFERENCES;
    } else if (id === 'soundsAndSettingsButton') {
      this.curriculum = Curriculum.SOUNDS_AND_SETTINGS;
    } else if (id === 'resourcesButton') {
      this.curriculum = Curriculum.RESOURCES;
    } else {
      throw new Error('Invalid target for chooseCurriculum: ' + evt.target.id);
    }
    this.showLessonMenu();
  },

  showNextLesson() {
    this.showLesson(this.activeLessonIndex + 1);
  },

  /** @private */
  showPreviousLesson() {
    this.showLesson(this.activeLessonIndex - 1);
  },

  /** @private */
  showFirstLesson() {
    this.showLesson(0);
  },

  /**
   * @param {number} index
   * @private
   */
  showLesson(index) {
    this.showLessonContainer();
    if (this.interactiveMode) {
      this.stopInteractiveMode();
    }
    if (index < 0 || index >= this.numLessons) {
      return;
    }

    this.activeLessonIndex = index;

    // Lessons observe activeLessonNum. When updated, lessons automatically
    // update their visibility.
    this.activeLessonNum = this.includedLessons[index].lessonNum;

    const lesson = this.getCurrentLesson();
    if (lesson.autoInteractive) {
      this.startInteractiveMode(lesson.actions);
      // Read the title since initial focus gets placed on the first piece of
      // text content.
      this.readCurrentLessonTitle();
    } else {
      // Otherwise, automatically read current lesson content.
      setTimeout(this.readCurrentLessonContent.bind(this), 1000);
    }
  },

  // Methods for hiding and showing screens.

  /** @private */
  hideAllScreens() {
    this.activeScreen = Screen.NONE;
  },

  /** @private */
  showMainMenu() {
    this.activeScreen = Screen.MAIN_MENU;
    this.$.mainMenuHeader.focus();
  },

  /** @private */
  showLessonMenu() {
    this.activeScreen = Screen.LESSON_MENU;
    this.createLessonShortcuts();
    this.$.lessonMenuHeader.focus();
  },

  /** @private */
  showLessonContainer() {
    this.activeScreen = Screen.LESSON;
  },

  /** @private */
  updateIncludedLessons() {
    this.includedLessons = [];
    this.activeLessonNum = -1;
    this.activeLessonIndex = -1;
    this.numLessons = 0;
    const lessons = this.$.lessonContainer.children;
    for (let i = 0; i < lessons.length; ++i) {
      const element = lessons[i];
      if (element.is !== 'tutorial-lesson') {
        continue;
      }

      const lesson = element;
      if (lesson.shouldInclude(this.medium, this.curriculum)) {
        this.includedLessons.push(lesson);
      }
    }
    this.numLessons = this.includedLessons.length;
    this.createLessonShortcuts();
  },

  /** @private */
  onActiveScreenChanged() {
    if (this.interactiveMode) {
      this.stopInteractiveMode();
    }
  },

  /** @private */
  createLessonShortcuts() {
    // Clear previous lesson shortcuts, as the user may have chosen a new
    // curriculum or medium for the tutorial.
    this.$.lessonShortcuts.innerHTML = '';

    // Create shortcuts for each included lesson.
    let count = 1;
    for (const lesson of this.includedLessons) {
      const button = document.createElement('cr-button');
      button.addEventListener('click', this.showLesson.bind(this, count - 1));
      button.textContent = this.getMsg(lesson.title);
      this.$.lessonShortcuts.appendChild(button);
      count += 1;
    }
  },


  // Methods for computing attributes and properties.

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideNextLessonButton(activeLessonIndex, activeScreen) {
    return activeLessonIndex === this.numLessons - 1 ||
        activeScreen !== Screen.LESSON;
  },

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHidePreviousLessonButton(activeLessonIndex, activeScreen) {
    return activeLessonIndex === 0 || activeScreen !== Screen.LESSON;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonMenuButton(activeScreen) {
    return !this.curriculum || this.curriculum === Curriculum.NONE ||
        activeScreen === Screen.MAIN_MENU ||
        activeScreen === Screen.LESSON_MENU;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideMainMenuButton(activeScreen) {
    return activeScreen === Screen.MAIN_MENU;
  },

  /**
   * @param {number} activeLessonIndex
   * @return {boolean}
   * @private
   */
  shouldHideRestartQuickOrientationButton(activeLessonIndex, activeScreen) {
    // Only show when the user is on the last screen of the basic orientation.
    return !(
        this.curriculum === Curriculum.QUICK_ORIENTATION &&
        activeLessonIndex === this.numLessons - 1 &&
        this.activeScreen === Screen.LESSON);
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideMainMenu(activeScreen) {
    return activeScreen !== Screen.MAIN_MENU;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonContainer(activeScreen) {
    return activeScreen !== Screen.LESSON;
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonMenu(activeScreen) {
    return activeScreen !== Screen.LESSON_MENU;
  },

  /**
   * @param {Curriculum} curriculum
   * @return {string}
   * @private
   */
  computeLessonMenuHeader(curriculum) {
    // TODO (akihiroota): localize. (http://crbug.com/1124068).
    let numLessons = 0;
    for (let i = 0; i < this.lessonData.length; ++i) {
      if (this.lessonData[i].curriculums.includes(curriculum)) {
        numLessons += 1;
      }
    }
    // Remove underscores and capitalize the first letter of each word.
    const words = curriculum.split('_');
    for (let i = 0; i < words.length; ++i) {
      words[i] = words[i][0].toUpperCase() + words[i].substring(1);
    }
    const curriculumCopy = words.join(' ');
    return `${curriculumCopy} Tutorial, ${numLessons} ${
        numLessons > 1 ? 'Lessons' : 'Lesson'}`;
  },

  /** @private */
  exit() {
    this.stopNudges();
    this.dispatchEvent(new CustomEvent('closetutorial', {}));
  },

  // Interactive mode.

  /**
   * @param {!Array<{
   *    type: string,
   *    value: (string|Object),
   *    beforeActionMsg: (string|undefined),
   *    afterActionMsg: (string|undefined)}>} actions
   * @private
   */
  startInteractiveMode(actions) {
    this.interactiveMode = true;
    this.dispatchEvent(new CustomEvent(
        'startinteractivemode', {composed: true, detail: {actions}}));
  },

  /** @private */
  stopInteractiveMode() {
    this.interactiveMode = false;
    this.dispatchEvent(
        new CustomEvent('stopinteractivemode', {composed: true}));
  },

  /**
   * @param {Event} evt
   * @private
   */
  onKeyDown(evt) {
    const key = evt.key;
    if (key === 'Escape') {
      this.exit();
      evt.preventDefault();
      evt.stopPropagation();
      return;
    }

    if (window.BackgroundKeyboardHandler &&
        window.BackgroundKeyboardHandler.onKeyDown) {
      window.BackgroundKeyboardHandler.onKeyDown(evt);
    }

    if (key === 'Tab' && this.interactiveMode) {
      // Prevent Tab from being used in interactive mode. This ensures the user
      // can only navigate if they press the expected sequence of keys.
      evt.preventDefault();
      evt.stopPropagation();
    }
  },

  // Nudges.

  /**
   * @param {NudgeType} type
   * @private
   */
  startNudges(type) {
    this.stopNudges();
    this.initializeNudges(type);
    this.setNudgeInterval();
  },

  /** @private */
  setNudgeInterval() {
    this.nudgeIntervalId =
        setInterval(this.giveNudge.bind(this), this.NUDGE_INTERVAL_TIME_MS);
  },

  /**
   * @param {NudgeType} type
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  initializeNudges(type) {
    const maybeGiveNudge = (msg) => {
      if (this.interactiveMode) {
        // Do not announce message since ChromeVox blocks actions in interactive
        // mode.
        return;
      }

      this.requestSpeech(msg, QueueMode.INTERJECT);
    };

    this.nudgeArray = [];
    if (type === NudgeType.PRACTICE_AREA) {
      // Convert hint strings into functions that will request speech for those
      // strings.
      const hints = this.lessonData[this.activeLessonNum].hints;
      for (const hint of hints) {
        this.nudgeArray.push(
            this.requestSpeech.bind(this, hint, QueueMode.INTERJECT));
      }
    } else if (type === NudgeType.GENERAL) {
      this.nudgeArray = [
        this.requestFullyDescribe.bind(this),
        this.requestFullyDescribe.bind(this),
        this.requestFullyDescribe.bind(this),
        maybeGiveNudge.bind(
            this, 'Hint: Hold Search and press the arrow keys to navigate.'),
        maybeGiveNudge.bind(
            this, 'Hint: Press Search + Space to activate the current item.'),
        this.requestSpeech.bind(
            this, 'Hint: Press Escape if you would like to exit this tutorial.',
            QueueMode.INTERJECT)
      ];
    } else {
      throw new Error('Invalid NudgeType: ' + type);
    }
  },

  /** @private */
  stopNudges() {
    this.nudgeCounter = 0;
    if (this.nudgeIntervalId) {
      clearInterval(this.nudgeIntervalId);
    }
  },

  /** @private */
  restartNudges() {
    this.stopNudges();
    this.setNudgeInterval();
  },

  /** @private */
  giveNudge() {
    if (this.nudgeCounter < 0 || this.nudgeCounter >= this.nudgeArray.length) {
      this.stopNudges();
      return;
    }

    this.nudgeArray[this.nudgeCounter]();
    this.nudgeCounter += 1;
  },

  /**
   * @param {string} text
   * @param {number} queueMode
   * @param {{doNotInterrupt: boolean}=} properties
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  requestSpeech(text, queueMode, properties) {
    this.dispatchEvent(new CustomEvent(
        'requestspeech',
        {composed: true, detail: {text, queueMode, properties}}));
  },

  /** @private */
  requestFullyDescribe() {
    this.dispatchEvent(
        new CustomEvent('requestfullydescribe', {composed: true}));
  },

  /**
   * @param {Event} evt
   * @private
   */
  onFocus(evt) {
    // Restart nudges whenever focus changes. Skip this for the practice area
    // so nudges are given in regular intervals.
    if (this.isPracticeAreaActive) {
      return;
    }

    this.restartNudges();
  },

  /** @return {!TutorialLesson} */
  getCurrentLesson() {
    return this.includedLessons[this.activeLessonIndex];
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  readCurrentLessonTitle() {
    const lesson = this.getCurrentLesson();
    this.requestSpeech(
        this.getMsg(lesson.title), QueueMode.INTERJECT, {doNotInterrupt: true});
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing QueueMode,
   * which is defined on the Panel window.
   */
  readCurrentLessonContent() {
    const lesson = this.getCurrentLesson();
    for (const text of lesson.content) {
      // Queue lesson content so it is read after the lesson title.
      this.requestSpeech(this.getMsg(text), QueueMode.QUEUE);
    }
  },

  /**
   * @private
   * @suppress {undefinedVars|missingProperties} For referencing
   * EarconDescription, which is defined on the Panel window.
   */
  buildEarconLesson() {
    // Find earcon lesson.
    let earconLesson;
    const elements = this.$.lessonContainer.children;
    for (const element of elements) {
      if (element.is === 'tutorial-lesson' &&
          element.title === 'tutorial_earcon_page_title') {
        earconLesson = element;
      }
    }

    if (!earconLesson) {
      throw new Error('Could not find the earcon lesson.');
    }

    // Add text and listeners.
    for (const earconId in EarconDescription) {
      const msgid = EarconDescription[earconId];
      const earconElement = document.createElement('p');
      earconElement.innerText = this.getMsg(msgid);
      earconElement.setAttribute('tabindex', -1);
      earconElement.addEventListener(
          'focus', this.requestEarcon.bind(this, earconId));
      earconLesson.contentDiv.appendChild(earconElement);
    }
  },

  /**
   * @param {string} earconId
   * @private
   */
  requestEarcon(earconId) {
    this.dispatchEvent(
        new CustomEvent('requestearcon', {composed: true, detail: {earconId}}));
  },
});
