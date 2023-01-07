// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the lesson container in
 * the tutorial.
 */

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {LessonData, Screen} from './constants.js';
import {TutorialLesson} from './tutorial_lesson.js';

export const LessonContainer = Polymer({
  is: 'lesson-container',

  _template: html`{__html_template__}`,

  properties: {
    /** @type {Array<!LessonData>} */
    lessonData: {type: Array},

    // Observed properties.
    /** @type {Screen} */
    activeScreen: {type: String},

    /** @type {number} */
    activeLessonId: {type: Number},
  },

  /** @override */
  ready() {
    this.$.lessonTemplate.addEventListener('dom-change', evt => {
      // Executes once all lessons have been added to the dom.
      this.onLessonsLoaded_();
    });
  },

  /** @private */
  onLessonsLoaded_() {
    this.fire('lessons-loaded');
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonContainer_(activeScreen) {
    return activeScreen !== Screen.LESSON;
  },

  /** @return {!Array<!TutorialLesson>} */
  getLessonsFromDom() {
    const lessons = [];
    const elements = this.$.lessonContainer.children;
    for (let i = 0; i < elements.length; ++i) {
      const element = elements[i];
      if (element.is !== 'tutorial-lesson') {
        continue;
      }

      lessons.push(element);
    }

    return lessons;
  },
});
