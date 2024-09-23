// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_radio_button/cr_card_radio_button.js';
import '//resources/ash/common/cr_elements/cr_radio_group/cr_radio_group.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/cr_card_radio_group_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';
import '../../components/oobe_cr_lottie.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {afterNextRender, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {OobeUiState} from '../../components/display_manager_types.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {MultiStepMixin} from '../../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import type {OobeCrLottie} from '../../components/oobe_cr_lottie.js';
import {GaiaInfoPageCallbackRouter, GaiaInfoPageHandler_UserCreationFlowType, GaiaInfoPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './gaia_info.html.js';

export const GaiaInfoScreenElementBase =
    LoginScreenMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));


enum GaiaInfoStep {
  OVERVIEW = 'overview',
}


enum UserCreationFlowType {
  MANUAL = 'manual',
  QUICKSTART = 'quickstart',
}


export class GaiaInfoScreen extends GaiaInfoScreenElementBase {
  static get is() {
    return 'gaia-info-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * The currently selected flow type.
       */
      selectedFlowType: {
        type: String,
        value: '',
      },
      /**
       * Whether Quick start feature is enabled. If it's enabled the quick start
       * button will be shown in the gaia info screen.
       */
      isQuickStartVisible: {
        type: Boolean,
        value: false,
      },
    };
  }

  private selectedFlowType: string;
  private isQuickStartVisible: boolean;
  private callbackRouter: GaiaInfoPageCallbackRouter;
  private handler: GaiaInfoPageHandlerRemote;

  constructor() {
    super();
    this.callbackRouter = new GaiaInfoPageCallbackRouter();
    this.handler = new GaiaInfoPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishGaiaInfoScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });
    this.callbackRouter.setQuickStartVisible.addListener(() => {
      this.setQuickStartVisible();
    });
  }

  override get UI_STEPS() {
    return GaiaInfoStep;
  }

  override onBeforeShow(): void {
    super.onBeforeShow();
    this.selectedFlowType = '';
    this.setAnimationPlaying(true);
  }

  override onBeforeHide(): void {
    super.onBeforeHide();
    this.setAnimationPlaying(false);
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): GaiaInfoStep {
    return GaiaInfoStep.OVERVIEW;
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GaiaInfoScreen');
  }

  setQuickStartVisible(): void {
    this.isQuickStartVisible = true;
    afterNextRender(this, () => {
      const dialog = this.shadowRoot?.querySelector('#gaiaInfoDialog');
      if (!this.hidden && dialog instanceof OobeAdaptiveDialog){
        dialog.focus();
      }
    });
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override getOobeUIInitialState(): OobeUiState {
    return OobeUiState.GAIA_INFO;
  }

  private onNextClicked(): void {
    if (this.isQuickStartVisible &&
        this.selectedFlowType === UserCreationFlowType.QUICKSTART) {
      this.handler.onNextClicked(
          GaiaInfoPageHandler_UserCreationFlowType.kQuickstart);
    } else {
      this.handler.onNextClicked(
          GaiaInfoPageHandler_UserCreationFlowType.kManual);
    }
  }

  private onBackClicked(): void {
    this.handler.onBackClicked();
  }

  private isNextButtonEnabled(
      isQuickStartVisible: boolean, selectedFlowType: string): boolean {
    return (!isQuickStartVisible) || selectedFlowType !== '';
  }

  private setAnimationPlaying(play: boolean): void {
    const gaiaInfoAnimation =
        this.shadowRoot!.querySelector<OobeCrLottie>('#gaiaInfoAnimation');
    if (gaiaInfoAnimation) {
      gaiaInfoAnimation.playing = play;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GaiaInfoScreen.is]: GaiaInfoScreen;
  }
}

customElements.define(GaiaInfoScreen.is, GaiaInfoScreen);
