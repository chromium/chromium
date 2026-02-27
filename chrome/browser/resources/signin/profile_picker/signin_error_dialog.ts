// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './signin_error_dialog.css.js';
import {getHtml} from './signin_error_dialog.html.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import type {ManageProfilesBrowserProxy} from './manage_profiles_browser_proxy.js';

export interface SigninErrorDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const SigninErrorDialogElementBase = WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SigninErrorDialogElement extends SigninErrorDialogElementBase {
  static get is() {
    return 'signin-error-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      signinErrorDialogTitle_: {type: String},
      signinErrorDialogBody_: {type: String},
      shouldShowSigninButton_: {type: Boolean},
    };
  }

  protected accessor signinErrorDialogTitle_: string = '';
  protected accessor signinErrorDialogBody_: string = '';
  protected accessor shouldShowSigninButton_: boolean = false;
  private forceSigninErrorProfilePath_: string = '';
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'display-signin-error-dialog',
        (title: string, body: string, profilePath: string) =>
            this.show(title, body, profilePath));
  }

  show(title: string, body: string, profilePath: string) {
    this.signinErrorDialogTitle_ = title;
    this.signinErrorDialogBody_ = body;
    this.forceSigninErrorProfilePath_ = profilePath;
    this.shouldShowSigninButton_ = profilePath.length !== 0;
    this.$.dialog.showModal();
  }

  protected onOkButtonClick_() {
    this.$.dialog.close();
    this.clearErrorDialogInfo_();
  }

  protected onReauthClick_() {
    this.$.dialog.close();
    this.manageProfilesBrowserProxy_.launchSelectedProfile(
        this.forceSigninErrorProfilePath_);
    this.clearErrorDialogInfo_();
  }

  private clearErrorDialogInfo_(): void {
    this.signinErrorDialogTitle_ = '';
    this.signinErrorDialogBody_ = '';
    this.forceSigninErrorProfilePath_ = '';
    this.shouldShowSigninButton_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'signin-error-dialog': SigninErrorDialogElement;
  }
}

customElements.define(SigninErrorDialogElement.is, SigninErrorDialogElement);
