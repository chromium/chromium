// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines constants used throughout the tutorial.
 */

/**
 * The various curriculums for the ChromeVox tutorial.
 * @enum {string}
 */
export const Curriculum = {
  NONE: 'none',
  QUICK_ORIENTATION: 'quick_orientation',
  ESSENTIAL_KEYS: 'essential_keys',
  NAVIGATION: 'navigation',
  COMMAND_REFERENCES: 'command_references',
  SOUNDS_AND_SETTINGS: 'sounds_and_settings',
  RESOURCES: 'resources',
  TOUCH_ORIENTATION: 'touch_orientation',
};

/**
 * The user’s interaction medium. Influences tutorial content.
 * Note: keep in sync with the enum in
 * c/b/r/c/accessibility/common/constants.js.
 * TODO: Unify with the above file when ES6 is standard in ChromeVox.
 * @enum {string}
 */
export const InteractionMedium = {
  NONE: 'none',
  KEYBOARD: 'keyboard',
  TOUCH: 'touch',
  BRAILLE: 'braille',
};

/**
 * Defines a type for data used to generate lessons.
 * @typedef {{
 *    title: string,
 *    content: Array<string>,
 *    medium: InteractionMedium,
 *    curriculums: Array<Curriculum>,
 *    actions: (Array<Object>|undefined),
 *    autoInteractive: (boolean|undefined),
 *    practiceTitle: (string|undefined),
 *    practiceInstructions: (string|undefined),
 *    practiceFile: (string|undefined),
 * }}
 */
export let LessonData;

/**
 * Defines a type for data used to generate main menu buttons.
 * @typedef {{
 *    title: string,
 *    curriculum: Curriculum,
 *    medium: InteractionMedium,
 * }}
 */
export let MainMenuButtonData;

/** @const {number} */
export const NO_ACTIVE_LESSON = -1;

/**
 * The various screens within the tutorial.
 * @enum {string}
 */
export const Screen = {
  NONE: 'none',
  MAIN_MENU: 'main_menu',
  LESSON_MENU: 'lesson_menu',
  LESSON: 'lesson',
};
