// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './checkup_section.css.js';
import {getHtml} from './checkup_section.html.js';
import type {FocusConfig} from './focus_config.js';
import type {CredentialsChangedListener, PasswordCheckStatusChangedListener} from './password_manager_proxy.js';
import {PasswordCheckInteraction, PasswordManagerImpl} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {CheckupSubpage, Page, RouteObserverMixinLit, Router, UrlParam} from './router.js';
import {UserUtilMixinLit} from './user_utils_mixin_lit.js';

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

export interface CheckupSectionElement {
  $: {
    checkupResult: HTMLElement,
    checkupStatusLabel: HTMLElement,
    checkupStatusSubLabel: HTMLElement,
    refreshButton: CrIconButtonElement,
    retryButton: CrButtonElement,
    spinner: HTMLElement,
    compromisedRow: CrLinkRowElement,
    reusedRow: CrLinkRowElement,
    weakRow: CrLinkRowElement,
  };
}

const CheckupSectionElementBase =
    UserUtilMixinLit(RouteObserverMixinLit(I18nMixinLit(CrLitElement)));

export class CheckupSectionElement extends CheckupSectionElementBase {
  static get is() {
    return 'checkup-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      focusConfig: {type: Object},

      /**
       * The number of checked passwords as a formatted string.
       */
      checkedPasswordsText_: {type: String},

      /**
       * The number of compromised passwords as a formatted string.
       */
      compromisedPasswordsText_: {type: String},

      /**
       * The number of weak passwords as a formatted string.
       */
      reusedPasswordsText_: {type: String},

      /**
       * The number of weak passwords as a formatted string.
       */
      weakPasswordsText_: {type: String},

      /**
       * Suggested action to take upon compromised passwords discovery.
       */
      compromisedPasswordsSuggestion_: {type: String},

      /**
       * The status indicates progress and affects banner, title and icon.
       */
      status_: {type: Object},

      compromisedPasswords_: {type: Array},
      reusedPasswords_: {type: Array},
      weakPasswords_: {type: Array},
      isCheckRunning_: {type: Boolean},
      isCheckSuccessful_: {type: Boolean},
      bannerImage_: {type: String},
      passwordCount_: {type: Number},
    };
  }

  accessor focusConfig: FocusConfig;
  protected accessor checkedPasswordsText_: string = '';
  private accessor compromisedPasswordsText_: string = '';
  protected accessor reusedPasswordsText_: string = '';
  protected accessor weakPasswordsText_: string = '';
  private accessor compromisedPasswordsSuggestion_: string = '';
  private accessor status_: chrome.passwordsPrivate.PasswordCheckStatus|
      undefined;
  protected accessor compromisedPasswords_:
      chrome.passwordsPrivate.PasswordUiEntry[] = [];
  protected accessor weakPasswords_: chrome.passwordsPrivate.PasswordUiEntry[] =
      [];
  protected accessor isCheckRunning_: boolean = false;
  private accessor isCheckSuccessful_: boolean = false;
  protected accessor bannerImage_: string = 'checkup_result_banner_error';
  protected accessor reusedPasswords_:
      chrome.passwordsPrivate.PasswordUiEntry[] = [];
  private didCheckAutomatically_: boolean = false;
  private accessor passwordCount_: number = 0;

  private statusChangedListener_: PasswordCheckStatusChangedListener|null =
      null;
  private insecureCredentialsChangedListener_: CredentialsChangedListener|null =
      null;
  private setSavedPasswordsListener_: CredentialsChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.statusChangedListener_ = status => {
      this.status_ = status;
    };

    this.insecureCredentialsChangedListener_ = insecureCredentials => {
      this.compromisedPasswords_ = insecureCredentials.filter(cred => {
        return !cred.compromisedInfo!.isMuted &&
            cred.compromisedInfo!.compromiseTypes.some(type => {
              return (
                  type === chrome.passwordsPrivate.CompromiseType.LEAKED ||
                  type === chrome.passwordsPrivate.CompromiseType.PHISHED);
            });
      });

      this.reusedPasswords_ = insecureCredentials.filter(cred => {
        return cred.compromisedInfo!.compromiseTypes.some(type => {
          return type === chrome.passwordsPrivate.CompromiseType.REUSED;
        });
      });

      this.weakPasswords_ = insecureCredentials.filter(cred => {
        return cred.compromisedInfo!.compromiseTypes.some(type => {
          return type === chrome.passwordsPrivate.CompromiseType.WEAK;
        });
      });
    };

    this.setSavedPasswordsListener_ = passwordList => {
      this.passwordCount_ =
          passwordList
              .filter(entry => !entry.federationText && !entry.isPasskey)
              .length;
    };

    PasswordManagerImpl.getInstance().getPasswordCheckStatus().then(
        this.statusChangedListener_);
    PasswordManagerImpl.getInstance().addPasswordCheckStatusListener(
        this.statusChangedListener_);

    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);

    PasswordManagerImpl.getInstance().getSavedPasswordList().then(
        this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().addSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.statusChangedListener_);
    PasswordManagerImpl.getInstance().removePasswordCheckStatusListener(
        this.statusChangedListener_);
    this.statusChangedListener_ = null;

    assert(this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().removeInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);
    this.insecureCredentialsChangedListener_ = null;

    assert(this.setSavedPasswordsListener_);
    PasswordManagerImpl.getInstance().removeSavedPasswordListChangedListener(
        this.setSavedPasswordsListener_);
    this.setSavedPasswordsListener_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('focusConfig')) {
      this.focusConfigChanged_(changedProperties.get('focusConfig'));
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('status_')) {
      this.isCheckRunning_ = this.computeIsCheckRunning_();
      this.isCheckSuccessful_ = this.computeIsCheckSuccessful_();
    }

    if (changedPrivateProperties.has('passwordCount_') &&
        !changedPrivateProperties.has('status_')) {
      this.updateCheckedPasswordsText_();
    }

    if (changedPrivateProperties.has('compromisedPasswords_')) {
      this.onCompromisedPasswordsChanged_();
    }

    if (changedPrivateProperties.has('reusedPasswords_')) {
      this.onReusedPasswordsChanged_();
    }

    if (changedPrivateProperties.has('weakPasswords_')) {
      this.onWeakPasswordsChanged_();
    }

    if (changedPrivateProperties.has('status_') ||
        changedPrivateProperties.has('compromisedPasswords_') ||
        changedPrivateProperties.has('reusedPasswords_') ||
        changedPrivateProperties.has('weakPasswords_')) {
      this.bannerImage_ = this.computeBannerImage_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('status_')) {
      this.onStatusChanged_(
          this.status_!,
          changedPrivateProperties.get('status_') as
                  chrome.passwordsPrivate.PasswordCheckStatus |
              undefined);
    }
  }

  override currentRouteChanged(route: Route): void {
    const param = route.queryParameters.get(UrlParam.START_CHECK) || '';
    if (param === 'true' && !this.didCheckAutomatically_) {
      this.didCheckAutomatically_ = true;
      PasswordManagerImpl.getInstance().startBulkPasswordCheck();
      PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
          PasswordCheckInteraction.START_CHECK_AUTOMATICALLY);
    }
    if (route.page === Page.CHECKUP) {
      PasswordManagerImpl.getInstance()
          .dismissSafetyHubPasswordMenuNotification();
    }
  }

  private async onStatusChanged_(
      newStatus: chrome.passwordsPrivate.PasswordCheckStatus,
      oldStatus?: chrome.passwordsPrivate.PasswordCheckStatus) {
    // if state is unchanged - nothing to do.
    if (oldStatus !== undefined && oldStatus.state === newStatus.state) {
      return;
    }

    await this.updateCheckedPasswordsText_();

    if (newStatus.state === CheckState.NO_PASSWORDS) {
      return;
    }

    // Announce password check result and focus retry/refresh button when
    // password check is finished.
    if (!!oldStatus && oldStatus.state === CheckState.RUNNING &&
        newStatus.state !== CheckState.RUNNING) {
      let stateText: string;
      if (this.compromisedPasswords_.length > 0) {
        stateText = this.i18n('checkupResultRed');
      } else if (this.hasAnyIssues_()) {
        stateText = this.i18n('checkupResultYellow');
      } else {
        stateText = this.i18n('checkupResultGreen');
      }
      getAnnouncerInstance().announce(
          [this.checkedPasswordsText_, stateText].join('. '));
      focusWithoutInk(
          this.showRetryButton_() ? this.$.retryButton : this.$.refreshButton);
    } else if (
        !!oldStatus && oldStatus.state !== CheckState.RUNNING &&
        newStatus.state === CheckState.RUNNING) {
      // Announce password checkup has started.
      getAnnouncerInstance().announce('Password check started');
    }
  }

  private async updateCheckedPasswordsText_() {
    if (!this.status_) {
      return;
    }

    switch (this.status_.state) {
      case CheckState.IDLE:
      case CheckState.OFFLINE:
      case CheckState.SIGNED_OUT:
      case CheckState.QUOTA_LIMIT:
      case CheckState.OTHER_ERROR:
      case CheckState.NO_PASSWORDS:
        this.checkedPasswordsText_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'checkedPasswords', this.passwordCount_);
        return;
      case CheckState.CANCELED:
        this.checkedPasswordsText_ = this.i18n('checkupCanceled');
        return;
      case CheckState.RUNNING:
        this.checkedPasswordsText_ =
            await PluralStringProxyImpl.getInstance().getPluralString(
                'checkingPasswords', this.status_.totalNumberOfPasswords || 0);
        return;
      default:
        assertNotReached(
            'Can\'t find a title for state: ' + this.status_.state);
    }
  }

  private async onCompromisedPasswordsChanged_() {
    this.compromisedPasswordsText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'compromisedPasswords', this.compromisedPasswords_.length);

    this.compromisedPasswordsSuggestion_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'compromisedPasswordsTitle', this.compromisedPasswords_.length);
  }

  private async onReusedPasswordsChanged_() {
    this.reusedPasswordsText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'reusedPasswords', this.reusedPasswords_.length);
  }

  private async onWeakPasswordsChanged_() {
    this.weakPasswordsText_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'weakPasswords', this.weakPasswords_.length);
  }

  /**
   * @return true iff a check is running right according to the given |status|.
   */
  private computeIsCheckRunning_(): boolean {
    return !!this.status_ && this.status_.state === CheckState.RUNNING;
  }

  private computeIsCheckSuccessful_(): boolean {
    return !!this.status_ && this.status_.state === CheckState.IDLE;
  }

  protected didCompromiseCheckFail_(): boolean {
    if (!this.status_) {
      return false;
    }
    return [
      CheckState.OFFLINE,
      CheckState.SIGNED_OUT,
      CheckState.QUOTA_LIMIT,
      CheckState.OTHER_ERROR,
    ].includes(this.status_.state);
  }

  protected showRetryButton_(): boolean {
    return !this.isCheckRunning_ && !this.isCheckSuccessful_;
  }

  protected showCheckButton_(): boolean {
    return !!this.status_ && this.status_.state !== CheckState.NO_PASSWORDS;
  }

  /**
   * Starts/Restarts bulk password check.
   */
  protected onPasswordCheckButtonClick_() {
    PasswordManagerImpl.getInstance().startBulkPasswordCheck();
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.START_CHECK_MANUALLY);
  }

  private computeBannerImage_(): string {
    if (!this.status_) {
      return 'checkup_result_banner_error';
    }

    if (this.isCheckRunning_ ||
        this.status_.state === CheckState.NO_PASSWORDS) {
      return 'checkup_result_banner_running';
    }
    if (this.isCheckSuccessful_) {
      return this.hasAnyIssues_() ? 'checkup_result_banner_compromised' :
                                    'checkup_result_banner_ok';
    }
    return 'checkup_result_banner_error';
  }

  protected getIcon_(
      issues: chrome.passwordsPrivate.PasswordUiEntry[],
      checkForError: boolean = false): string {
    if (checkForError && this.status_ && this.didCompromiseCheckFail_()) {
      return 'cr:error-filled';
    }
    return !!issues && issues.length ? 'cr:error-filled' : 'cr:check-circle';
  }

  private hasAnyIssues_(): boolean {
    if (!this.compromisedPasswords_ || !this.reusedPasswords_ ||
        !this.weakPasswords_) {
      return false;
    }
    return !!this.compromisedPasswords_.length ||
        !!this.reusedPasswords_.length || !!this.weakPasswords_.length;
  }

  protected hasIssues_(issues: chrome.passwordsPrivate.PasswordUiEntry[]):
      boolean {
    return !!issues.length;
  }

  protected getCompromisedSectionLabel_(): string {
    if (this.status_ && this.didCompromiseCheckFail_()) {
      // In case of an error, don't show "No compromised passwords" title since
      // this might be a lie.
      return !this.compromisedPasswords_ || !this.compromisedPasswords_.length ?
          this.i18n('compromisedRowWithError') :
          this.compromisedPasswordsText_;
    }
    return this.compromisedPasswordsText_;
  }

  protected getCompromisedSectionSublabel_(): string {
    if (!this.status_ || !this.compromisedPasswords_) {
      return '';
    }
    const brandingName = this.i18n('localPasswordManager');
    switch (this.status_.state) {
      case CheckState.IDLE:
      case CheckState.NO_PASSWORDS:
      case CheckState.RUNNING:
      case CheckState.CANCELED:
        return this.compromisedPasswords_.length ?
            this.compromisedPasswordsSuggestion_ :
            this.i18n('compromisedPasswordsEmpty');
      case CheckState.OFFLINE:
        return this.i18n('checkupErrorOffline', brandingName);
      case CheckState.SIGNED_OUT:
        return this.i18n('checkupErrorSignedOut', brandingName);
      case CheckState.QUOTA_LIMIT:
        return this.i18n('checkupErrorQuota', brandingName);
      case CheckState.OTHER_ERROR:
        return this.i18n('checkupErrorGeneric', brandingName);
      default:
        assertNotReached(
            'Can\'t find a title for state: ' + this.status_.state);
    }
  }

  protected getReusedSectionSublabel_(): string {
    return this.reusedPasswords_.length ? this.i18n('reusedPasswordsTitle') :
                                          this.i18n('reusedPasswordsEmpty');
  }

  protected getWeakSectionSublabel_(): string {
    return this.weakPasswords_.length ? this.i18n('weakPasswordsTitle') :
                                        this.i18n('weakPasswordsEmpty');
  }

  protected onCompromisedClick_() {
    if (!this.compromisedPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
  }

  protected onReusedClick_() {
    if (!this.reusedPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.REUSED);
  }

  protected onWeakClick_() {
    if (!this.weakPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, CheckupSubpage.WEAK);
  }

  protected getCheckupSublabelValue_(): TrustedHTML {
    if (!this.status_ || this.actionableError === null) {
      return sanitizeInnerHtml('');
    }
    if (this.isCheckRunning_) {
      return sanitizeInnerHtml(this.i18n(
          'checkupProgress', this.status_.alreadyProcessed || 0,
          this.status_.totalNumberOfPasswords || 0));
    }

    if (this.status_.state !== CheckState.NO_PASSWORDS) {
      return sanitizeInnerHtml(this.status_.elapsedTimeSinceLastCheck || '');
    }

    if (this.isTrustedVaultKeyNeeded()) {
      return this.i18nAdvanced('checkupEmptyStateTrustedVaultKeyNeeded');
    }

    return this.i18nAdvanced(
        'checkupErrorNoPasswords',
        {substitutions: [this.i18n('localPasswordManager')]});
  }

  protected onCheckupStatusSubLabelClick_(e: Event) {
    const target = e.target as HTMLElement;
    if (target.tagName === 'A') {
      e.preventDefault();
      if (this.isTrustedVaultKeyNeeded()) {
        PasswordManagerImpl.getInstance().startTrustedVaultUnlock();
      }
    }
  }

  protected showCheckupResult_(): boolean {
    if (!this.status_) {
      return false;
    }
    if (this.isCheckRunning_) {
      return false;
    }
    return this.status_.state !== CheckState.NO_PASSWORDS;
  }

  private focusConfigChanged_(oldConfig?: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);
    assert(this.focusConfig);

    this.focusConfig.set(Page.CHECKUP_DETAILS, () => {
      const previousRoute = Router.getInstance().previousRoute;

      switch (previousRoute?.details as CheckupSubpage) {
        case CheckupSubpage.COMPROMISED:
          focusWithoutInk(this.$.compromisedRow);
          break;
        case CheckupSubpage.REUSED:
          focusWithoutInk(this.$.reusedRow);
          break;
        case CheckupSubpage.WEAK:
          focusWithoutInk(this.$.weakRow);
          break;
        default:
          break;
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-section': CheckupSectionElement;
  }
}

customElements.define(CheckupSectionElement.is, CheckupSectionElement);
