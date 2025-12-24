// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';

import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {EligibilityState} from './aim_eligibility.mojom-webui.js';
import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import {BrowserProxy} from './browser_proxy.js';

// Input states.
enum InputState {
  NONE = '',
  FAIL = 'fail',
}

// Check result classes.
enum CheckClass {
  PASS = 'pass',
  FAIL = 'fail',
}

export class AimEligibilityAppElement extends CrLitElement {
  static get is() {
    return 'aim-eligibility-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      eligibilityState_: {type: Object},
      inputState_: {type: String},
    };
  }

  protected accessor eligibilityState_: EligibilityState = {
    isEligible: false,
    isEligibleByDse: false,
    isEligibleByPolicy: false,
    isEligibleByServer: false,
    isServerEligibilityEnabled: false,
    lastUpdated: new Date(0),
    serverResponseBase64Encoded: '',
    serverResponseBase64UrlEncoded: '',
    serverResponseSource: '',
  };
  protected accessor inputState_: InputState = InputState.NONE;

  private callbackRouter_ = BrowserProxy.getInstance().getCallbackRouter();
  private listenerIds_: number[] = [];
  private openWindowProxy_ = OpenWindowProxyImpl.getInstance();
  private pageHandler_ = BrowserProxy.getInstance().getPageHandler();

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_.push(
        this.callbackRouter_.onEligibilityStateChanged.addListener(
            this.onEligibilityStateChanged_.bind(this)));

    this.pageHandler_.getEligibilityState().then(
        ({state}) => this.onEligibilityStateChanged_(state));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    for (const listenerId of this.listenerIds_) {
      this.callbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
  }

  protected onResponseInput_(e: Event) {
    this.eligibilityState_ = {
      ...this.eligibilityState_,
      serverResponseBase64Encoded: (e.target as HTMLTextAreaElement).value,
    };
    this.inputState_ = InputState.NONE;
  }

  protected onServerRequestClick_() {
    this.pageHandler_.requestServerEligibilityForDebugging();
  }

  protected onViewResponseClick_() {
    this.openWindowProxy_.openUrl(this.getProtoshopUrl_(
        this.eligibilityState_.serverResponseBase64UrlEncoded));
  }

  protected onDraftResponseClick_() {
    this.openWindowProxy_.openUrl(this.getProtoshopUrl_(''));
  }

  protected async onSaveResponseClick_() {
    const result = await this.pageHandler_.setEligibilityResponseForDebugging(
        this.eligibilityState_.serverResponseBase64Encoded);
    this.inputState_ = result.success ? InputState.NONE : InputState.FAIL;
  }

  protected getCheckClass_(isPass: boolean): CheckClass {
    return isPass ? CheckClass.PASS : CheckClass.FAIL;
  }

  protected getEligibilityText_(): string {
    return this.eligibilityState_.isEligible ? '✓ Eligible' : '✗ Not Eligible';
  }

  protected getPolicyEligibilityText_(): string {
    return this.eligibilityState_.isEligibleByPolicy ? '✓ Allowed' :
                                                       '✗ Blocked';
  }

  protected getDseEligibilityText_(): string {
    return this.eligibilityState_.isEligibleByDse ? '✓ Google' : '✗ Not Google';
  }

  protected getServerEligibilityText_(): string {
    return this.eligibilityState_.isEligibleByServer ? '✓ Eligible' :
                                                       '✗ Not Eligible';
  }

  protected getLastUpdatedTimestamp_(): string {
    return this.eligibilityState_.lastUpdated.getTime() > 0 ?
        this.eligibilityState_.lastUpdated.toLocaleString() :
        '';
  }

  private onEligibilityStateChanged_(state: EligibilityState) {
    this.eligibilityState_ = state;
    this.inputState_ = InputState.NONE;
  }

  private getProtoshopUrl_(base64UrlProto: string): string {
    const protoType = 'gws.searchbox.chrome.AimEligibilityResponse';
    if (!base64UrlProto) {
      return `http://protoshop/${protoType}`;
    }
    return `http://protoshop/embed?tabs=textproto&type=${
        protoType}&protobytes=${base64UrlProto}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'aim-eligibility-app': AimEligibilityAppElement;
  }
}

customElements.define(AimEligibilityAppElement.is, AimEligibilityAppElement);
