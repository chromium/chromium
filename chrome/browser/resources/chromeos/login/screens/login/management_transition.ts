// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design management
 * transition screen.
 */

import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './management_transition.html.js';


enum ManagementTransitionUiState {
  PROGRESS = 'progress',
  ERROR = 'error',
}

/**
 * Possible transition types. Must be in the same order as
 * ArcSupervisionTransition enum values.
 */
enum ArcSupervisionTransition {
  NO_TRANSITION = 0,
  CHILD_TO_REGULAR = 1,
  REGULAR_TO_CHILD = 2,
  UNMANAGED_TO_MANAGED=  3,
}

const ManagementTransitionScreenBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

interface ManagementTransitionScreenData {
  arcTransition: ArcSupervisionTransition;
  managementEntity: string;
}

class ManagementTransitionScreen extends ManagementTransitionScreenBase {
  static get is() {
    return 'management-transition-element'as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Property that determines transition direction.
       */
      arcTransition: {
        type: Number,
        value: ArcSupervisionTransition.NO_TRANSITION,
      },

      /**
       * String that represents management entity for the user. Can be domain or
       * admin name.
       */
      managementEntity: {
        type: String,
        value: '',
      },
    };
  }

  private arcTransition: number;
  private managementEntity: string;

  constructor() {
    super();
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return ManagementTransitionUiState.PROGRESS;
  }

  override get UI_STEPS() {
    return ManagementTransitionUiState;
  }

  override get EXTERNAL_API(): string[] {
    return ['showStep'];
  }

  override ready() {
    super.ready();
    this.initializeLoginScreen('ManagementTransitionScreen');
  }

  override onBeforeShow(data: ManagementTransitionScreenData): void {
    super.onBeforeShow(data);
    this.setArcTransition(data['arcTransition']);
    this.setManagementEntity(data['managementEntity']);
  }

  /**
   * Switches between different steps.
   * @param step the steps to show
   */
  private showStep(step: string): void {
    this.setUIStep(step);
  }

  /**
   * Sets arc transition type.
   * @param arc_transition enum element indicating
   *     transition type
   */
  private setArcTransition(arcTransition: ArcSupervisionTransition): void {
    switch (arcTransition) {
      case ArcSupervisionTransition.CHILD_TO_REGULAR:
      case ArcSupervisionTransition.REGULAR_TO_CHILD:
      case ArcSupervisionTransition.UNMANAGED_TO_MANAGED:
        this.arcTransition = arcTransition;
        break;
      case ArcSupervisionTransition.NO_TRANSITION:
        console.error(
            'Screen should not appear for ' +
            'ARC_SUPERIVISION_TRANSITION.NO_TRANSITION');
        break;
      default:
        console.error('Not handled transition type: ' + arcTransition);
    }
  }

  private setManagementEntity(managementEntity: string): void {
    this.managementEntity = managementEntity;
  }

  private getDialogTitle(locale: string, arcTransition: number,
      managementEntity: string): string {
    switch (arcTransition) {
      case ArcSupervisionTransition.CHILD_TO_REGULAR:
        return this.i18nDynamic(locale, 'removingSupervisionTitle');
      case ArcSupervisionTransition.REGULAR_TO_CHILD:
        return this.i18nDynamic(locale, 'addingSupervisionTitle');
      case ArcSupervisionTransition.UNMANAGED_TO_MANAGED:
        if (managementEntity) {
          return this.i18nDynamic(locale, 'addingManagementTitle',
              managementEntity);
        } else {
          return this.i18nDynamic(locale, 'addingManagementTitleUnknownAdmin');
        }
    }
    return '';
  }

  private isChildTransition(arcTransition: number): boolean {
    return arcTransition !== ArcSupervisionTransition.UNMANAGED_TO_MANAGED;
  }

  /**
   * On-tap event handler for OK button.
   */
  private onAcceptAndContinue(): void {
    this.userActed(['finish-management-transition']);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [ManagementTransitionScreen.is]: ManagementTransitionScreen;
  }
}

customElements.define(
    ManagementTransitionScreen.is, ManagementTransitionScreen);
