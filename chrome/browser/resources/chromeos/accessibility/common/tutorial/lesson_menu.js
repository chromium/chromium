// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the lesson menu in the
 * tutorial.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Curriculum, Screen} from './constants.js';
import {Localization} from './localization.js';

export const LessonMenu = Polymer({
  is: 'lesson-menu',

  _template: html`{__html_template__}`,

  behaviors: [Localization],

  properties: {
    /** @private {string} */
    headerDescription_: {type: String},

    /** @private {Array<!{title: string, curriculums: !Array<Curriculum>}>} */
    buttonData_: {type: Array},

    // Observed properties.

    /** @type {Screen} */
    activeScreen: {type: String, observer: 'maybeFocusHeader_'},

    /** @type {Curriculum} */
    curriculum: {
      type: String,
      value: Curriculum.NONE,
    },

    /** @type {number} */
    numLessons: {type: Number},
  },

  /** @private */
  maybeFocusHeader_() {
    if (this.activeScreen === Screen.LESSON_MENU) {
      this.$.lessonMenuHeader.focus();
    }
  },

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonMenu_(activeScreen) {
    return activeScreen !== Screen.LESSON_MENU;
  },

  /**
   * @param {Curriculum} curriculum
   * @return {string}
   * @private
   */
  computeLessonMenuHeader_(curriculum) {
    // Remove underscores and capitalize the first letter of each word.
    const words = curriculum.split('_');
    for (let i = 0; i < words.length; ++i) {
      words[i] = words[i][0].toUpperCase() + words[i].substring(1);
    }
    const curriculumCopy = words.join(' ');
    return this.getMsg(
        'tutorial_lesson_menu_header', [curriculumCopy, this.numLessons]);
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  onButtonClicked_(evt) {
    // Fires an event with button data attached to |evt.detail|.
    this.fire('button-clicked', evt.model.data);
  },

  /**
   * @param {Array<Curriculum>} validCurriculums
   * @param {Curriculum} curriculum
   * @return {boolean}
   * @private
   */
  shouldHideLessonButton_(validCurriculums, curriculum) {
    return !validCurriculums.includes(curriculum);
  },
});
