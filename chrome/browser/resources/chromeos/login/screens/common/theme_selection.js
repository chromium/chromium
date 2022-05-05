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
    [OobeDialogHostBehavior, OobeI18nBehavior, LoginScreenBehavior],
    Polymer.Element);

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
      selectedTheme: {
        type: String,
      }
    };
  }

  constructor() {
    super();
  }

  get EXTERNAL_API() {
    return [];
  }

  ready() {
    super.ready();
    this.initializeLoginScreen('ThemeSelectionScreen');
    this.selectedTheme = 'auto';
  }

  getOobeUIInitialState() {
    return OOBE_UI_STATE.THEME_SELECTION;
  }

  onNextClicked_() {
    this.userActed(UserAction.NEXT);
  }

  setDarkTheme_() {
    this.userActed([UserAction.SELECT, SelectedTheme.DARK]);
  }

  setLightTheme_() {
    this.userActed([UserAction.SELECT, SelectedTheme.LIGHT]);
  }

  setAutoTheme_() {
    this.userActed([UserAction.SELECT, SelectedTheme.AUTO]);
  }
}
customElements.define(ThemeSelectionScreen.is, ThemeSelectionScreen);
