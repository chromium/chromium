// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import './checkup_list_item.js';

import {PrefServiceObserverMixinLit} from '/shared/settings/prefs2/pref_service_observer_mixin_lit.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './checkup_details_section.css.js';
import {getHtml} from './checkup_details_section.html.js';
import type {CheckupListItemElement} from './checkup_list_item.js';
import {PasswordAutomaticChangeState} from './password_manager.mojom-webui.js';
import type {CredentialsChangedListener} from './password_manager_proxy.js';
import {PasswordCheckInteraction, PasswordManagerImpl} from './password_manager_proxy.js';
import type {Route} from './router.js';
import {CheckupSubpage, Page, RouteObserverMixinLit, Router} from './router.js';

export class ReusedPasswordInfo {
  constructor(credentials: chrome.passwordsPrivate.PasswordUiEntry[]) {
    this.credentials = credentials;
  }

  async init() {
    this.title = await PluralStringProxyImpl.getInstance().getPluralString(
        'numberOfPasswordReuse', this.credentials.length);
  }

  credentials: chrome.passwordsPrivate.PasswordUiEntry[];
  title: string;
}

export interface CheckupDetailsSectionElement {
  $: {
    backButton: HTMLElement,
    description: HTMLElement,
    moreActionsMenu: CrActionMenuElement,
    menuShowPassword: HTMLButtonElement,
    menuEditPassword: HTMLButtonElement,
    menuDeletePassword: HTMLButtonElement,
    subtitle: HTMLElement,
  };
}

const CheckupDetailsSectionElementBase = PrefServiceObserverMixinLit(
    I18nMixinLit(RouteObserverMixinLit(CrLitElement)));

export class CheckupDetailsSectionElement extends
    CheckupDetailsSectionElementBase {
  static get is() {
    return 'checkup-details-section';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      pageTitle_: {
        type: String,
      },

      pageSubtitle_: {
        type: String,
      },

      insecurityType_: {
        type: String,
      },

      groups_: {
        type: Array,
      },

      allInsecureCredentials_: {
        type: Array,
      },

      shownInsecureCredentials_: {
        type: Array,
      },

      credentialsWithReusedPassword_: {
        type: Array,
      },

      mutedCompromisedCredentials_: {
        type: Array,
      },

      mutedLeakedCredentialsExpanded_: {
        type: Boolean,
      },

      activeListItem_: {
        type: Object,
      },

      /**
       * The ids of insecure credentials for which user clicked "Change
       * Password" button
       */
      clickedChangePasswordIds_: {
        type: Object,
      },

      isMutingDisabled_: {
        type: Boolean,
      },

      passwordChangeStates_: {
        type: Object,
      },
    };
  }

  protected accessor pageTitle_: string = '';
  protected accessor pageSubtitle_: string = '';
  private accessor insecurityType_: CheckupSubpage|undefined;
  private accessor groups_: chrome.passwordsPrivate.CredentialGroup[] = [];
  private accessor allInsecureCredentials_:
      chrome.passwordsPrivate.PasswordUiEntry[]|undefined;
  protected accessor shownInsecureCredentials_:
      chrome.passwordsPrivate.PasswordUiEntry[] = [];
  protected accessor credentialsWithReusedPassword_: ReusedPasswordInfo[] = [];
  protected accessor mutedCompromisedCredentials_:
      chrome.passwordsPrivate.PasswordUiEntry[] = [];
  protected accessor mutedLeakedCredentialsExpanded_: boolean = false;
  private accessor activeListItem_: CheckupListItemElement|null = null;
  private accessor clickedChangePasswordIds_: Set<number> = new Set();
  protected accessor isMutingDisabled_: boolean = false;
  private accessor passwordChangeStates_:
      Record<number, PasswordAutomaticChangeState> = {};

  private insecureCredentialsChangedListener_: CredentialsChangedListener|null =
      null;
  private listenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addPrefObserver<boolean>(
        'profile.password_dismiss_compromised_alert', pref => {
          this.isMutingDisabled_ = !pref.value;
        });

    const updateGroups = () => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(
          groups => this.groups_ = groups);
    };

    this.insecureCredentialsChangedListener_ = insecureCredentials => {
      // TODO(crbug.com/532045774): handle the case when the id of the
      // credential has changed and APC tracking fails
      this.allInsecureCredentials_ = insecureCredentials;
      updateGroups();
    };

    updateGroups();
    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);

    this.listenerId_ =
        PasswordManagerImpl.getInstance()
            .callbackRouter.onPasswordAutomaticChangeStateUpdated.addListener(
                (id: number, state: PasswordAutomaticChangeState) =>
                    this.onStateUpdated_(id, state));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    if (this.insecureCredentialsChangedListener_) {
      PasswordManagerImpl.getInstance().removeInsecureCredentialsListener(
          this.insecureCredentialsChangedListener_);
    }
    if (this.listenerId_ !== null) {
      PasswordManagerImpl.getInstance().callbackRouter.removeListener(
          this.listenerId_);
      this.listenerId_ = null;
    }
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('insecurityType_') ||
        changedPrivateProperties.has('allInsecureCredentials_')) {
      this.updateShownCredentials_();
    }
    if (changedPrivateProperties.has('shownInsecureCredentials_') &&
        this.allInsecureCredentials_) {
      this.onCredentialsChanged_();
    }
  }

  private onStateUpdated_(id: number, state: PasswordAutomaticChangeState) {
    this.passwordChangeStates_[id] = state;
    this.requestUpdate();
  }

  protected getPasswordChangeStateFor_(id: number):
      PasswordAutomaticChangeState {
    return this.passwordChangeStates_[id] ||
        PasswordAutomaticChangeState.kInactive;
  }

  override currentRouteChanged(route: Route, oldRoute?: Route): void {
    if (route.page !== Page.CHECKUP_DETAILS) {
      return;
    }
    this.insecurityType_ = route.details as CheckupSubpage;
    // Focus back button when it's not direct navigation.
    if (oldRoute !== undefined) {
      setTimeout(() => {  // Async to allow page to load.
        this.$.backButton.focus();
      });
    }
  }

  protected onBackButtonClick_() {
    Router.getInstance().navigateTo(Page.CHECKUP);
  }

  private compareCredentialsByGroup_ =
      (lhs: chrome.passwordsPrivate.PasswordUiEntry,
       rhs: chrome.passwordsPrivate.PasswordUiEntry): number => {
        const lhsName = this.getCurrentGroup_(lhs.id)?.name || '';
        const rhsName = this.getCurrentGroup_(rhs.id)?.name || '';
        if (lhsName === rhsName) {
          return 0;
        }
        return lhsName > rhsName ? 1 : -1;
      };

  private matchesInsecurityType_(
      credential: chrome.passwordsPrivate.PasswordUiEntry,
      types: chrome.passwordsPrivate.CompromiseType[]): boolean {
    assert(credential.compromisedInfo);
    return credential.compromisedInfo.compromiseTypes.some(
        t => types.includes(t));
  }

  private getCredentialsForCurrentType_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    const types = this.getInsecurityType_();

    return (this.allInsecureCredentials_ || [])
        .filter(cred => this.matchesInsecurityType_(cred, types));
  }

  private async updateShownCredentials_() {
    if (!this.insecurityType_ || !this.allInsecureCredentials_) {
      return;
    }

    const currentTypeCredentials = this.getCredentialsForCurrentType_();
    // 1. Compromised subpage (Leaked / Phished)
    if (this.isCompromisedType()) {
      // Compromised credentials can be muted. Show muted credentials
      // separately.
      this.mutedCompromisedCredentials_ =
          currentTypeCredentials.filter(cred => cred.compromisedInfo!.isMuted);
      this.shownInsecureCredentials_ =
          currentTypeCredentials.filter(cred => !cred.compromisedInfo!.isMuted);
      return;
    }

    // 2. Reused subpage
    if (this.isReusedType()) {
      const requestedType = this.insecurityType_;
      this.shownInsecureCredentials_ = currentTypeCredentials;
      const allReusedCredentials = await PasswordManagerImpl.getInstance()
                                       .getCredentialsWithReusedPassword();
      if (this.insecurityType_ !== requestedType) {
        return;
      }
      const credentialsWithReusedPassword =
          await Promise.all(allReusedCredentials.map(
              async(credentials): Promise<ReusedPasswordInfo> => {
                const reuseInfo = new ReusedPasswordInfo(
                    credentials.entries.sort(this.compareCredentialsByGroup_));
                await reuseInfo.init();
                return reuseInfo;
              }));
      if (this.insecurityType_ !== requestedType) {
        return;
      }
      this.credentialsWithReusedPassword_ = credentialsWithReusedPassword.sort(
          (lhs, rhs) =>
              (lhs.credentials.length === rhs.credentials.length ?
                   0 :
                   (lhs.credentials.length > rhs.credentials.length ? -1 : 1)));
      return;
    }

    // 3. Weak subpage
    this.shownInsecureCredentials_ =
        currentTypeCredentials.sort(this.compareCredentialsByGroup_);
  }

  private async onCredentialsChanged_() {
    assert(this.insecurityType_);
    this.pageTitle_ = await PluralStringProxyImpl.getInstance().getPluralString(
        this.insecurityType_.concat('Passwords'),
        this.shownInsecureCredentials_.length);
    if (this.insecurityType_ === CheckupSubpage.COMPROMISED) {
      this.pageSubtitle_ =
          await PluralStringProxyImpl.getInstance().getPluralString(
              `${this.insecurityType_}PasswordsTitle`,
              this.shownInsecureCredentials_.length);
    } else {
      this.pageSubtitle_ = this.i18n(`${this.insecurityType_}PasswordsTitle`);
    }
  }

  private getInsecurityType_(): chrome.passwordsPrivate.CompromiseType[] {
    assert(this.insecurityType_);
    switch (this.insecurityType_) {
      case CheckupSubpage.COMPROMISED:
        return [
          chrome.passwordsPrivate.CompromiseType.LEAKED,
          chrome.passwordsPrivate.CompromiseType.PHISHED,
        ];
      case CheckupSubpage.REUSED:
        return [chrome.passwordsPrivate.CompromiseType.REUSED];
      case CheckupSubpage.WEAK:
        return [chrome.passwordsPrivate.CompromiseType.WEAK];
      default:
        assertNotReached();
    }
  }

  protected getDescription_() {
    if (!this.insecurityType_) {
      return '';
    }
    return this.i18n(`${this.insecurityType_}PasswordsDescription`);
  }

  protected isCompromisedType(): boolean {
    return this.insecurityType_ === CheckupSubpage.COMPROMISED;
  }

  protected isReusedType(): boolean {
    return this.insecurityType_ === CheckupSubpage.REUSED;
  }

  protected onMoreActionsClick_(
      event: CustomEvent<
          {listItem: CheckupListItemElement, target: HTMLElement}>) {
    const target = event.detail.target;
    this.$.moreActionsMenu.showAt(target);
    this.activeListItem_ = event.detail.listItem;
  }

  protected onMenuShowPasswordClick_() {
    this.activeListItem_?.showHidePassword();
    this.$.moreActionsMenu.close();
    this.activeListItem_ = null;
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.SHOW_PASSWORD);
  }

  protected onMenuEditPasswordClick_() {
    this.activeListItem_?.showEditDialog();
    this.$.moreActionsMenu.close();
    this.activeListItem_ = null;
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.EDIT_PASSWORD);
  }

  protected onMenuDeletePasswordClick_() {
    this.activeListItem_?.showDeleteDialog();
    this.$.moreActionsMenu.close();
    this.activeListItem_ = null;
  }

  protected getShowHideTitle_(): string {
    return this.activeListItem_?.getShowHideButtonLabel() || '';
  }

  protected getMuteUnmuteLabel_(): string {
    return this.activeListItem_?.item.compromisedInfo?.isMuted === true ?
        this.i18n('unmuteCompromisedPassword') :
        this.i18n('muteCompromisedPassword');
  }

  protected onMenuMuteUnmuteClick_() {
    assert(this.activeListItem_);
    if (this.activeListItem_.item.compromisedInfo?.isMuted === true) {
      PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
          PasswordCheckInteraction.UNMUTE_PASSWORD);
      PasswordManagerImpl.getInstance().unmuteInsecureCredential(
          this.activeListItem_.item);
    } else {
      PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
          PasswordCheckInteraction.MUTE_PASSWORD);
      PasswordManagerImpl.getInstance().muteInsecureCredential(
          this.activeListItem_.item);
    }
    this.$.moreActionsMenu.close();
  }

  protected getCurrentGroup_(id: number):
      chrome.passwordsPrivate.CredentialGroup|undefined {
    return this.groups_?.find(
        group => group.entries.some(entry => entry.id === id));
  }

  protected onChangePasswordClicked_(event: CustomEvent<number>) {
    this.clickedChangePasswordIds_.add(event.detail);
    this.requestUpdate();
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.CHANGE_PASSWORD);
  }

  protected clickedChangePassword_(
      item: chrome.passwordsPrivate.PasswordUiEntry): boolean {
    return this.clickedChangePasswordIds_.has(item.id);
  }

  protected onMutedLeakedCredentialsExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.mutedLeakedCredentialsExpanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-details-section': CheckupDetailsSectionElement;
  }
}

customElements.define(
    CheckupDetailsSectionElement.is, CheckupDetailsSectionElement);
