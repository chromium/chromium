// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for theme selection screen.
 */
import '//resources/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {html, mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OOBE_UI_STATE} from '../../components/display_manager_types.js';


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {LoginScreenBehaviorInterface}
 * @implements {OobeI18nBehaviorInterface}
 * @implements {MultiStepBehaviorInterface}
 */
const ThemeSelectionScreenElementBase = mixinBehaviors(
    [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior], PolymerElement);

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

  static get template() {
    return html`{__html_template__}`;
  }

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

  get UI_STEPS() {
    return ThemeSelectionStep;
  }

  defaultUIStep() {
    return ThemeSelectionStep.OVERVIEW;
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
  }

  onBeforeShow(data) {
    if ('selectedTheme' in data) {
      this.selectedTheme = data.selectedTheme;
    }
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
