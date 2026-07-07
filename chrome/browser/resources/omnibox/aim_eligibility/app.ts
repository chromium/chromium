// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DisclaimerState} from './aim_eligibility.mojom-webui.js';
import type {DriveStatus, EligibilityState} from './aim_eligibility.mojom-webui.js';
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
    isThirdPartyEligibleByPolicy: false,
    isEligibleByServer: false,
    isServerEligibilityEnabled: false,
    lastUpdated: new Date(0),
    eligibilityResponseBase64Encoded: '',
    eligibilityResponseSource: '',
    eligibilityResponseAuthType: null,
    searchboxConfigBase64UrlEncoded: '',
    driveStatus: null,
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
    this.listenerIds_.push(
        this.callbackRouter_.onDriveStatusChanged.addListener(
            this.onDriveStatusChanged_.bind(this)));

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
      eligibilityResponseBase64Encoded: (e.target as HTMLTextAreaElement).value,
    };
    this.inputState_ = InputState.NONE;
  }

  protected onServerRequestClick_() {
    this.pageHandler_.requestServerEligibilityForDebugging();
  }

  protected onViewResponseClick_() {
    this.openWindowProxy_.openUrl(this.getProtoshopUrl_(
        this.eligibilityState_.eligibilityResponseBase64Encoded));
  }

  protected onDraftResponseClick_() {
    this.openWindowProxy_.openUrl(this.getProtoshopUrl_(''));
  }

  protected onViewSearchboxConfigClick_() {
    this.openWindowProxy_.openUrl(`http://go/aim-pec-api-demo?config=${
        this.eligibilityState_.searchboxConfigBase64UrlEncoded}`);
  }

  protected async onSaveResponseClick_() {
    const result = await this.pageHandler_.setEligibilityResponseForDebugging(
        this.eligibilityState_.eligibilityResponseBase64Encoded);
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

  protected getThirdPartyPolicyEligibilityText_(): string {
    return this.eligibilityState_.isThirdPartyEligibleByPolicy ? '✓ Allowed' :
                                                                 '✗ Blocked';
  }

  protected getDseEligibilityText_(): string {
    return this.eligibilityState_.isEligibleByDse ? '✓ Google' : '✗ Not Google';
  }

  protected getServerEligibilityText_(): string {
    return this.eligibilityState_.isEligibleByServer ? '✓ Eligible' :
                                                       '✗ Not Eligible';
  }

  protected getDriveSupportedText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isDriveSupported ?
        '✓ Drive Supported' :
        '✗ Drive Not Supported';
  }

  protected getPecEligibleText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isPecEligible ? '✓ Yes' : '✗ No';
  }

  protected getIdentityMatchText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isIdentityMatch ? '✓ Match' :
                                                                '✗ No Match';
  }

  protected getIncognitoText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isIncognito ? '✓ Yes' : '✗ No';
  }

  protected getFeatureFlagText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isFeatureFlagEnabled ?
        '✓ Enabled' :
        '✗ Disabled';
  }

  protected getForceDisclaimerText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isForceDriveDisclaimerAccepted ?
        '✓ Enabled' :
        '✗ Disabled';
  }

  protected getSearchSharingText_(): string {
    if (!this.eligibilityState_.driveStatus) {
      return '';
    }
    return this.eligibilityState_.driveStatus.isSearchContentSharingEnabled ?
        '✓ Enabled' :
        '✗ Disabled';
  }

  protected getDisclaimerAcceptedText_(): string {
    const status = this.eligibilityState_.driveStatus;
    if (!status) {
      return 'Loading...';
    }
    const forced = status.isForceDriveDisclaimerAccepted;
    switch (status.disclaimerState) {
      case DisclaimerState.kAccepted:
        return forced ? '✓ Accepted (Forced by flag)' : '✓ Accepted';
      case DisclaimerState.kNotAccepted:
        return '✗ Not Accepted';
      case DisclaimerState.kRestricted:
        return '✗ Restricted';
      default:
        return 'Unknown';
    }
  }

  protected getDisclaimerClass_(): CheckClass|'' {
    const status = this.eligibilityState_.driveStatus;
    if (!status) {
      return '';
    }
    switch (status.disclaimerState) {
      case DisclaimerState.kAccepted:
        return CheckClass.PASS;
      case DisclaimerState.kNotAccepted:
      case DisclaimerState.kRestricted:
        return CheckClass.FAIL;
      default:
        return CheckClass.FAIL;
    }
  }

  protected getLastUpdatedTimestamp_(): string {
    return this.eligibilityState_.lastUpdated.getTime() > 0 ?
        this.eligibilityState_.lastUpdated.toLocaleString() :
        '';
  }

  private onEligibilityStateChanged_(state: EligibilityState) {
    const oldDriveStatus = this.eligibilityState_.driveStatus;
    this.eligibilityState_ = state;
    if (!this.eligibilityState_.driveStatus && oldDriveStatus) {
      this.eligibilityState_.driveStatus = oldDriveStatus;
    }
    this.inputState_ = InputState.NONE;
  }

  private onDriveStatusChanged_(status: DriveStatus) {
    this.eligibilityState_ = {
      ...this.eligibilityState_,
      driveStatus: status,
    };
  }

  private getProtoshopUrl_(base64Proto: string): string {
    const protoType = 'gws.searchbox.chrome.AimEligibilityResponse';
    if (!base64Proto) {
      return `http://protoshop/${protoType}`;
    }
    return `http://protoshop/embed?tabs=textproto&type=${
        protoType}&protobytes=${encodeURIComponent(base64Proto)}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'aim-eligibility-app': AimEligibilityAppElement;
  }
}

customElements.define(AimEligibilityAppElement.is, AimEligibilityAppElement);
