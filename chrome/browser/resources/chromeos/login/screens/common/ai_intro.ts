// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_carousel.js';
import '../../components/oobe_icons.html.js';
import '../../components/oobe_slide.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {assertInstanceof} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {AiIntroPageCallbackRouter, AiIntroPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './ai_intro.html.js';

const AiIntroScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class AiIntroScreen extends AiIntroScreenElementBase {
  static get is() {
    return 'ai-intro-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether auto-transition on carousel is enabled or not.
       */
      autoTransition: {
        type: Boolean,
        value: false,
      },
    };
  }

  private autoTransition: boolean;
  private callbackRouter: AiIntroPageCallbackRouter;
  private handler: AiIntroPageHandlerRemote;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('AiIntro');
    this.callbackRouter = new AiIntroPageCallbackRouter();
    this.handler = new AiIntroPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishAiIntroScreenPipe(this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });

    this.callbackRouter.setAutoTransition.addListener(
        this.setAutoTransition.bind(this));
  }

  override get defaultControl(): HTMLElement {
    const dialog =  this.shadowRoot?.querySelector('#aiIntroDialog');
    assertInstanceof(dialog, OobeAdaptiveDialog);
    return dialog;
  }

  /**
   * Sets whether carousel should auto transit slides.
   */
  setAutoTransition(value: boolean): void {
    this.autoTransition = value;
  }

  private onNextClicked(): void {
    this.handler.onNextClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [AiIntroScreen.is]: AiIntroScreen;
  }
}

customElements.define(AiIntroScreen.is, AiIntroScreen);
