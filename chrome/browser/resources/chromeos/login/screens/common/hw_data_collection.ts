// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for HW data collection screen.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/js/action_link.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';

import {getTemplate} from './hw_data_collection.html.js';


const HwDataCollectionScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

enum HwDataCollectionStep {
  OVERVIEW = 'overview',
}

interface HwDataCollectionScreenData {
  hwDataUsageEnabled: boolean;
}

export class HwDataCollectionScreen extends HwDataCollectionScreenElementBase {
  static get is() {
    return 'hw-data-collection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      dataUsageChecked: {
        type: Boolean,
        value: false,
      },
    };
  }

  private dataUsageChecked: boolean;

  override get UI_STEPS() {
    return HwDataCollectionStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): HwDataCollectionStep {
    return HwDataCollectionStep.OVERVIEW;
  }

  constructor() {
    super();
  }

  /**
   * Event handler that is invoked just before the screen is shown.
   * param data Screen init payload
   */
  override onBeforeShow(data: HwDataCollectionScreenData): void {
    super.onBeforeShow(data);
    this.dataUsageChecked =
        'hwDataUsageEnabled' in data && data.hwDataUsageEnabled;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('HWDataCollectionScreen');
  }

  private getHwDataCollectionContent_(locale: string): TrustedHTML {
    return this.i18nAdvancedDynamic(
        locale, 'HWDataCollectionContent', {tags: ['p']});
  }

  private onAcceptButtonClicked_(): void {
    this.userActed('accept-button');
  }

  /**
   * On-change event handler for dataUsageChecked.
   */
  private onDataUsageChanged_(): void {
    this.userActed(['select-hw-data-usage', this.dataUsageChecked]);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [HwDataCollectionScreen.is]: HwDataCollectionScreen;
  }
}

customElements.define(HwDataCollectionScreen.is, HwDataCollectionScreen);
