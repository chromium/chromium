// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines a custom Polymer component for the main menu in the
 * tutorial.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Curriculum, InteractionMedium, MainMenuButtonData, Screen} from './constants.js';
import {Localization} from './localization.js';

export const MainMenu = Polymer({
  is: 'main-menu',

  _template: html`{__html_template__}`,

  behaviors: [Localization],

  properties: {
    /** @private {string} */
    header_: {type: String},
    /** @private {string} */
    headerDescription_: {type: String},
    /** @private  {Array<!MainMenuButtonData>} */
    buttonData_: {type: Array},

    // Observed properties.
    /** @type {Screen} */
    activeScreen: {type: String, observer: 'maybeFocusHeader_'},
  },

  /** @private */
  maybeFocusHeader_() {
    if (this.activeScreen === Screen.MAIN_MENU) {
      this.$.mainMenuHeader.focus();
    }
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
   * @param {InteractionMedium} buttonMedium
   * @param {InteractionMedium} activeMedium
   * @return {boolean}
   * @private
   */
  shouldHideButton_(buttonMedium, activeMedium) {
    return buttonMedium !== activeMedium;
  },

  /**
   * @param {!MouseEvent} evt
   * @private
   */
  onButtonClicked_(evt) {
    // Fires an event with button data attached to |evt.detail|.
    this.fire('button-clicked', evt.model.data);
  },
});
