// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './shared_style.css.js';
import './checkup_list_item.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './checkup_details_section.html.js';
import {CheckupListItemElement} from './checkup_list_item.js';
import {CredentialsChangedListener, PasswordCheckInteraction, PasswordManagerImpl} from './password_manager_proxy.js';
import {PrefMixin} from './prefs/pref_mixin.js';
import {CheckupSubpage, Page, Route, RouteObserverMixin, Router} from './router.js';

export interface CheckupDetailsSectionElement {
  $: {
    description: HTMLElement,
    moreActionsMenu: CrActionMenuElement,
    menuShowPassword: HTMLButtonElement,
    menuEditPassword: HTMLButtonElement,
    menuRemovePassword: HTMLButtonElement,
    subtitle: HTMLElement,
  };
}

const CheckupDetailsSectionElementBase =
    PrefMixin(I18nMixin(RouteObserverMixin(PolymerElement)));

export class CheckupDetailsSectionElement extends
    CheckupDetailsSectionElementBase {
  static get is() {
    return 'checkup-details-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      pageTitle_: String,

      insecurityType_: {
        type: String,
        observer: 'updateShownCredentials_',
      },

      allInsecureCredentials_: {
        type: Array,
        observer: 'updateShownCredentials_',
      },

      shownInsecureCredentials_: {
        type: Array,
        observer: 'onCredentialsChanged_',
      },
    };
  }

  private pageTitle_: string;
  private insecurityType_: CheckupSubpage|undefined;
  private groups_: chrome.passwordsPrivate.CredentialGroup[] = [];
  private allInsecureCredentials_: chrome.passwordsPrivate.PasswordUiEntry[];
  private shownInsecureCredentials_: chrome.passwordsPrivate.PasswordUiEntry[];
  private mutedCompromisedCredentials_:
      chrome.passwordsPrivate.PasswordUiEntry[];
  private activeListItem_: CheckupListItemElement|null;
  private insecureCredentialsChangedListener_: CredentialsChangedListener|null =
      null;

  constructor() {
    super();
    this.prefKey = 'profile.password_dismiss_compromised_alert';
  }

  override connectedCallback() {
    super.connectedCallback();

    const updateGroups = () => {
      PasswordManagerImpl.getInstance().getCredentialGroups().then(
          groups => this.groups_ = groups);
    };

    this.insecureCredentialsChangedListener_ = insecureCredentials => {
      this.allInsecureCredentials_ = insecureCredentials;
      updateGroups();
    };

    updateGroups();
    PasswordManagerImpl.getInstance().getInsecureCredentials().then(
        this.insecureCredentialsChangedListener_);
    PasswordManagerImpl.getInstance().addInsecureCredentialsListener(
        this.insecureCredentialsChangedListener_);
  }

  override currentRouteChanged(route: Route, _: Route): void {
    if (route.page !== Page.CHECKUP_DETAILS) {
      this.insecurityType_ = undefined;
      return;
    }
    this.insecurityType_ = route.details as unknown as CheckupSubpage;
  }

  private navigateBack_() {
    Router.getInstance().navigateTo(Page.CHECKUP);
  }

  private updateShownCredentials_() {
    if (!this.insecurityType_ || !this.allInsecureCredentials_) {
      return;
    }
    this.shownInsecureCredentials_ =
        this.allInsecureCredentials_.filter(cred => {
          return !cred.compromisedInfo!.isMuted &&
              cred.compromisedInfo!.compromiseTypes.some(type => {
                return this.getInsecurityType_().includes(type);
              });
        });
    this.mutedCompromisedCredentials_ =
        this.allInsecureCredentials_.filter(cred => {
          return cred.compromisedInfo!.isMuted &&
              cred.compromisedInfo!.compromiseTypes.some(type => {
                return this.getInsecurityType_().includes(type);
              });
        });
  }

  private async onCredentialsChanged_() {
    assert(this.insecurityType_);
    this.pageTitle_ = await PluralStringProxyImpl.getInstance().getPluralString(
        this.insecurityType_.concat('Passwords'),
        this.shownInsecureCredentials_.length);
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
    }
  }

  private getSubTitle_() {
    assert(this.insecurityType_);
    return this.i18n(`${this.insecurityType_}PasswordsTitle`);
  }

  private getDescription_() {
    assert(this.insecurityType_);
    return this.i18n(`${this.insecurityType_}PasswordsDescription`);
  }

  private isCompromisedSection(): boolean {
    return this.insecurityType_ === CheckupSubpage.COMPROMISED;
  }

  private isReusedSection(): boolean {
    return this.insecurityType_ === CheckupSubpage.REUSED;
  }

  private onMoreActionsClick_(event: CustomEvent) {
    const target = event.detail.target;
    this.$.moreActionsMenu.showAt(target);
    this.activeListItem_ =
        event.detail.listItem as unknown as CheckupListItemElement;
  }

  private onMenuShowPasswordClick_() {
    this.activeListItem_?.showHidePassword();
    this.$.moreActionsMenu.close();
    this.activeListItem_ = null;
    PasswordManagerImpl.getInstance().recordPasswordCheckInteraction(
        PasswordCheckInteraction.SHOW_PASSWORD);
  }

  private getShowHideTitle_(): string {
    return this.activeListItem_?.getShowHideButtonLabel() || '';
  }

  private isMutingDisabledByPrefs_(): boolean {
    return !!this.pref && this.pref.value === false;
  }

  private getMuteUnmuteLabel_(): string {
    return this.activeListItem_?.item.compromisedInfo?.isMuted === true ?
        this.i18n('unmuteCompromisedPassword') :
        this.i18n('muteCompromisedPassword');
  }

  private onMenuMuteUnmuteClick_() {
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

  private getCurrentGroup_(id: number): chrome.passwordsPrivate.CredentialGroup
      |undefined {
    return this.groups_.find(
        group => group.entries.some(entry => entry.id === id));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'checkup-details-section': CheckupDetailsSectionElement;
  }
}

customElements.define(
    CheckupDetailsSectionElement.is, CheckupDetailsSectionElement);
