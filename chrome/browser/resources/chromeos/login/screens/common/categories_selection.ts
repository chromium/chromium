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

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {OobeCategoriesList, OobeCategoriesListData} from '../../components/oobe_categories_list.js';

import {getTemplate} from './categories_selection.html.js';

export const CategoriesScreenElementBase = OobeDialogHostMixin(
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement))));

enum CategoriesStep {
  LOADING = 'loading',
  OVERVIEW = 'overview',
}

/**
 * Available user actions.
 */
enum UserAction {
  SKIP = 'skip',
  NEXT = 'next',
  LOADED = 'loaded',
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
    return CategoriesStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep() {
    return CategoriesStep.LOADING;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('CategoriesSelectionScreen');
  }

  override get EXTERNAL_API(): string[] {
    return [
      'setCategoriesData',
      'setOverviewStep',
    ];
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.ONBOARDING;
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.setUIStep(CategoriesStep.LOADING);
  }


  override onBeforeHide(): void {
    super.onBeforeHide();
    this.shadowRoot!.querySelector<OobeCategoriesList>(
                        '#categoriesList')!.reset();
  }

  setCategoriesData(categoriesData: CategoriesScreenData): void {
    assert('categories' in categoriesData);
    this.shadowRoot!.querySelector<OobeCategoriesList>('#categoriesList')!
          .init(categoriesData['categories']);
  }

  setOverviewStep(): void {
    this.setUIStep(CategoriesStep.OVERVIEW);
    const categoriesList =
        this.shadowRoot?.querySelector<HTMLElement>('#categoriesList');
    if (categoriesList instanceof HTMLElement) {
      categoriesList.focus();
    }
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
    this.userActed(UserAction.LOADED);
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
