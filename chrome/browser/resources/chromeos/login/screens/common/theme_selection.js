// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for theme selection screen.
 */
/* #js_imports_placeholder */

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 */
const ThemeSelectionScreenElementBase = Polymer.mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
    Polymer.Element);

/**
 * Enum to represent steps on the theme selection screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 * @enum {string}
 */
const ThemeSelectionStep = {
  OVERVIEW: 'overview',
};

/**
 * Available themes. The values should be in sync with the enum
 * defined in theme_selection_screen.h
 * @enum {number}
 */
const SelectedTheme = {
  AUTO: 0,
  DARK: 1,
  LIGHT: 2,
};

/**
 * Available user actions.
 * @enum {string}
 */
const UserAction = {
  SELECT: 'select',
  NEXT: 'next',
};

/**
 * @polymer
 */
class ThemeSelectionScreen extends ThemeSelectionScreenElementBase {
  static get is() {
    return 'theme-selection-element';
  }

  /* #html_template_placeholder */

  static get properties() {
    return {
      /**
       * Indicates selected theme
       * @private
       */
      selectedTheme: {type: String, value: 'auto', observer: 'onThemeChanged_'},

      /**
       * Indicates if the device is used in tablet mode
       * @private
       */
      isInTabletMode_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();
  }

  get UI_STEPS() {
    return ThemeSelectionStep;
  }

  defaultUIStep() {
    return ThemeSelectionStep.OVERVIEW;
  }

  get EXTERNAL_API() {
    return [];
  }

  /**
   * Updates "device in tablet mode" state when tablet mode is changed.
   * Overridden from LoginScreenBehavior.
   * @param {boolean} isInTabletMode True when in tablet mode.
   */
  setTabletModeState(isInTabletMode) {
    this.isInTabletMode_ = isInTabletMode;
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ThemeSelectionScreen');
    this.selectedTheme = 'auto';
  }

  onBeforeShow(data) {
    this.selectedTheme = 'selectedTheme' in data && data.selectedTheme;
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.THEME_SELECTION;
  }

  onNextClicked_() {
    this.userActed(UserAction.NEXT);
  }

  onThemeChanged_(themeSelect, oldTheme) {
    if (oldTheme === undefined) {
      return;
    }
    if (themeSelect === 'auto') {
      this.userActed([UserAction.SELECT, SelectedTheme.AUTO]);
    }
    if (themeSelect === 'light') {
      this.userActed([UserAction.SELECT, SelectedTheme.LIGHT]);
    }
    if (themeSelect === 'dark') {
      this.userActed([UserAction.SELECT, SelectedTheme.DARK]);
    }
  }
}
customElements.define(ThemeSelectionScreen.is, ThemeSelectionScreen);
