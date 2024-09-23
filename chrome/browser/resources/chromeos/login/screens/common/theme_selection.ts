// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for theme selection screen.
 */
import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/oobe_icons.html.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './theme_selection.html.js';

const ThemeSelectionScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface ThemeSelectionScreenData {
  selectedTheme: string;
  shouldShowReturn: boolean;
}

/**
 * Enum to represent steps on the theme selection screen.
 * Currently there is only one step, but we still use
 * MultiStepBehavior because it provides implementation of
 * things like processing 'focus-on-show' class
 */
enum ThemeSelectionStep {
  OVERVIEW = 'overview',
}

/**
 * Available themes. The values should be in sync with the enum
 * defined in theme_selection_screen.h
 */
enum SelectedTheme {
  AUTO = 0,
  DARK = 1,
  LIGHT = 2,
}

/**
 * Available user actions.
 */
enum UserAction {
  SELECT = 'select',
  NEXT = 'next',
  RETURN = 'return',
}

class ThemeSelectionScreen extends ThemeSelectionScreenElementBase {
  static get is() {
    return 'theme-selection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Indicates selected theme
       */
      selectedTheme: {type: String, value: 'auto', observer: 'onThemeChanged_'},

      /**
       * Whether the button to return to CHOOBE screen should be shown.
       */
      shouldShowReturn_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private selectedTheme: string;
  private shouldShowReturn_: boolean;

  override get UI_STEPS() {
    return ThemeSelectionStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): ThemeSelectionStep {
    return ThemeSelectionStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('ThemeSelectionScreen');
  }

  /**
   * @param data Screen init payload.
   */
  override onBeforeShow(data: ThemeSelectionScreenData): void {
    super.onBeforeShow(data);
    this.selectedTheme = data.selectedTheme!;
    this.shouldShowReturn_ = data['shouldShowReturn'];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.THEME_SELECTION;
  }

  private onNextClicked_(): void {
    this.userActed(UserAction.NEXT);
  }

  private onThemeChanged_(themeSelect:string, oldTheme?: string): void {
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

  private onReturnClicked_(): void {
    this.userActed(UserAction.RETURN);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ThemeSelectionScreen.is]: ThemeSelectionScreen;
  }
}

customElements.define(ThemeSelectionScreen.is, ThemeSelectionScreen);
