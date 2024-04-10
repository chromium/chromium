// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the navigation buttons
 * in the tutorial.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Curriculum, Screen} from './constants.js';
import {Localization, LocalizationInterface} from './localization.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LocalizationInterface}
 */
const NavigationButtonsBase =
    mixinBehaviors([Localization], PolymerElement);

/** @polymer */
export class NavigationButtons extends NavigationButtonsBase {
  static get is() {
    return 'navigation-buttons';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      // Observed properties.
      /** @type {Screen} */
      activeScreen: {type: String},
      /** @type {number} */
      activeLessonIndex: {type: Number, value: 0},
      /** @type {Curriculum} */
      curriculum: {
        type: String,
        value: Curriculum.NONE,
      },
      /** @type {number} */
      numLessons: {type: Number, value: 0},
    };
  }

  /** @param {string} eventName */
  fire(eventName) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true}));
  }

  /** @private */
  onNextLessonButtonClicked_() {
    this.fire('next-lesson-button-clicked');
  }

  /** @private */
  onPreviousLessonButtonClicked_() {
    this.fire('previous-lesson-button-clicked');
  }

  /** @private */
  onRestartQuickOrientationButtonClicked_() {
    this.fire('restart-quick-orientation-button-clicked');
  }

  /** @private */
  onLessonMenuButtonClicked_() {
    this.fire('lesson-menu-button-clicked');
  }

  /** @private */
  onMainMenuButtonClicked_() {
    this.fire('main-menu-button-clicked');
  }

  /** @private */
  onExitButtonClicked_() {
    this.fire('exit-button-clicked');
  }

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideNextLessonButton_(activeLessonIndex, activeScreen) {
    return activeLessonIndex === this.numLessons - 1 ||
        activeScreen !== Screen.LESSON;
  }

  /**
   * @param {number} activeLessonIndex
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHidePreviousLessonButton_(activeLessonIndex, activeScreen) {
    return activeLessonIndex === 0 || activeScreen !== Screen.LESSON;
  }

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideLessonMenuButton_(activeScreen) {
    return !this.curriculum || this.curriculum === Curriculum.NONE ||
        activeScreen === Screen.MAIN_MENU ||
        activeScreen === Screen.LESSON_MENU || this.numLessons === 1;
  }

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideMainMenuButton_(activeScreen) {
    return activeScreen === Screen.MAIN_MENU;
  }

  /**
   * @param {number} activeLessonIndex
   * @return {boolean}
   * @private
   */
  shouldHideRestartQuickOrientationButton_(activeLessonIndex, activeScreen) {
    // Only show when the user is on the last screen of the quick orientation.
    return !(
        this.curriculum === Curriculum.QUICK_ORIENTATION &&
        activeLessonIndex === this.numLessons - 1 &&
        this.activeScreen === Screen.LESSON);
  }

  /**
   * @param {Screen} activeScreen
   * @return {boolean}
   * @private
   */
  shouldHideNavSeparator_(activeScreen) {
    return activeScreen !== Screen.LESSON;
  }
}
customElements.define(NavigationButtons.is, NavigationButtons);
