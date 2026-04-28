// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import '/icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import './managed_user_profile_notice_disclosure_refresh.js';
import './managed_user_profile_notice_value_prop.js';
import './managed_user_profile_notice_state.js';
import './managed_user_profile_notice_data_handling.js';

import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assertNotReached, assertNotReachedCase} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './managed_user_profile_notice_app_refresh.css.js';
import {getHtml} from './managed_user_profile_notice_app_refresh.html.js';
import type {ManagedUserProfileInfo, ManagedUserProfileNoticeBrowserProxy} from './managed_user_profile_notice_browser_proxy.js';
import {AppMode, BrowsingDataHandling, ManagedUserProfileNoticeBrowserProxyImpl, ScreenType, State} from './managed_user_profile_notice_browser_proxy.js';

export interface ManagedUserProfileNoticeAppRefreshElement {
  $: {
    proceedButton: CrButtonElement,
    cancelButton: CrButtonElement,
  };
}

const ManagedUserProfileNoticeAppRefreshElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ManagedUserProfileNoticeAppRefreshElement extends
    ManagedUserProfileNoticeAppRefreshElementBase {
  static get is() {
    return 'managed-user-profile-notice-app-refresh';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      profileInfo_: {type: Object},

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

      proceedLabel_: {type: String},

      errorTitle_: {type: String},
      errorSubtitle_: {type: String},

      disableProceedButton_: {type: Boolean},
      currentState_: {type: Number},
      processingSubtitle_: {type: String},
      selectedDataHandling_: {type: String},
      usePrimaryAndTonalButtons_: {type: Boolean},
      appMode_: {
        type: String,
        reflect: true,
        attribute: 'app-mode',
      },
      revampEnabled_: {
        type: Boolean,
        reflect: true,
        attribute: 'revamp-enabled',
      },
    };
  }

  protected accessor profileInfo_: ManagedUserProfileInfo = {
    accountName: '',
    continueAs: '',
    email: '',
    pictureUrl: '',
    showEnterpriseBadge: false,
    title: '',
    subtitle: '',
    proceedLabel: '',
    checkLinkDataCheckboxByDefault: false,
  };

  protected accessor isModalDialog_: boolean =
      loadTimeData.getBoolean('isModalDialog');
  protected accessor proceedLabel_: string = '';
  protected accessor errorTitle_: string = '';
  protected accessor errorSubtitle_: string = '';
  protected accessor disableProceedButton_: boolean = false;
  protected accessor currentState_: State = State.DISCLOSURE;
  protected accessor appMode_: AppMode = AppMode.FIRST_RUN;
  protected accessor revampEnabled_: boolean =
      loadTimeData.getBoolean('isFirstRunDesktopRevampEnabled');
  protected accessor processingSubtitle_: string =
      loadTimeData.getString('processingSubtitle');
  protected accessor selectedDataHandling_: BrowsingDataHandling|undefined;
  private accessor usePrimaryAndTonalButtons_: boolean =
      loadTimeData.getBoolean('usePrimaryAndTonalButtonsForPromos');

  private managedUserProfileNoticeBrowserProxy_:
      ManagedUserProfileNoticeBrowserProxy =
          ManagedUserProfileNoticeBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener(
        'on-state-changed', (state: State) => this.updateCurrentState_(state));

    this.addWebUiListener(
        'on-state-changed-to-error',
        (errorTitle: string, errorSubTitle: string) => {
          this.updateErrorStrings_(errorTitle, errorSubTitle);
          this.updateCurrentState_(State.ERROR);
        });

    this.addWebUiListener(
        'on-profile-info-changed',
        (info: ManagedUserProfileInfo) => this.updateProfileInfo_(info));

    this.addWebUiListener(
        'on-long-processing', () => this.updateProcessingText_());

    this.managedUserProfileNoticeBrowserProxy_.initialized().then(info => {
      this.updateProfileInfo_(info);
      this.updateCurrentState_(loadTimeData.getInteger('initialState'));
      this.updateAppMode_(loadTimeData.getInteger('screenType') as ScreenType);

      // Prefer using |document.body.offsetHeight| instead of
      // |document.body.scrollHeight| as it returns the correct height of the
      // page even when the page zoom in Chrome is different than 100%.
      this.managedUserProfileNoticeBrowserProxy_.initializedWithSize(
          document.body.offsetHeight);
    });
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('currentState_')) {
      this.disableProceedButton_ = false;
    }

    if (changedPrivateProperties.has('profileInfo_')) {
      this.selectedDataHandling_ =
          this.profileInfo_.checkLinkDataCheckboxByDefault ?
          BrowsingDataHandling.MERGE :
          BrowsingDataHandling.SEPARATE;
    }

    if (changedPrivateProperties.has('currentState_') ||
        changedPrivateProperties.has('profileInfo_')) {
      this.proceedLabel_ = this.computeProceedLabel_();
    }
  }

  /** Called when the proceed button is clicked. */
  protected onProceedButtonClick_() {
    this.disableProceedButton_ = true;
    const linkData = this.selectedDataHandling_ === BrowsingDataHandling.MERGE;
    this.managedUserProfileNoticeBrowserProxy_.proceed(
        /*currentState=*/ this.currentState_, linkData);
  }

  /** Called when the cancel button is clicked. */
  protected onCancelButtonClick_() {
    if (this.allowValuePropStateBackFromDisclosure_()) {
      this.updateCurrentState_(State.VALUE_PROPOSITION);
      return;
    }
    this.managedUserProfileNoticeBrowserProxy_.cancel();
  }

  protected allowValuePropStateBackFromDisclosure_() {
    return this.currentState_ === State.DISCLOSURE &&
        loadTimeData.getInteger('initialState') !== State.DISCLOSURE;
  }

  private updateProfileInfo_(info: ManagedUserProfileInfo) {
    this.profileInfo_ = info;
  }

  private updateCurrentState_(state: State) {
    this.currentState_ = state;
  }

  private updateAppMode_(screenType: ScreenType) {
    switch (screenType) {
      case ScreenType.PROFILE_PICKER:
      case ScreenType.ENTERPRISE_ACCOUNT_SYNC_ENABLED:
      case ScreenType.ENTERPRISE_ACCOUNT_SYNC_DISABLED:
      case ScreenType.CONSUMER_ACCOUNT_SYNC_DISABLED:
        this.appMode_ = AppMode.PROFILE_PICKER;
        break;
      case ScreenType.FIRST_RUN:
        this.appMode_ = AppMode.FIRST_RUN;
        break;
      case ScreenType.ENTERPRISE_ACCOUNT_CREATION:
      case ScreenType.ENTERPRISE_OIDC:
        assertNotReached();
      default:
        assertNotReachedCase(screenType);
    }
  }

  private updateErrorStrings_(errorTitle: string, errorSubTitle: string) {
    this.errorTitle_ = errorTitle;
    this.errorSubtitle_ = errorSubTitle;
  }

  protected allowCancel_(): boolean {
    return this.isState_(State.DISCLOSURE) ||
        this.isState_(State.VALUE_PROPOSITION) ||
        this.isState_(State.USER_DATA_HANDLING) ||
        this.isState_(State.TIMEOUT) || this.isState_(State.PROCESSING);
  }

  protected getCancelLabel_(): string {
    if (this.isState_(State.VALUE_PROPOSITION) &&
        !loadTimeData.getBoolean('enforcedByPolicy')) {
      return this.i18n('cancelValueProp');
    }
    return this.i18n(
        this.allowValuePropStateBackFromDisclosure_() ? 'backLabel' :
                                                        'cancelLabel');
  }

  protected shouldDisableProceedButton_(): boolean {
    return this.disableProceedButton_ ||
        (this.isState_(State.USER_DATA_HANDLING) &&
         !this.selectedDataHandling_);
  }

  private computeProceedLabel_(): string {
    switch (this.currentState_) {
      case State.VALUE_PROPOSITION:
        return this.profileInfo_.continueAs;
      case State.DISCLOSURE:
      case State.PROCESSING:
      case State.SUCCESS:
        return this.profileInfo_.proceedLabel || this.i18n('continueLabel');
      case State.USER_DATA_HANDLING:
        return this.i18n('confirmLabel');
      case State.ERROR:
        return this.i18n('closeLabel');
      case State.TIMEOUT:
        return this.i18n('retryLabel');
      default:
        assertNotReachedCase(this.currentState_);
    }
  }

  private updateProcessingText_() {
    this.processingSubtitle_ = this.i18n('longProcessingSubtitle');
  }

  protected onSelectedDataHandlingChanged_(
      e: CustomEvent<{value: BrowsingDataHandling}>) {
    this.selectedDataHandling_ = e.detail.value;
  }

  protected getCancelButtonClass_(): string {
    return this.usePrimaryAndTonalButtons_ ? 'tonal-button' : '';
  }

  protected isState_(state: State): boolean {
    return this.currentState_ === state;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'managed-user-profile-notice-app-refresh':
        ManagedUserProfileNoticeAppRefreshElement;
  }
}

customElements.define(
    ManagedUserProfileNoticeAppRefreshElement.is,
    ManagedUserProfileNoticeAppRefreshElement);
