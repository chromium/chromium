// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './strings.m.js';
import './managed_user_profile_notice_disclosure.js';
import './managed_user_profile_notice_value_prop.js';
import './managed_user_profile_notice_state.js';
import './managed_user_profile_notice_data_handling.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_app.css.js';
import {getHtml} from './managed_user_profile_notice_app.html.js';
import type {ManagedUserProfileInfo, ManagedUserProfileNoticeBrowserProxy} from './managed_user_profile_notice_browser_proxy.js';
import {BrowsingDataHandling, ManagedUserProfileNoticeBrowserProxyImpl, State} from './managed_user_profile_notice_browser_proxy.js';

document.addEventListener('DOMContentLoaded', () => {
  const managedUserProfileNoticeBrowserProxyImpl =
      ManagedUserProfileNoticeBrowserProxyImpl.getInstance();
  // Prefer using |document.body.offsetHeight| instead of
  // |document.body.scrollHeight| as it returns the correct height of the
  // page even when the page zoom in Chrome is different than 100%.
  managedUserProfileNoticeBrowserProxyImpl.initializedWithSize(
      document.body.offsetHeight);
  // The web dialog size has been initialized, so reset the body width to
  // auto. This makes sure that the body only takes up the viewable width,
  // e.g. when there is a scrollbar.
  document.body.style.width = 'auto';
});

const ManagedUserProfileNoticeAppElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ManagedUserProfileNoticeAppElement extends
    ManagedUserProfileNoticeAppElementBase {
  static get is() {
    return 'managed-user-profile-notice-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showEnterpriseBadge_: {
        type: Boolean,
      },

      /** URL for the profile picture */
      pictureUrl_: {type: String},

      /** The title and subtitle of the screen */
      title_: {type: String},
      subtitle_: {type: String},

      /** The detailed info about enterprise management */
      enterpriseInfo_: {type: String},

      /**
       * Whether this page is being shown as a dialog.
       *
       * Reflected as an attribute to allow configuring variables and styles at
       * the element host level.
       */
      isModalDialog_: {
        type: Boolean,
        reflect: true,
      },

      /** The label for the button to proceed with the flow */
      continueAs_: {type: String},
      proceedLabel_: {type: String},
      cancelLabel_: {type: String},

      /** Whether to show the cancel button on the screen */
      showCancelButton_: {type: Boolean},

      disableProceedButton_: {type: Boolean},
      currentState_: {type: Number},
      showDisclosure_: {type: Boolean},
      showProcessing_: {type: Boolean},
      showSuccess_: {type: Boolean},
      showTimeout_: {type: Boolean},
      showError_: {type: Boolean},

      processingSubtitle_: {type: String},

      showUserDataHandling_: {type: Boolean},

      useUpdatedUi_: {
        type: Boolean,
        reflect: true,
      },

      selectedDataHandling_: {type: String},
    };
  }

  protected email_: string;
  protected accountName_: string;
  private continueAs_: string;
  protected showEnterpriseBadge_: boolean = false;
  protected pictureUrl_: string;
  protected title_: string;
  protected subtitle_: string;
  private enterpriseInfo_: string;
  protected isModalDialog_: boolean = loadTimeData.getBoolean('isModalDialog');
  protected proceedLabel_: string;
  protected cancelLabel_: string;
  protected disableProceedButton_: boolean = false;
  private showCancelButton_: boolean = true;
  private currentState_: State = State.DISCLOSURE;
  protected showValueProposition_: boolean = false;
  protected showDisclosure_: boolean = false;
  protected showProcessing_: boolean = false;
  protected showSuccess_: boolean = false;
  protected showTimeout_: boolean = false;
  protected showError_: boolean = false;
  protected useUpdatedUi_: boolean = loadTimeData.getBoolean('useUpdatedUi');
  protected processingSubtitle_: string =
      loadTimeData.getString('processingSubtitle');
  protected showUserDataHandling_: boolean = false;
  protected selectedDataHandling_: BrowsingDataHandling;
  private managedUserProfileNoticeBrowserProxy_:
      ManagedUserProfileNoticeBrowserProxy =
          ManagedUserProfileNoticeBrowserProxyImpl.getInstance();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentState_')) {
      this.cancelLabel_ = this.computeCancelLabel_();
    }

    if (changedPrivateProperties.has('currentState_') ||
        changedPrivateProperties.has('continueAs_')) {
      this.proceedLabel_ = this.computeProceedLabel_();
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.addWebUiListener(
        'on-state-changed', (state: State) => this.updateCurrentState_(state));

    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ManagedUserProfileInfo) => this.setProfileInfo_(info));

    this.addWebUiListener(
        'on-long-processing', () => this.updateProcessingText_());

    this.managedUserProfileNoticeBrowserProxy_.initialized().then(info => {
      this.setProfileInfo_(info);
      this.updateCurrentState_(loadTimeData.getInteger('initialState'));
    });
  }

  /** Called when the proceed button is clicked. */
  protected onProceed_() {
    this.disableProceedButton_ = true;
    const linkData = this.selectedDataHandling_ === BrowsingDataHandling.MERGE;
    this.managedUserProfileNoticeBrowserProxy_.proceed(
        /*currentState=*/ this.currentState_, linkData);
  }

  /** Called when the cancel button is clicked. */
  protected onCancel_() {
    this.managedUserProfileNoticeBrowserProxy_.cancel();
  }

  private setProfileInfo_(info: ManagedUserProfileInfo) {
    this.pictureUrl_ = info.pictureUrl;
    this.email_ = info.email;
    this.accountName_ = info.accountName;
    this.continueAs_ = info.continueAs;
    this.showEnterpriseBadge_ = info.showEnterpriseBadge;
    this.title_ = info.title;
    this.subtitle_ = info.subtitle;
    this.enterpriseInfo_ = info.enterpriseInfo;
    this.selectedDataHandling_ = info.checkLinkDataCheckboxByDefault ?
        BrowsingDataHandling.MERGE :
        BrowsingDataHandling.SEPARATE;
  }

  private updateCurrentState_(state: State) {
    this.currentState_ = state;
    this.showValueProposition_ = state === State.VALUE_PROPOSITION;
    this.showDisclosure_ = state === State.DISCLOSURE;
    this.showProcessing_ = state === State.PROCESSING;
    this.showSuccess_ = state === State.SUCCESS;
    this.showTimeout_ = state === State.TIMEOUT;
    this.showError_ = state === State.ERROR;
    this.showUserDataHandling_ = state === State.USER_DATA_HANDLING;
    this.disableProceedButton_ = false;
  }

  protected allowCancel_() {
    return this.showDisclosure_ || this.showValueProposition_ ||
        this.showUserDataHandling_;
  }

  private computeCancelLabel_() {
    return this.currentState_ === State.VALUE_PROPOSITION &&
            !loadTimeData.getBoolean('enforcedByPolicy') ?
        this.i18n('cancelValueProp') :
        this.i18n('cancelLabel');
  }

  protected allowProceedButton_() {
    return !this.disableProceedButton_ &&
        (!this.showUserDataHandling_ || !!this.selectedDataHandling_);
  }

  private computeProceedLabel_() {
    switch (this.currentState_) {
      case State.VALUE_PROPOSITION:
        return this.continueAs_;
      case State.DISCLOSURE:
      case State.PROCESSING:
        return this.i18n('continueLabel');
      case State.USER_DATA_HANDLING:
      case State.TIMEOUT:
      case State.SUCCESS:
      case State.ERROR:
        return this.i18n('confirmLabel');
    }
  }

  private updateProcessingText_() {
    this.processingSubtitle_ = this.i18n('longProcessingSubtitle');
  }

  protected onDataHandlingChanged_(
      e: CustomEvent<{value: BrowsingDataHandling}>) {
    this.selectedDataHandling_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-app': ManagedUserProfileNoticeAppElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeAppElement.is, ManagedUserProfileNoticeAppElement);
