// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './shared_style.css.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import type {CrLinkRowElement} from 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_section.html.js';
import type {FocusConfig} from './focus_config.js';
import type {CredentialsChangedListener, PasswordCheckStatusChangedListener} from './password_manager_proxy.js';
import {PasswordCheckInteraction, PasswordManagerImpl} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {CheckupSubpage, Page, RouteObserverMixin, Router, UrlParam} from './router.js';

const CheckState = chrome.passwordsPrivate.PasswordCheckState;

export interface CheckupSectionElement {
  $: {
    checkupResult: HTMLElement,
    checkupStatusLabel: HTMLElement,
    checkupStatusSubLabel: HTMLElement,
    refreshButton: CrIconButtonElement,
    retryButton: CrButtonElement,
    spinner: PaperSpinnerLiteElement,
    compromisedRow: CrLinkRowElement,
    reusedRow: CrLinkRowElement,
    weakRow: CrLinkRowElement,
  };
}

const CheckupSectionElementBase = RouteObserverMixin(I18nMixin(PolymerElement));

export class CheckupSectionElement extends CheckupSectionElementBase {
  static get is() {
    return 'checkup-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      focusConfig: {
        type: Object,
        observer: 'focusConfigChanged_',
      },

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
       * Suggested action to take upon compromised passwords discovery.
       */
      compromisedPasswordsSuggestion_: String,

      /**
       * The status indicates progress and affects banner, title and icon.
       */
      status_: {
        type: Object,
        observer: 'onStatusChanged_',
      },

      compromisedPasswords_: {
        type: Array,
        observer: 'onCompromisedPasswordsChanged_',
      },

      reusedPasswords_: {
        type: Array,
        observer: 'onReusedPasswordsChanged_',
      },

      weakPasswords_: {
        type: Array,
        observer: 'onWeakPasswordsChanged_',
      },

      isCheckRunning_: {
        type: Boolean,
        computed: 'computeIsCheckRunning_(status_)',
      },

      isCheckSuccessful_: {
        type: Boolean,
        computed: 'computeIsCheckSuccessful_(status_)',
      },

      bannerImage_: {
        type: Array,
        value: 'checkup_result_banner_error',
        computed: 'computeBannerImage_(status_, compromisedPasswords_, ' +
            'reusedPasswords_, weakPasswords_)',
      },

      groupCount_: {
        type: Number,
        value: 0,
        observer: 'updateCheckedPasswordsText_',
      },
    };
  }

  focusConfig: FocusConfig;
  private checkedPasswordsText_: string;
  private compromisedPasswordsText_: string;
  private reusedPasswordsText_: string;
  private weakPasswordsText_: string;
  private compromisedPasswordsSuggestion_: string;
  private status_: chrome.passwordsPrivate.PasswordCheckStatus;
  private compromisedPasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private weakPasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private reusedPasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private didCheckAutomatically_: boolean = false;
  private groupCount_: number;

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

    this.setSavedPasswordsListener_ = _passwordList => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(
          groups => this.groupCount_ = groups.length);
    };

    PasswordManagerImpl.getInstance().getPasswordCheckStatus().then(
        this.statusChangedListener_);
    PasswordManagerImpl.getInstance().addPasswordCheckStatusListener(
        this.statusChangedListener_);

    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);

    PasswordManagerImpl.getInstance().getCredentialGroups().then(
        groups => this.groupCount_ = groups.length);
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

  override currentRouteChanged(route: Route): void {
    const param = route.queryParameters.get(UrlParam.START_CHECK) || '';
    if (param === 'true' && !this.didCheckAutomatically_) {
      this.didCheckAutomatically_ = true;
      PasswordManagerImpl.getInstance().startBulkPasswordCheck().catch(
          () => {});
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
      oldStatus: chrome.passwordsPrivate.PasswordCheckStatus) {
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
                'checkedPasswords', this.groupCount_);
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
    return this.status_.state === CheckState.RUNNING;
  }

  private computeIsCheckSuccessful_(): boolean {
    return this.status_.state === CheckState.IDLE;
  }

  private didCompromiseCheckFail_(): boolean {
    return [
      CheckState.OFFLINE,
      CheckState.SIGNED_OUT,
      CheckState.QUOTA_LIMIT,
      CheckState.OTHER_ERROR,
    ].includes(this.status_.state);
  }

  private showRetryButton_(): boolean {
    return !this.computeIsCheckRunning_() && !this.computeIsCheckSuccessful_();
  }

  private showCheckButton_(): boolean {
    return this.status_.state !== CheckState.NO_PASSWORDS;
  }

  /**
   * Starts/Restarts bulk password check.
   */
  private onPasswordCheckButtonClick_() {
    PasswordManagerImpl.getInstance().startBulkPasswordCheck().catch(() => {});
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.START_CHECK_MANUALLY);
  }

  private computeBannerImage_(): string {
    if (!this.status_) {
      return 'checkup_result_banner_error';
    }

    if (this.computeIsCheckRunning_() ||
        this.status_.state === CheckState.NO_PASSWORDS) {
      return 'checkup_result_banner_running';
    }
    if (this.computeIsCheckSuccessful_()) {
      return this.hasAnyIssues_() ? 'checkup_result_banner_compromised' :
                                    'checkup_result_banner_ok';
    }
    return 'checkup_result_banner_error';
  }

  private getIcon_(
      issues: chrome.passwordsPrivate.PasswordUiEntry[],
      checkForError: boolean): string {
    if (checkForError && this.status_ && this.didCompromiseCheckFail_()) {
      return 'cr:error';
    }
    return !!issues && issues.length ? 'cr:error' : 'cr:check-circle';
  }

  private hasAnyIssues_(): boolean {
    if (!this.compromisedPasswords_ || !this.reusedPasswords_ ||
        !this.weakPasswords_) {
      return false;
    }
    return !!this.compromisedPasswords_.length ||
        !!this.reusedPasswords_.length || !!this.weakPasswords_.length;
  }

  private hasIssues_(issues: chrome.passwordsPrivate.PasswordUiEntry[]):
      boolean {
    return !!issues.length;
  }

  private getCompromisedSectionLabel_(): string {
    if (this.status_ && this.didCompromiseCheckFail_()) {
      // In case of an error, don't show "No compromised passwords" title since
      // this might be a lie.
      return !this.compromisedPasswords_ || !this.compromisedPasswords_.length ?
          this.i18n('compromisedRowWithError') :
          this.compromisedPasswordsText_;
    }
    return this.compromisedPasswordsText_;
  }

  private getCompromisedSectionSublabel_(): string {
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

  private getReusedSectionSublabel_(): string {
    return this.reusedPasswords_.length ? this.i18n('reusedPasswordsTitle') :
                                          this.i18n('reusedPasswordsEmpty');
  }

  private getWeakSectionSublabel_(): string {
    return this.weakPasswords_.length ? this.i18n('weakPasswordsTitle') :
                                        this.i18n('weakPasswordsEmpty');
  }

  private onCompromisedClick_() {
    if (!this.compromisedPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
  }

  private onReusedClick_() {
    if (!this.reusedPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.REUSED);
  }

  private onWeakClick_() {
    if (!this.weakPasswords_.length) {
      return;
    }

    Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, CheckupSubpage.WEAK);
  }

  private showCheckupSublabel_(): boolean {
    return this.computeIsCheckRunning_();
  }

  private getCheckupSublabelValue_(): string {
    assert(this.status_);
    if (!this.computeIsCheckRunning_()) {
      return this.status_.state === CheckState.NO_PASSWORDS ?
          this.i18n(
              'checkupErrorNoPasswords', this.i18n('localPasswordManager')) :
          this.status_.elapsedTimeSinceLastCheck || '';
    }
    return this.i18n(
        'checkupProgress', this.status_.alreadyProcessed || 0,
        this.status_.totalNumberOfPasswords || 0);
  }

  private showCheckupResult_(): boolean {
    assert(this.status_);
    if (this.computeIsCheckRunning_()) {
      return false;
    }
    return this.status_.state !== CheckState.NO_PASSWORDS;
  }

  private focusConfigChanged_(_newConfig: FocusConfig, oldConfig: FocusConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    this.focusConfig.set(Page.CHECKUP_DETAILS, () => {
      const previousRoute = Router.getInstance().previousRoute;

      switch (previousRoute?.details as unknown as CheckupSubpage) {
        case CheckupSubpage.COMPROMISED:
          focusWithoutInk(this.$.compromisedRow);
          break;
        case CheckupSubpage.REUSED:
          focusWithoutInk(this.$.reusedRow);
          break;
        case CheckupSubpage.WEAK:
          focusWithoutInk(this.$.weakRow);
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
