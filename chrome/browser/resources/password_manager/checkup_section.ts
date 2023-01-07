// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_section.html.js';
import {PasswordCheckInteraction, PasswordCheckStatusChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

export interface CheckupSectionElement {
  $: {
    checkupResult: HTMLAnchorElement,
    lastCheckupTime: HTMLAnchorElement,
    refreshButton: CrIconButtonElement,
    retryButton: CrButtonElement,
    spinner: PaperSpinnerLiteElement,
  };
}

export class CheckupSectionElement extends PolymerElement {
  static get is() {
    return 'checkup-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The number of checked passwords as a formatted string.
       */
      checkedPasswordsText_: String,

      /**
       * The number of compromised passwords as a formatted string.
       */
      compromisedPasswordsText_: String,

      /**
       * The number of weak passwords as a formatted string.
       */
      reusedPasswordsText_: String,

      /**
       * The number of weak passwords as a formatted string.
       */
      weakPasswordsText_: String,

      /**
       * The status indicates progress and affects banner, title and icon.
       */
      status_: {
        type: Object,
        value: () => ({state: chrome.passwordsPrivate.PasswordCheckState.IDLE}),
      },

      isCheckRunning_: {
        type: Boolean,
        computed: 'computeIsCheckRunning_(status_)',
      },

      isCheckSuccessful_: {
        type: Boolean,
        computed: 'computeIsCheckSuccessful_(status_)',
      },
    };
  }

  private checkedPasswordsText_: string;
  private compromisedPasswordsText_: string;
  private reusedPasswordsText_: string;
  private weakPasswordsText_: string;
  private status_: chrome.passwordsPrivate.PasswordCheckStatus;

  private statusChangedListener_: PasswordCheckStatusChangedListener|null =
      null;

  override ready() {
    super.ready();
    this.fetchPluralizedStrings_();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.statusChangedListener_ = status => {
      this.status_ = status;
    };

    PasswordManagerImpl.getInstance().getPasswordCheckStatus().then(
        this.statusChangedListener_);

    PasswordManagerImpl.getInstance().addPasswordCheckStatusListener(
        this.statusChangedListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.statusChangedListener_);
    PasswordManagerImpl.getInstance().removePasswordCheckStatusListener(
        this.statusChangedListener_);
    this.statusChangedListener_ = null;
  }

  private fetchPluralizedStrings_() {
    const proxy = PluralStringProxyImpl.getInstance();

    proxy.getPluralString('checkedPasswords', 6)
        .then(result => this.checkedPasswordsText_ = result);

    proxy.getPluralString('compromisedPasswords', 2)
        .then(result => this.compromisedPasswordsText_ = result);

    proxy.getPluralString('reusedPasswords', 0)
        .then(result => this.reusedPasswordsText_ = result);

    proxy.getPluralString('weakPasswords', 4)
        .then(result => this.weakPasswordsText_ = result);
  }

  /**
   * @return true iff a check is running right according to the given |status|.
   */
  private computeIsCheckRunning_(): boolean {
    return this.status_.state === CheckState.RUNNING;
  }

  private computeIsCheckSuccessful_(): boolean {
    return this.status_.state === CheckState.IDLE;
  }

  private showRetryButton_(): boolean {
    return !this.computeIsCheckRunning_() && !this.computeIsCheckSuccessful_();
  }

  private showCheckButton_(): boolean {
    return this.status_.state !== CheckState.NO_PASSWORDS &&
        this.status_.state !== CheckState.QUOTA_LIMIT;
  }

  /**
   * Starts/Restarts bulk password check.
   */
  private onPasswordCheckButtonClick_() {
    PasswordManagerImpl.getInstance().startBulkPasswordCheck();
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.START_CHECK_MANUALLY);
  }

  private getBannerImageFileName_(): string {
    if (this.computeIsCheckRunning_()) {
      return 'checkup_result_banner_running';
    }
    if (this.computeIsCheckSuccessful_()) {
      // TODO(crbug.com/1350947): Show either OK state or Compromised state
      // depnding on presence of issues.
      return 'checkup_result_banner_compromised';
    }
    return 'checkup_result_banner_error';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-section': CheckupSectionElement;
  }
}

customElements.define(CheckupSectionElement.is, CheckupSectionElement);
