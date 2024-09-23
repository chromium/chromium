// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines TutorialBehavior, a Polymer behavior to perform generic
 * tutorial functions, like showing/hiding screens.
 */

import {Curriculum, InteractionMedium, LessonData, MainMenuButtonData, NO_ACTIVE_LESSON, Screen} from './constants.js';
import {TutorialLesson} from './tutorial_lesson.js';

/** @polymerBehavior */
export const TutorialBehavior = {
  properties: {
    /** @type {Curriculum} */
    curriculum: {
      type: String,
      value: Curriculum.NONE,
      observer: 'updateIncludedLessons_',
    },

    /** @type {InteractionMedium} */
    medium: {
      type: String,
      value: InteractionMedium.NONE,
      observer: 'updateIncludedLessons_',
    },

    /**
     * Stores included lessons.
     * Not all lessons are included, some are filtered out based on the current
     * medium and curriculum.
     * @private {Array<!TutorialLesson>}
     */
    includedLessons_: {type: Array},

    /**
     * The number of included lessons.
     * @type {number}
     */
    numLessons: {type: Number, value: 0},

    /**
     * An index into |includedLessons_|, representing the currently active
     * lesson.
     * @type {number}
     */
    activeLessonIndex: {type: Number, value: NO_ACTIVE_LESSON},

    /**
     * The ID of the currently active lesson.
     * @type {number}
     */
    activeLessonId: {
      type: Number,
      value: NO_ACTIVE_LESSON,
      observer: 'onActiveLessonIdChanged_',
    },

    /** @type {Screen} */
    activeScreen: {type: String, observer: 'onActiveScreenChanged_'},

    /** @type {number} */
    numLoadedLessons: {type: Number, value: 0},

    /** @type {boolean} */
    isPracticeAreaActive: {type: Boolean, value: false},

    /** @type {boolean} */
    isVisible: {type: Boolean, observer: 'onTutorialVisibilityChanged_'},

    /**
     * Should be defined by component implementing this behavior.
     * @type {Array<!LessonData>}
     */
    lessonData: {type: Array},

    /**
     * Should be defined by component implementing this behavior.
     * @type {Array<!MainMenuButtonData>}
     */
    mainMenuButtonData: {type: Array},
  },

  // Public methods.

  /**
   * Shows the tutorial by assigning isVisible. Components that implement this
   * behavior should add an observer for isVisible if any additional logic
   * needs to be performed when the tutorial is shown.
   */
  show() {
    if (this.curriculum === Curriculum.QUICK_ORIENTATION ||
        this.curriculum === Curriculum.TOUCH_ORIENTATION) {
      // If opening the tutorial from the OOBE, automatically show the first
      // lesson.
      this.updateIncludedLessons_();
      this.showLesson_(0);
    } else {
      this.showMainMenu_();
    }
    this.isVisible = true;
  },

  /** Shows the next lesson. */
  showNextLesson() {
    this.showLesson_(this.activeLessonIndex + 1);
  },

  /**
   * Hides all screens by assigning activeScreen. Components that observe this
   * variable will update their visibility accordingly.
   */
  hideAllScreens() {
    this.activeScreen = Screen.NONE;
  },

  /** Exits the tutorial. */
  exit() {
    this.isVisible = false;
  },

  /** @return {!TutorialLesson} */
  getCurrentLesson() {
    return this.includedLessons_[this.activeLessonIndex];
  },

  /**
   * Find and return a lesson with the given title message id.
   * @param {string} titleMsgId The message id of the lesson's title
   * @return {TutorialLesson}
   */
  getLessonWithTitle(titleMsgId) {
    const lessons = this.$.lessonContainer.getLessonsFromDom();
    for (const lesson of lessons) {
      if (lesson.title === titleMsgId) {
        return lesson;
      }
    }
    return null;
  },

  // Private methods.

  /**
   * @return {Array<!{title: string, curriculums: !Array<Curriculum>}>}
   * @private
   */
  computeLessonMenuButtonData_() {
    const ret = [];
    for (let i = 0; i < this.lessonData.length; ++i) {
      ret.push({
        title: this.lessonData[i].title,
        curriculums: this.lessonData[i].curriculums,
        lessonId: i,
      });
    }
    return ret;
  },

  /** @private */
  showMainMenu_() {
    this.activeScreen = Screen.MAIN_MENU;
  },

  /** @private */
  showLessonMenu_() {
    if (this.includedLessons_.length === 1) {
      // If there's only one lesson, immediately show it.
      this.showLesson_(0);
      this.activeScreen = Screen.LESSON;
    } else {
      this.activeScreen = Screen.LESSON_MENU;
    }
  },

  /** @private */
  showLessonContainer_() {
    this.activeScreen = Screen.LESSON;
  },

  /** @private */
  showPreviousLesson_() {
    this.showLesson_(this.activeLessonIndex - 1);
  },

  /** @private */
  showFirstLesson_() {
    this.showLesson_(0);
  },

  /**
   * @param {number} lessonId
   * @private
   */
  showLessonFromId_(lessonId) {
    for (let i = 0; i < this.includedLessons_.length; ++i) {
      const lesson = this.includedLessons_[i];
      if (lessonId === lesson.lessonId) {
        this.showLesson_(i);
      }
    }
  },

  /**
   * @param {number} index
   * @private
   */
  showLesson_(index) {
    if (index < 0 || index >= this.numLessons) {
      return;
    }

    this.showLessonContainer_();
    this.activeLessonIndex = index;
    // Lessons observe activeLessonId. When updated, lessons automatically
    // update their visibility.
    this.activeLessonId = this.includedLessons_[index].lessonId;
  },

  /** @private */
  updateIncludedLessons_() {
    this.includedLessons_ = [];
    this.activeLessonId = -1;
    this.activeLessonIndex = -1;
    this.numLessons = 0;
    const lessons = this.$.lessonContainer.getLessonsFromDom();
    for (const lesson of lessons) {
      if (lesson.shouldInclude(this.medium, this.curriculum)) {
        this.includedLessons_.push(lesson);
      }
    }
    this.numLessons = this.includedLessons_.length;
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  onMainMenuButtonClicked_(evt) {
    const detail =
        /** @type {!MainMenuButtonData} */ (evt.detail);
    this.curriculum = detail.curriculum;
    this.showLessonMenu_();
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  onLessonMenuButtonClicked_(evt) {
    const detail =
        /**
           @type {!{title: string, curriculums: Array<Curriculum>, lessonId:
               number}}
         */
        (evt.detail);
    this.showLessonFromId_(detail.lessonId);
  },

  // Components that import this behavior can override the functions below
  // if they want to perform special logic when a variable gets updated.

  /** @private */
  onActiveScreenChanged_() {},

  /** @private */
  onActiveLessonIdChanged_() {},

  /** @private */
  onTutorialVisibilityChanged_() {},
};

export class TutorialBehaviorInterface {
  constructor() {
    /** @type {number} */
    this.activeLessonId;
    /** @type {number} */
    this.activeLessonIndex;
    /** @type {Screen} */
    this.activeScreen;
    /** @type {Curriculum} */
    this.curriculum;
    /** @type {boolean} */
    this.isPracticeAreaActive;
    /** @type {boolean} */
    this.isVisible;
    /** @type {Array<!LessonData>} */
    this.lessonData;
    /** @type {Array<!MainMenuButtonData>} */
    this.mainMenuButtonData;
    /** @type {InteractionMedium} */
    this.medium;
    /** @type {number} */
    this.numLessons;
    /** @type {number} */
    this.numLoadedLessons;
  }

  exit() {}
  hideAllScreens() {}
  show() {}
  showNextLesson() {}

  /** @return {!TutorialLesson} */
  getCurrentLesson() {}
  /**
   * @param {string} titleMsgId
   * @return {TutorialLesson}
   */
  getLessonWithTitle(titleMsgId) {}
}