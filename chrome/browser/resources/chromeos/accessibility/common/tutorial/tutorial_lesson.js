// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for a lesson in the
 * ChromeVox interactive tutorial.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/ash/common/cr_elements/md_select.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/ash/common/cr_elements/cr_shared_vars.css.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InteractionMedium} from './constants.js';
import {Localization, LocalizationInterface} from './localization.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LocalizationInterface}
 */
const TutorialLessonBase = mixinBehaviors([Localization], PolymerElement);

/** @polymer */
export class TutorialLesson extends TutorialLessonBase {
  static get is() {
    return 'tutorial-lesson';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      lessonId: {type: Number},

      title: {type: String},

      content: {type: Array},

      medium: {type: String},

      curriculums: {type: Array},

      practiceTitle: {type: String},

      practiceInstructions: {type: String},

      practiceFile: {type: String},

      actions: {type: Array},

      autoInteractive: {type: Boolean, value: false},

      // Observed properties.
      /** @type {number} */
      activeLessonId: {type: Number, observer: 'setVisibility'},
    };
  }

  /** @override */
  ready() {
    super.ready();

    this.$.contentTemplate.addEventListener('dom-change', evt => {
      this.dispatchEvent(new CustomEvent('lessonready', {composed: true}));
    });


    if (this.practiceFile) {
      this.populatePracticeContent();
      this.$.practiceContent.addEventListener('focus', evt => {
        // The practice area has the potential to overflow, so ensure elements
        // are scrolled into view when focused.
        evt.target.scrollIntoView();
      }, true);
      this.$.practiceContent.addEventListener('click', evt => {
        // Intercept click events. For example, clicking a link will exit the
        // tutorial without this listener.
        evt.preventDefault();
        evt.stopPropagation();
      }, true);
    }
  }

  /**
   * Updates this lessons visibility whenever the active lesson of the tutorial
   * changes.
   * @private
   */
  setVisibility() {
    if (this.lessonId === this.activeLessonId) {
      this.show();
    } else {
      this.hide();
    }
  }

  /** @private */
  show() {
    this.$.container.hidden = false;
    let focus;
    if (this.autoInteractive) {
      // Auto interactive lessons immediately initialize the UserActionMonitor,
      // which will block ChromeVox execution until a desired key sequence is
      // pressed. To ensure users hear instructions for these lessons, place
      // focus on the first piece of text content.
      // Shorthand for Polymer.dom(this.root).querySelector(...).
      focus = this.shadowRoot.querySelector('p');
    } else {
      // Otherwise, we can place focus on the lesson title.
      focus = this.shadowRoot.querySelector('h1');
    }
    if (!focus) {
      throw new Error('A lesson must have an element to focus.');
    }
    focus.focus();
    if (!focus.isEqualNode(this.shadowRoot.activeElement)) {
      // Call show() again if we weren't able to focus the target element.
      setTimeout(() => this.show(), 500);
    }
  }

  /** @private */
  hide() {
    this.$.container.hidden = true;
  }

  // Methods for managing the practice area.

  /**
   * Asynchronously populates practice area.
   * @private
   */
  async populatePracticeContent() {
    const path = '../tutorial/practice_areas/' + this.practiceFile + '.html';
    try {
      const resp = await fetch(path);
      if (resp.ok) {
        this.$.practiceContent.innerHTML = await resp.text();
        this.localizePracticeAreaContent();
      } else {
        console.error(`${resp.status}: ${resp.statusText}`);
      }
    } catch (err) {
      console.error('Failed to open practice file: ' + path);
      console.error(err);
    }
  }

  /** @private */
  startPractice() {
    this.notifyStartPractice();
    this.$.practice.showModal();
    this.$.practiceInstructions.focus();
  }

  /** @private */
  endPractice() {
    this.$.practice.close();
    this.notifyEndPractice();
    this.$.startPractice.focus();
  }

  /** @private */
  notifyStartPractice() {
    this.dispatchEvent(new CustomEvent('startpractice', {composed: true}));
  }

  /** @private */
  notifyEndPractice() {
    this.dispatchEvent(new CustomEvent('endpractice', {composed: true}));
  }

  // Miscellaneous methods.

  /**
   * @param {string} medium
   * @param {string} curriculum
   * @return {boolean}
   */
  shouldInclude(medium, curriculum) {
    if (this.medium === medium && this.curriculums.includes(curriculum)) {
      return true;
    }

    return false;
  }

  /**
   * @return {boolean}
   * @private
   */
  shouldHidePracticeButton() {
    if (!this.practiceFile) {
      return true;
    }

    return false;
  }

  /** @return {Element} */
  get contentDiv() {
    return this.$.content;
  }

  /** @return {string} */
  getTitleText() {
    return this.$.title.textContent;
  }

  /** @private */
  localizePracticeAreaContent() {
    const root = this.$.practiceContent;
    const elements = root.querySelectorAll('[msgid]');
    for (const element of elements) {
      const msgId = element.getAttribute('msgid');
      element.textContent = this.getMsg(msgId);
    }
  }

  /**
   * @private
   * @return {string}
   */
  computeTitleDescription_() {
    if (this.medium === InteractionMedium.KEYBOARD) {
      return this.getMsg('tutorial_lesson_title_description');
    }

    // Automatically return the description for touch, since the only supported
    // interaction mediums are touch and keyboard.
    return this.getMsg('tutorial_touch_lesson_title_description');
  }
}
customElements.define(TutorialLesson.is, TutorialLesson);
