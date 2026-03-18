// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../components/buttons/oobe_text_button.js';

import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {FjordImageSelectionPageHandler_FjordImageType, FjordImageSelectionPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './fjord_image_selection.html.js';

export const FjordImageSelectionScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class FjordImageSelectionScreen extends
    FjordImageSelectionScreenElementBase {
  static get is() {
    return 'fjord-image-selection-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      selectedImageType: {
        type: String,
        value: '',
      },
    };
  }

  private selectedImageType: string;
  private handler: FjordImageSelectionPageHandlerRemote;

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FjordImageSelectionScreen');
    this.handler = new FjordImageSelectionPageHandlerRemote();
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory.establishFjordImageSelectionScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver());
  }

  private onNextButtonClicked(): void {
    let imageType: FjordImageSelectionPageHandler_FjordImageType;
    if (this.selectedImageType === 'zoom') {
      imageType = FjordImageSelectionPageHandler_FjordImageType.kSquid;
    } else if (this.selectedImageType === 'meet') {
      imageType = FjordImageSelectionPageHandler_FjordImageType.kCuttlefish;
    } else {
      console.error('Unexpected selection: ' + this.selectedImageType);
      return;
    }
    this.handler.onImageSelected(imageType);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FjordImageSelectionScreen.is]: FjordImageSelectionScreen;
  }
}

customElements.define(FjordImageSelectionScreen.is, FjordImageSelectionScreen);
