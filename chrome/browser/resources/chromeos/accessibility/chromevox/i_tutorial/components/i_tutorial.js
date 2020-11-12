// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the ChromeVox
 * interactive tutorial engine.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';

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

    lessonData: {
      type: Array,
      value: [
        {
          title: 'tutorial_welcome_heading',
          content: ['tutorial_quick_orientation_intro_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [32 /* Space */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_control_title',
          content: ['tutorial_quick_orientation_control_text'],
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
          title: 'tutorial_quick_orientation_shift_title',
          content: ['tutorial_quick_orientation_shift_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [16 /* Shift */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_search_title',
          content: ['tutorial_quick_orientation_search_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {skipStripping: false, keys: {keyCode: [91 /* Search */]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_basic_navigation_title',
          content: [
            'tutorial_quick_orientation_basic_navigation_move_text',
            'tutorial_quick_orientation_basic_navigation_click_text'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [39]}},
            },
            {
              type: 'key_sequence',
              value: {cvoxModifier: true, keys: {keyCode: [32]}},
            }
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_tab_title',
          content: ['tutorial_quick_orientation_tab_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_shift_tab_title',
          content: ['tutorial_quick_orientation_shift_tab_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [{
            type: 'key_sequence',
            value: {keys: {keyCode: [9 /* Tab */], shiftKey: [true]}},
          }],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_enter_title',
          content: ['tutorial_quick_orientation_enter_text'],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          actions: [
            {type: 'key_sequence', value: {keys: {keyCode: [13 /* Enter */]}}}
          ],
          autoInteractive: true,
        },

        {
          title: 'tutorial_quick_orientation_lists_title',
          content: [
            'tutorial_quick_orientation_lists_text',
            'tutorial_quick_orientation_lists_continue_text'
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.QUICK_ORIENTATION],
          practiceTitle: 'tutorial_quick_orientation_lists_practice_title',
          practiceInstructions:
              'tutorial_quick_orientation_lists_practice_instructions',
          practiceFile: 'selects',
          practiceState: {},
          events: [],
          hints: []
        },

        {
          title: 'tutorial_quick_orientation_complete_title',
          content: [
            'tutorial_quick_orientation_complete_text',
            'tutorial_quick_orientation_complete_additional_text'
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
        },

        {
          title: 'tutorial_jump_heading',
          content: [
            'tutorial_jump',
            'tutorial_jump_efficiency',
            'tutorial_jump_more',
          ],
          medium: InteractionMedium.KEYBOARD,
          curriculums: [Curriculum.NAVIGATION],
          practiceTitle: 'tutorial_jump_practice_title',
          practiceInstructions: 'tutorial_jump_practice_instructions',
          practiceFile: 'jump_commands',
          practiceState: {},
          events: [],
          hints: []
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
          title: 'tutorial_learn_more_heading',
          content: ['tutorial_learn_more'],
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
        this.buildLearnMoreLesson();
        this.dispatchEvent(
            new CustomEvent('readyfortesting', {composed: true}));
      }
    });
    this.$.lessonTemplate.addEventListener('dom-change', (evt) => {
      // Executes once all lessons have been added to the dom.
      this.show();
    });
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
    if (this.includedLessons.length === 1) {
      // If there's only one lesson, immediately show it.
      this.showLesson(0);
      this.activeScreen = Screen.LESSON;
    } else {
      this.activeScreen = Screen.LESSON_MENU;
      this.createLessonShortcuts();
      this.$.lessonMenuHeader.focus();
    }
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
      const shortcut = document.createElement('cr-link-row');
      shortcut.addEventListener('click', this.showLesson.bind(this, count - 1));
      shortcut.label = this.getMsg(lesson.title);
      shortcut.classList.add('hr');
      this.$.lessonShortcuts.appendChild(shortcut);
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
        activeScreen === Screen.LESSON_MENU ||
        this.includedLessons.length === 1;
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

  shouldHideNavSeparator(activeScreen) {
    return activeScreen !== Screen.LESSON;
  },

  /**
   * @param {Curriculum} curriculum
   * @return {string}
   * @private
   */
  computeLessonMenuHeader(curriculum) {
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
    return this.getMsg(
        'tutorial_lesson_menu_header', [curriculumCopy, numLessons]);
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
        maybeGiveNudge.bind(this, this.getMsg('tutorial_hint_navigate')),
        maybeGiveNudge.bind(this, this.getMsg('tutorial_hint_click')),
        this.requestSpeech.bind(
            this, this.getMsg('tutorial_hint_exit'), QueueMode.INTERJECT)
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
    const earconLesson = this.getLessonWithTitle('tutorial_earcon_page_title');
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

  /** @private */
  buildLearnMoreLesson() {
    const learnMoreLesson =
        this.getLessonWithTitle('tutorial_learn_more_heading');
    if (!learnMoreLesson) {
      throw new Error('Could not find the learn more lesson');
    }

    // Add links to resources.
    const resources = [
      {
        msgId: 'next_command_reference',
        link: 'https://www.chromevox.com/next_keyboard_shortcuts.html'
      },
      {
        msgId: 'chrome_keyboard_shortcuts',
        link: 'https://support.google.com/chromebook/answer/183101?hl=en'
      },
      {
        msgId: 'touchscreen_accessibility',
        link: 'https://support.google.com/chromebook/answer/6103702?hl=en'
      },
    ];
    for (const resource of resources) {
      const link = document.createElement('a');
      link.innerText = this.getMsg(resource.msgId);
      link.href = resource.link;
      link.addEventListener('click', (evt) => {
        this.stopNudges();
        this.dispatchEvent(new CustomEvent(
            'openUrl', {composed: true, detail: {url: link.href}}));
        evt.preventDefault();
        evt.stopPropagation();
      });
      learnMoreLesson.contentDiv.appendChild(link);
      const br = document.createElement('br');
      learnMoreLesson.contentDiv.appendChild(br);
    }
  },

  /**
   * Find and return a lesson with the given title message id.
   * @param {string} titleMsgId The message id of the lesson's title
   * @return {Element}
   * @private
   */
  getLessonWithTitle(titleMsgId) {
    const elements = this.$.lessonContainer.children;
    for (const element of elements) {
      if (element.is === 'tutorial-lesson' && element.title === titleMsgId) {
        return element;
      }
    }
    return null;
  }
});
