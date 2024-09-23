// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_illo_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
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
import {GeminiIntroPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './gemini_intro.html.js';

export const GeminiIntroScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

/**
 * Data that is passed to the screen during onBeforeShow.
 */
interface GeminiIntroScreenData {
  backButtonVisible: boolean;
}

export class GeminiIntroScreen extends GeminiIntroScreenElementBase {
  static get is() {
    return 'gemini-intro-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      backButtonVisible: {
        type: Boolean,
        value: false,
      },
    };
  }

  private backButtonVisible: boolean;
  private handler: GeminiIntroPageHandlerRemote;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('GeminiIntro');
    this.handler = new GeminiIntroPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishGeminiIntroScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());
  }

  override get defaultControl(): HTMLElement {
    const dialog =  this.shadowRoot?.querySelector('#geminiIntroDialog');
    assertInstanceof(dialog, OobeAdaptiveDialog);
    return dialog;
  }

  override onBeforeShow(data: GeminiIntroScreenData): void {
    super.onBeforeShow(data);
    this.backButtonVisible = data['backButtonVisible'];
  }

  private onBackClicked(): void {
    this.handler.onBackClicked();
  }

  private onNextClicked(): void {
    this.handler.onNextClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [GeminiIntroScreen.is]: GeminiIntroScreen;
  }
}

customElements.define(GeminiIntroScreen.is, GeminiIntroScreen);
