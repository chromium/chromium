// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Polymer element for categoreis selection screen.
 */

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/dialogs/oobe_loading_dialog.js';
import '../../components/oobe_categories_list.js';
import '../../components/oobe_cr_lottie.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_categories_list.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';
import '../../components/dialogs/oobe_loading_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '//resources/js/assert.js';
import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeDialogHostBehavior, OobeDialogHostBehaviorInterface} from '../../components/behaviors/oobe_dialog_host_behavior.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {OobeI18nMixin, OobeI18nMixinInterface} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCategoriesList, OobeCategoriesListData} from '../../components/oobe_categories_list.js';

import {getTemplate} from './categories_selection.html.js';

export const CategoriesScreenElementBase =
    mixinBehaviors(
        [LoginScreenBehavior, OobeDialogHostBehavior, MultiStepBehavior],
        OobeI18nMixin(PolymerElement)) as {
      new (): PolymerElement & OobeI18nMixinInterface &
          LoginScreenBehaviorInterface & OobeDialogHostBehaviorInterface &
          MultiStepBehaviorInterface,
    };

enum CaegoriesStep {
  LOADING = 'loading',
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  SKIP = 'skip',
  NEXT = 'next',
}

interface CategoriesScreenData {
  categories: OobeCategoriesListData;
}

export class CategoriesScreenElement extends CategoriesScreenElementBase {
  static get is() {
    return 'categories-selection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      numberOfSelectedCategories: {
        type: Number,
        value: 0,
      },
    };
  }

  private numberOfSelectedCategories: number;

  override get UI_STEPS() {
    return CaegoriesStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return CaegoriesStep.LOADING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('CategoriesSelectionScreen');
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setCategoriesData',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  setCategoriesData(categoriesData: CategoriesScreenData): void {
    assert('categories' in categoriesData);
    this.shadowRoot!.querySelector<OobeCategoriesList>('#categoriesList')!
          .init(categoriesData['categories']);
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    const categoriesDialog =
        this.shadowRoot?.querySelector<HTMLElement>('#categoriesDialog');
    if (categoriesDialog instanceof HTMLElement) {
      return categoriesDialog;
    }
    return null;
  }

  /**
   * Handles event when contents in the webview is generated.
   */
  private onFullyLoaded(): void {
    this.setUIStep(CaegoriesStep.OVERVIEW);
    const categoriesList =
        this.shadowRoot?.querySelector<HTMLElement>('#categoriesList');
    if (categoriesList instanceof HTMLElement) {
      categoriesList.focus();
    }
  }

  private onNextClicked(): void {
    const categoriesSelected =
        this.shadowRoot!.querySelector<OobeCategoriesList>(
                            '#categoriesList')!.getCategoriesSelected();
    this.userActed([UserAction.NEXT, categoriesSelected]);
  }

  private onSkip(): void {
    this.userActed(UserAction.SKIP);
  }

  private canProceed(): boolean {
    return this.numberOfSelectedCategories > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CategoriesScreenElement.is]: CategoriesScreenElement;
  }
}

customElements.define(CategoriesScreenElement.is, CategoriesScreenElement);
