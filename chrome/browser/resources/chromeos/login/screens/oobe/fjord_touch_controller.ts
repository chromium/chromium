// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import '../../components/buttons/oobe_text_button.js';

import {assert} from '//resources/js/assert.js';
import type {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenMixin} from '../../components/mixins/login_screen_mixin.js';
import {OobeDialogHostMixin} from '../../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../../components/mixins/oobe_i18n_mixin.js';
import {FjordTouchControllerPageCallbackRouter, FjordTouchControllerPageHandlerRemote} from '../../mojom-webui/screens_common.mojom-webui.js';
import {OobeScreensFactoryBrowserProxy} from '../../oobe_screens_factory_proxy.js';

import {getTemplate} from './fjord_touch_controller.html.js';

export const FjordTouchControllerScreenElementBase =
    OobeDialogHostMixin(LoginScreenMixin(OobeI18nMixin(PolymerElement)));

export class FjordTouchControllerScreen extends
    FjordTouchControllerScreenElementBase {
  static get is() {
    return 'fjord-touch-controller-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether the next button is disabled. It is enabled once the touch
       * controller signals that its setup is finished.
       */
      isNextButtonDisabled: {
        type: Boolean,
        value: true,
      },

      /**
       * Whether the test image skip button is visible. Only true when the
       * device is running a test image.
       */
      isTestImageSkipButtonVisible: {
        type: Boolean,
        value: false,
      },
    };
  }

  private declare isNextButtonDisabled: boolean;
  private declare isTestImageSkipButtonVisible: boolean;
  private webview: chrome.webviewTag.WebView|null = null;
  private handler = new FjordTouchControllerPageHandlerRemote();
  private callbackRouter = new FjordTouchControllerPageCallbackRouter();

  override onBeforeShow(): void {
    super.onBeforeShow();
    // Trigger a reload because at the time the dialog is created, the web
    // server is not up yet and will show an error page.
    assert(this.webview);
    this.webview.reload();
  }

  override ready(): void {
    super.ready();
    this.initializeLoginScreen('FjordTouchControllerScreen');
    this.webview =
        this.shadowRoot!.querySelector<chrome.webviewTag.WebView>('webview')!;
    OobeScreensFactoryBrowserProxy.getInstance()
        .screenFactory
        .establishFjordTouchControllerScreenPipe(
            this.handler.$.bindNewPipeAndPassReceiver())
        .then((response: any) => {
          this.callbackRouter.$.bindHandle(response.pending.handle);
        });
    this.callbackRouter.setNextButtonEnabled.addListener(
        this.setNextButtonEnabled.bind(this));
    this.callbackRouter.setTestImageSkipButtonVisible.addListener(
        this.setTestImageSkipButtonVisible.bind(this));
  }

  /**
   * Enables or disables the next button.
   */
  setNextButtonEnabled(enabled: boolean): void {
    this.isNextButtonDisabled = !enabled;
  }

  /**
   * Shows or hides the test image skip button.
   */
  setTestImageSkipButtonVisible(visible: boolean): void {
    this.isTestImageSkipButtonVisible = visible;
  }

  /**
   * Returns the control which should receive initial focus.
   */
  override get defaultControl(): HTMLElement|null {
    return this.shadowRoot!.querySelector('#primaryButton');
  }


  private onDoneButtonClicked(): void {
    if (this.isNextButtonDisabled) {
      return;
    }
    this.handler.onSetupComplete();
  }

  private onTestImageSkipButtonClicked(): void {
    this.handler.onTestImageSkipClicked();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FjordTouchControllerScreen.is]: FjordTouchControllerScreen;
  }
}

customElements.define(
    FjordTouchControllerScreen.is, FjordTouchControllerScreen);
