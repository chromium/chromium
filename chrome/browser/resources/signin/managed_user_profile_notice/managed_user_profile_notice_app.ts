// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './strings.m.js';
import './signin_shared.css.js';
import './signin_vars.css.js';
import './tangible_sync_style_shared.css.js';
import './managed_user_profile_notice_disclosure.js';
import './managed_user_profile_notice_state.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './managed_user_profile_notice_app.html.js';
import type {ManagedUserProfileInfo, ManagedUserProfileNoticeBrowserProxy} from './managed_user_profile_notice_browser_proxy.js';
import {ManagedUserProfileNoticeBrowserProxyImpl, State} from './managed_user_profile_notice_browser_proxy.js';

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
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class ManagedUserProfileNoticeAppElement extends
    ManagedUserProfileNoticeAppElementBase {
  static get is() {
    return 'managed-user-profile-notice-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showEnterpriseBadge_: {
        type: Boolean,
        value: false,
      },

      /** URL for the profile picture */
      pictureUrl_: String,

      /** The title and subtitle of the screen */
      title_: String,
      subtitle_: String,

      /** The detailed info about enterprise management */
      enterpriseInfo_: String,

      /**
       * Whether this page is being shown as a dialog.
       *
       * Reflected as an attribute to allow configuring variables and styles at
       * the element host level.
       */
      isModalDialog_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('isModalDialog');
        },
      },

      /** The label for the button to proceed with the flow */
      proceedLabel_: {
        type: String,
        computed: 'computeProceedLabel_(currentState_)',
      },

      /** Whether to show the cancel button on the screen */
      showCancelButton_: {
        type: Boolean,
        value: true,
      },

      disableProceedButton_: {
        type: Boolean,
        value: false,
      },

      currentState_: {
        type: Number,
        value: State.DISCLOSURE,
      },

      showDisclosure_: {
        type: Boolean,
        value: true,
      },

      showProcessing_: {
        type: Boolean,
        value: false,
      },

      showSuccess_: {
        type: Boolean,
        value: false,
      },

      showTimeout_: {
        type: Boolean,
        value: false,
      },

      showError_: {
        type: Boolean,
        value: false,
      },

      useUpdatedUi_: {
        type: Boolean,
        reflectToAttribute: true,
        value() {
          return loadTimeData.getBoolean('useUpdatedUi');
        },
      },
    };
  }

  private showEnterpriseBadge_: boolean;
  private pictureUrl_: string;
  private title_: string;
  private subtitle_: string;
  private enterpriseInfo_: string;
  private isModalDialog_: boolean;
  private proceedLabel_: string;
  private disableProceedButton_: boolean;
  private showCancelButton_: boolean;
  private currentState_: State;
  private showDisclosure_: boolean;
  private showProcessing_: boolean;
  private showSuccess_: boolean;
  private showTimeout_: boolean;
  private showError_: boolean;
  private managedUserProfileNoticeBrowserProxy_:
      ManagedUserProfileNoticeBrowserProxy =
          ManagedUserProfileNoticeBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.addWebUiListener(
        'on-state-changed', (state: State) => this.updateCurrentState_(state));

    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ManagedUserProfileInfo) => this.setProfileInfo_(info));
    this.managedUserProfileNoticeBrowserProxy_.initialized().then(
        info => this.setProfileInfo_(info));
  }

  /** Called when the proceed button is clicked. */
  private onProceed_() {
    this.disableProceedButton_ = true;
    this.managedUserProfileNoticeBrowserProxy_.proceed(
        /*currentState=*/ this.currentState_, /*linkData=*/ false);
  }

  /** Called when the cancel button is clicked. */
  private onCancel_() {
    this.managedUserProfileNoticeBrowserProxy_.cancel();
  }

  private setProfileInfo_(info: ManagedUserProfileInfo) {
    this.pictureUrl_ = info.pictureUrl;
    this.showEnterpriseBadge_ = info.showEnterpriseBadge;
    this.title_ = info.title;
    this.subtitle_ = info.subtitle;
    this.enterpriseInfo_ = info.enterpriseInfo;
    this.showCancelButton_ = info.showCancelButton;
  }

  private updateCurrentState_(state: State) {
    this.currentState_ = state;
    this.showDisclosure_ = state === State.DISCLOSURE;
    this.showProcessing_ = state === State.PROCESSING;
    this.showSuccess_ = state === State.SUCCESS;
    this.showTimeout_ = state === State.TIMEOUT;
    this.showError_ = state === State.ERROR;

    this.disableProceedButton_ = false;
    if (this.showError_ || this.showSuccess_ || this.showTimeout_) {
      this.shadowRoot!.querySelector<HTMLButtonElement>(
                          '#proceed-button')!.focus();
    }
  }

  private allowCancel_() {
    return this.showCancelButton_ && this.showDisclosure_;
  }

  private computeProceedLabel_(state: State) {
    switch (state) {
      case State.DISCLOSURE:
      case State.PROCESSING:
        return this.i18n('continueLabel');
      case State.TIMEOUT:
      case State.SUCCESS:
      case State.ERROR:
        return this.i18n('confirmLabel');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-app': ManagedUserProfileNoticeAppElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeAppElement.is, ManagedUserProfileNoticeAppElement);
