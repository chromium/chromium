// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
// clang-format off
// <if expr="chromeos_lacros">
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// </if>

import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
// clang-format on



import type {ManageProfilesBrowserProxy, ProfileState} from './manage_profiles_browser_proxy.js';
import {ManageProfilesBrowserProxyImpl} from './manage_profiles_browser_proxy.js';
import {getCss} from './profile_card_menu.css.js';
import {getHtml} from './profile_card_menu.html.js';
import {createDummyProfileState} from './profile_picker_util.js';

export interface Statistics {
  BrowsingHistory: number;
  Passwords: number;
  Bookmarks: number;
  Autofill: number;
}

/**
 * This is the data structure sent back and forth between C++ and JS.
 */
export interface StatisticsResult {
  profilePath: string;
  statistics: Statistics;
}

/**
 * Profile statistics data types.
 */
enum ProfileStatistics {
  BROWSING_HISTORY = 'BrowsingHistory',
  PASSWORDS = 'Passwords',
  BOOKMARKS = 'Bookmarks',
  AUTOFILL = 'Autofill',
}

export interface ProfileCardMenuElement {
  $: {
    actionMenu: CrActionMenuElement,
    moreActionsButton: HTMLElement,
    removeConfirmationDialog: CrDialogElement,
    removePrimaryLacrosProfileDialog: CrDialogElement,
  };
}

const ProfileCardMenuElementBase =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class ProfileCardMenuElement extends ProfileCardMenuElementBase {
  static get is() {
    return 'profile-card-menu';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      profileState: {type: Object},

      /**
       * Results of profile statistics, keyed by the suffix of the corresponding
       * data type, as reported by the C++ side.
       */
      statistics_: {type: Object},

      /**
       * List of selected data types.
       */
      profileStatistics_: {type: Array},

      moreActionsButtonAriaLabel_: {type: String},
      removeWarningText_: {type: String},
      removeWarningTitle_: {type: String},
      // <if expr="chromeos_lacros">
      removePrimaryLacrosProfileWarning_: {type: String},
      // </if>
    };
  }

  profileState: ProfileState = createDummyProfileState();
  private statistics_: Statistics = {
    BrowsingHistory: 0,
    Passwords: 0,
    Bookmarks: 0,
    Autofill: 0,
  };
  protected moreActionsButtonAriaLabel_: string = '';
  protected profileStatistics_: ProfileStatistics[] = [
    ProfileStatistics.BROWSING_HISTORY,
    ProfileStatistics.PASSWORDS,
    ProfileStatistics.BOOKMARKS,
    ProfileStatistics.AUTOFILL,
  ];
  protected removeWarningText_: string = '';
  protected removeWarningTitle_: string = '';
  // <if expr="chromeos_lacros">
  protected removePrimaryLacrosProfileWarning_: string;
  // </if>
  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'profiles-list-changed', () => this.handleProfilesUpdated_());
    this.addWebUiListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.addWebUiListener(
        'profile-statistics-received',
        this.handleProfileStatsReceived_.bind(this));
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('profileState')) {
      this.moreActionsButtonAriaLabel_ =
          this.computeMoreActionsButtonAriaLabel_();
      this.removeWarningTitle_ = this.computeRemoveWarningTitle_();
      // <if expr="chromeos_lacros">
      this.removePrimaryLacrosProfileWarning_ =
          this.computeRemovePrimaryLacrosProfileWarning_();
      // </if>
      // <if expr="not chromeos_lacros">
      this.removeWarningText_ = this.computeRemoveWarningText_();
      // </if>
    }
  }

  override firstUpdated() {
    // <if expr="chromeos_lacros">
    this.shadowRoot!.querySelector('#removeWarningHeader a')!.addEventListener(
        'click', () => this.onAccountSettingsClicked_());
    // </if>
  }

  private computeMoreActionsButtonAriaLabel_(): string {
    return this.i18n(
        'profileMenuAriaLabel', this.profileState.localProfileName);
  }

  // <if expr="chromeos_lacros">
  protected getRemoveWarningTextForLacros_(): TrustedHTML {
    return this.i18nAdvanced('removeWarningProfileLacros', {attrs: ['is']});
  }
  // </if>

  // <if expr="not chromeos_lacros">
  private computeRemoveWarningText_(): string {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfile' :
                                      'removeWarningLocalProfile');
  }
  // </if>

  private computeRemoveWarningTitle_(): string {
    return this.i18n(
        this.profileState.isSyncing ? 'removeWarningSignedInProfileTitle' :
                                      'removeWarningLocalProfileTitle');
  }

  // <if expr="chromeos_lacros">
  private computeRemovePrimaryLacrosProfileWarning_(): string {
    return this.i18n(
        'lacrosPrimaryProfileDeletionWarning', this.profileState.userName,
        loadTimeData.getString('deviceType'));
  }
  // </if>

  protected onMoreActionsButtonClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.$.actionMenu.showAt(this.$.moreActionsButton);
    chrome.metricsPrivate.recordUserAction(
        'ProfilePicker_ThreeDottedMenuClicked');
  }

  protected onRemoveButtonClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.manageProfilesBrowserProxy_.getProfileStatistics(
        this.profileState.profilePath);
    this.$.actionMenu.close();
    // <if expr="chromeos_lacros">
    if (this.profileState.isPrimaryLacrosProfile) {
      this.$.removePrimaryLacrosProfileDialog.showModal();
    } else {
      this.$.removeConfirmationDialog.showModal();
    }
    // </if>
    // <if expr="not chromeos_lacros">
    this.$.removeConfirmationDialog.showModal();
    // </if>
    chrome.metricsPrivate.recordUserAction('ProfilePicker_RemoveOptionClicked');
  }

  private handleProfileStatsReceived_(result: StatisticsResult) {
    if (result.profilePath !== this.profileState.profilePath) {
      return;
    }
    this.statistics_ = result.statistics;
  }

  protected getProfileStatisticText_(dataType: ProfileStatistics): string {
    switch (dataType) {
      case ProfileStatistics.BROWSING_HISTORY:
        return this.i18n('removeWarningHistory');
      case ProfileStatistics.PASSWORDS:
        return this.i18n('removeWarningPasswords');
      case ProfileStatistics.BOOKMARKS:
        return this.i18n('removeWarningBookmarks');
      case ProfileStatistics.AUTOFILL:
        return this.i18n('removeWarningAutofill');
      default:
        assertNotReached();
    }
  }

  protected getProfileStatisticCount_(dataType: keyof Statistics): string {
    const count = this.statistics_[dataType];
    return (count === undefined) ? this.i18n('removeWarningCalculating') :
                                   count.toString();
  }

  protected onRemoveConfirmationClicked_(e: Event) {
    e.stopPropagation();
    e.preventDefault();
    this.manageProfilesBrowserProxy_.removeProfile(
        this.profileState.profilePath);
  }

  protected onRemoveCancelClicked_() {
    this.$.removeConfirmationDialog.cancel();
    this.manageProfilesBrowserProxy_.closeProfileStatistics();
  }

  // <if expr="chromeos_lacros">
  protected onRemovePrimaryLacrosProfileCancelClicked_() {
    this.$.removePrimaryLacrosProfileDialog.cancel();
  }
  // </if>

  /**
   * Ensure any menu is closed on profile list updated.
   */
  private handleProfilesUpdated_() {
    this.$.actionMenu.close();
  }

  /**
   * Closes the remove confirmation dialog when the profile is removed.
   */
  private handleProfileRemoved_(profilePath: string) {
    this.handleProfilesUpdated_();
    if (this.profileState.profilePath === profilePath) {
      this.$.removeConfirmationDialog.close();
    }
  }

  protected onCustomizeButtonClicked_() {
    this.manageProfilesBrowserProxy_.openManageProfileSettingsSubPage(
        this.profileState.profilePath);
    this.$.actionMenu.close();
  }

  // <if expr="chromeos_lacros">
  private onAccountSettingsClicked_() {
    this.manageProfilesBrowserProxy_.openAshAccountSettingsPage();
    this.$.removeConfirmationDialog.close();
  }
  // </if>
}

declare global {
  interface HTMLElementTagNameMap {
    'profile-card-menu': ProfileCardMenuElement;
  }
}

customElements.define(ProfileCardMenuElement.is, ProfileCardMenuElement);
