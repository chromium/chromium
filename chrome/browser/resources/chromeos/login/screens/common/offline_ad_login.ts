// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../../components/oobe_icons.html.js';
import '../../components/buttons/oobe_back_button.js';
import '../../components/buttons/oobe_next_button.js';
import '../../components/buttons/oobe_text_button.js';
import '../../components/common_styles/oobe_common_styles.css.js';
import '../../components/common_styles/oobe_dialog_host_styles.css.js';

import {CrDialogElement} from '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import {CrInputElement} from '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {mixinBehaviors, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LoginScreenBehavior, LoginScreenBehaviorInterface} from '../../components/behaviors/login_screen_behavior.js';
import {MultiStepBehavior, MultiStepBehaviorInterface} from '../../components/behaviors/multi_step_behavior.js';
import {OobeI18nBehavior, OobeI18nBehaviorInterface} from '../../components/behaviors/oobe_i18n_behavior.js';
import {OobeAdaptiveDialog} from '../../components/dialogs/oobe_adaptive_dialog.js';
import {GaiaButton} from '../../components/gaia_button.js';
import type {SelectListType} from '../../components/oobe_select.js';
import {getSelectedValue, setupSelect} from '../../components/oobe_select.js';

import {getTemplate} from './offline_ad_login.html.js';


// The definitions below (JoinConfigType, ActiveDirectoryErrorState) are
// used in enterprise_enrollment.js as well.
export interface JoinConfigType {
  name: string;
  ad_username?: string;
  ad_password?: string;
  computer_ou?: string;
  encryption_types?: string;
  computer_name_validation_regex?: string;
}

// Possible error states of the screen. Must be in the same order as
// ActiveDirectoryErrorState enum values. Used in enterprise_enrollment
export enum ActiveDirectoryErrorState {
  NONE = 0,
  MACHINE_NAME_INVALID = 1,
  MACHINE_NAME_TOO_LONG = 2,
  BAD_USERNAME = 3,
  BAD_AUTH_PASSWORD = 4,
  BAD_UNLOCK_PASSWORD = 5,
}

// Used by enterprise_enrollment.js
// eslint-disable-next-line @typescript-eslint/naming-convention
export enum ADLoginStep {
  UNLOCK = 'unlock',
  CREDS = 'creds',
}

const DEFAULT_ENCRYPTION_TYPES = 'strong';

interface EncryptionSelectListItem {
  value: string;
  title: string;
  selected: boolean;
  subtitle?: string;
}

type EncryptionSelectListType = EncryptionSelectListItem[];

interface OfflineAdLoginOnBeforeShowData {
  realm: string;
  emailDomain: string;
}

const OfflineAdLoginBase =
    mixinBehaviors(
        [OobeI18nBehavior, LoginScreenBehavior, MultiStepBehavior],
        PolymerElement) as {
      new (): PolymerElement & OobeI18nBehaviorInterface &
          LoginScreenBehaviorInterface & MultiStepBehaviorInterface,
    };

export class OfflineAdLogin extends OfflineAdLoginBase {
  static get is() {
    return 'offline-ad-login-element' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * Whether the UI disabled.
       */
      disabled: {type: Boolean, value: false, observer: 'disabledObserver'},

      /**
       * Whether the loading UI shown.
       */
      loading: {type: Boolean, value: false},

      /**
       * Whether the screen is for domain join.
       */
      isDomainJoin: {type: Boolean, value: false},

      /**
       * The kerberos realm (AD Domain), the machine is part of.
       */
      realm: {type: String},

      /**
       * The user kerberos default realm. Used for autocompletion.
       */
      userRealm: {type: String, value: ''},

      /**
       * Predefined machine name.
       */
      machineName: {type: String, value: ''},

      /**
       * Predefined user name.
       */
      userName: {type: String, value: '', observer: 'userNameObserver'},

      /**
       * Label for the user input.
       */
      userNameLabel: {type: String, value: ''},

      /**
       * ID of localized welcome message on top of the UI.
       */
      adWelcomeMessageKey: {type: String, value: 'loginWelcomeMessage'},

      /**
       * Error message for the machine name input.
       */
      machineNameError: {type: String, value: ''},

      /**
       * Error state of the UI.
       */
      errorState: {
        type: Number,
        value: ActiveDirectoryErrorState.NONE,
        observer: 'errorStateObserver',
      },

      /**
       * Whether machine name input should be invalid.
       */
      machineNameInvalid: {
        type: Boolean,
        value: false,
        observer: 'machineNameInvalidObserver',
      },

      /**
       * Whether username input should be invalid.
       */
      userInvalid:
          {type: Boolean, value: false, observer: 'userInvalidObserver'},

      /**
       * Whether user password input should be invalid.
       */
      authPasswordInvalid: {
        type: Boolean,
        value: false,
        observer: 'authPasswordInvalidObserver',
      },

      /**
       * Whether unlock password input should be invalid.
       */
      unlockPasswordInvalid: {
        type: Boolean,
        value: false,
        observer: 'unlockPasswordInvalidObserver',
      },

      /**
       * Selected domain join configuration option.
       */
      selectedConfigOption: {type: Object, value: {}},

      /**
       * Verification pattern for the machine name input.
       */
      machineNameInputPattern: {
        type: String,
        computed: 'getmachineNameInputPattern(selectedConfigOption)',
      },

      encryptionValue: String,
    };
  }

  disabled: boolean;
  loading: boolean;
  isDomainJoin: boolean;
  realm: string;
  userRealm: string;
  machineName: string;
  userName: string;
  userNameLabel: string;
  adWelcomeMessageKey: string;
  machineNameError: string;
  errorState: number;
  machineNameInvalid: boolean;
  userInvalid: boolean;
  authPasswordInvalid: boolean;
  unlockPasswordInvalid: boolean;
  private selectedConfigOption: JoinConfigType|undefined;
  private machineNameInputPattern: string;
  encryptionValue: string;
  private storedOrgUnit: string;
  private storedEncryption: string;
  private previousselectedConfigOption: JoinConfigType|undefined;
  private encryptionValueToSubtitleMap: Record<string, string>;
  private defaultEncryption: string;
  private joinConfigOptions: JoinConfigType[]|undefined;
  private errorStateLocked: boolean;
  private backToUnlockButtonVisible: boolean;
  private joinConfigVisible: boolean;

  static get observers() {
    return ['calculateUserInputValue(selectedConfigOption)'];
  }

  constructor() {
    super();

    this.machineName = '';

    /**
     * Used for 'More options' dialog.
     */
    this.storedOrgUnit = '';

    /**
     * Used for 'More options' dialog.
     */
    this.storedEncryption = '';

    /**
     * Previous selected domain join configuration option.
     */
    this.previousselectedConfigOption = undefined;

    /**
     * Maps encryption value to subtitle message.
     */
    this.encryptionValueToSubtitleMap = {};

    /**
     * Contains preselected default encryption. Does not show the warning sign
     * for that one.
     */
    this.defaultEncryption = '';

    /**
     * List of domain join configuration options.
     */
    this.joinConfigOptions = undefined;

    /**
     * Mutex on errorState. True when errorState is being updated from the C++
     * side.
     */
    this.errorStateLocked = false;

    /**
     * True when we skip unlock step and show back button option.
     */
    this.backToUnlockButtonVisible = false;

    /**
     * True when join configurations are visible.
     */
    this.joinConfigVisible = false;
  }

  override get EXTERNAL_API(): string[] {
    return ['reset', 'setErrorState'];
  }

  override get UI_STEPS() {
    return ADLoginStep;
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  override defaultUIStep(): string {
    return ADLoginStep.CREDS;
  }

  override ready(): void {
    super.ready();
    if (this.isDomainJoin) {
      this.setupEncList();
    } else {
      this.initializeLoginScreen('ActiveDirectoryLoginScreen');
    }
  }

  onBeforeShow(data: OfflineAdLoginOnBeforeShowData): void {
    if (!data) {
      return;
    }
    if ('realm' in data) {
      this.realm = data['realm']!;
      if ('emailDomain' in data) {
        this.userRealm = '@' + data['emailDomain'];
      }
    }
    this.focus();
  }

  setErrorState(username: string, errorState: ActiveDirectoryErrorState): void {
    this.userName = username;
    this.errorState = errorState;
    this.loading = false;
  }

  reset(): void {
    this.getUserInput().value = '';
    this.getPasswordInput().value = '';
    this.errorState = ActiveDirectoryErrorState.NONE;
  }

  setupEncList(): void {
    const list = loadTimeData.getValue('encryptionTypesList') as
        EncryptionSelectListType;
    for (const item of list) {
      this.encryptionValueToSubtitleMap[item.value] = item.subtitle!;
      delete item.subtitle;
    }
    const selectList: SelectListType = list;
    setupSelect(
        this.getEncryptionList(), selectList, this.onEncryptionSelected.bind(this));
    this.defaultEncryption = getSelectedValue(selectList)!;
    this.encryptionValue = this.defaultEncryption;
    this.machineNameError =
        loadTimeData.getString('adJoinErrorMachineNameInvalid');
  }

  override focus(): void {
    if (this.uiStep === ADLoginStep.UNLOCK) {
      this.getUnlockPasswordInput().focus();
    } else if (this.isDomainJoin && !this.getMachineNameInput().value) {
      this.getMachineNameInput().focus();
    } else if (!this.getUserInput().value) {
      this.getUserInput().focus();
    } else {
      this.getPasswordInput().focus();
    }
  }

  errorStateObserver(): void {
    if (this.errorStateLocked) {
      return;
    }
    // Prevent updateErrorStateOnInputInvalidStateChange_ from changing
    // errorState.
    this.errorStateLocked = true;
    this.machineNameInvalid =
        this.errorState === ActiveDirectoryErrorState.MACHINE_NAME_INVALID ||
        this.errorState === ActiveDirectoryErrorState.MACHINE_NAME_TOO_LONG;
    this.userInvalid =
        this.errorState === ActiveDirectoryErrorState.BAD_USERNAME;
    this.authPasswordInvalid =
        this.errorState === ActiveDirectoryErrorState.BAD_AUTH_PASSWORD;
    this.unlockPasswordInvalid =
        this.errorState === ActiveDirectoryErrorState.BAD_UNLOCK_PASSWORD;

    // Clear password.
    if (this.errorState === ActiveDirectoryErrorState.NONE) {
      this.getPasswordInput().value = '';
    }
    this.errorStateLocked = false;
  }

  encryptionSubtitle(): string {
    return this.encryptionValueToSubtitleMap[this.encryptionValue];
  }

  isEncryptionStrong(): boolean {
    return this.encryptionValue === this.defaultEncryption;
  }

  setJoinConfigurationOptions(options: JoinConfigType[]): void {
    this.backToUnlockButtonVisible = false;
    if (!options || options.length < 1) {
      this.joinConfigVisible = false;
      return;
    }
    this.joinConfigOptions = options;
    const selectList: SelectListType = [];
    for (let i = 0; i < options.length; ++i) {
      selectList.push({
        title: options[i].name,
        value: i.toString(),
        selected: false,
      });
    }
    setupSelect(
        this.getJoinConfigSelect(), selectList,
        this.onJoinConfigSelected.bind(this));
    this.onJoinConfigSelected(this.getJoinConfigSelect().value);
    this.joinConfigVisible = true;
  }

  private onSubmit(): void {
    if (this.disabled) {
      return;
    }

    if (this.isDomainJoin) {
      this.machineNameInvalid = !this.getMachineNameInput().validate();
      if (this.machineNameInvalid) {
        return;
      }
    }

    this.userInvalid = !this.getUserInput().validate();
    if (this.userInvalid) {
      return;
    }

    this.authPasswordInvalid = !this.getPasswordInput().validate();
    if (this.authPasswordInvalid) {
      return;
    }

    let user = /** @type {string} */ (this.getUserInput().value);
    const password = /** @type {string} / */ (this.getPasswordInput().value);
    if (!user.includes('@') && this.userRealm) {
      user += this.userRealm;
    }

    if (this.isDomainJoin) {
      const msg = {
        'distinguished_name': this.storedOrgUnit,
        'username': user,
        'password': password,
        'machine_name': this.getMachineNameInput().value,
        'encryption_types': this.storedEncryption,
      };
      this.dispatchEvent(new CustomEvent(
          'authCompletedAd', {bubbles: true, composed: true, detail: msg}));
    } else {
      this.loading = true;
      this.userActed(['completeAdAuthentication', user, password]);
    }
  }

  private onBackButton(): void {
    this.userActed('cancel');
  }

  private onMoreOptionsClicked(): void {
    this.disabled = true;
    this.dispatchEvent(
        new CustomEvent('dialogShown', {bubbles: true, composed: true}));
    this.getMoreOptionsDialog().showModal();
    this.getOrgUnitInput().focus();
  }

  private onMoreOptionsConfirmTapped(): void {
    this.storedOrgUnit = this.getOrgUnitInput().value;
    this.storedEncryption = this.getEncryptionList().value;
    this.getMoreOptionsDialog().close();
  }

  private onMoreOptionsCancelTapped(): void {
    // Restore previous values
    this.getOrgUnitInput().value = this.storedOrgUnit;
    this.getEncryptionList().value = this.storedEncryption;
    this.getMoreOptionsDialog().close();
  }

  private onMoreOptionsClosed(): void {
    this.dispatchEvent(
        new CustomEvent('dialogHidden', {bubbles: true, composed: true}));
    this.disabled = false;
    const moreOptionsBtn = this.shadowRoot?.querySelector<GaiaButton>(
        '#moreOptionsBtn',
    );
    assert(moreOptionsBtn instanceof GaiaButton);
    moreOptionsBtn.focus();
  }

  private onUnlockPasswordEntered(): void {
    const msg = {
      'unlock_password': this.getUnlockPasswordInput().value,
    };
    this.dispatchEvent(new CustomEvent(
        'unlockPasswordEntered', {bubbles: true, composed: true, detail: msg}));
  }

  private onSkipClicked(): void {
    this.backToUnlockButtonVisible = true;
    this.setUIStep(ADLoginStep.CREDS);
    this.focus();
  }

  private onBackToUnlock(): void {
    if (this.disabled) {
      return;
    }
    this.setUIStep(ADLoginStep.UNLOCK);
    this.focus();
  }

  private onEncryptionSelected(value: string): void {
    this.encryptionValue = value;
  }

  private onJoinConfigSelected(value: any): void {
    if (this.selectedConfigOption === this.joinConfigOptions![value]) {
      return;
    }
    this.errorState = ActiveDirectoryErrorState.NONE;
    this.previousselectedConfigOption = this.selectedConfigOption;
    this.selectedConfigOption = this.joinConfigOptions![value];
    const option = this.selectedConfigOption;
    let encryptionTypes =
        option['encryption_types'] || DEFAULT_ENCRYPTION_TYPES;
    if (!(encryptionTypes in this.encryptionValueToSubtitleMap)) {
      encryptionTypes = DEFAULT_ENCRYPTION_TYPES;
    }
    this.encryptionValue = encryptionTypes;
    this.focus();
  }

  /**
   * Returns pattern for checking machine name input.
   *
   * @param option Value of this.selectedConfigOption.
   * @return Regular expression.
   */
  private getmachineNameInputPattern(option: JoinConfigType): string {
    return option['computer_name_validation_regex']!;
  }

  /**
   * Sets username according to |option|.
   * @param option Value of this.selectedConfigOption;
   */
  private calculateUserInputValue(option: JoinConfigType): void {
    this.userName =
        this.calculateInputValue('userInput', 'ad_username', option);
  }

  /**
   * Returns new input value when selected config option is changed.
   *
   * @param inputElementId Id of the input element.
   * @param key Input element key
   * @param option Value of this.selectedConfigOption;
   * @return New input value.
   */
  private calculateInputValue(
      inputElementId: string, key: string, option: JoinConfigType): string {
    if (option && key in option) {
      return option[key as keyof typeof option]!;
    }

    if (this.previousselectedConfigOption &&
        key in this.previousselectedConfigOption) {
      return '';
    }

    if (!this.shadowRoot) {
      return '';
    }

    // No changes.
    return this.getInputElementById(inputElementId).value;
  }

  /**
   * Returns true if input with the given key should be disabled.
   *
   * @param disabledAll Value of this.disabled.
   * @param key Input key.
   * @param option Value of this.selectedConfigOption;
   */
  private isInputDisabled(key: string, option: Object, disabledAll: boolean):
      boolean {
    return disabledAll || (key in option);
  }

  /**
   * Returns true if "Machine name is invalid" error should be displayed.
   */
  isMachineNameInvalid(errorState: ActiveDirectoryErrorState): boolean {
    return errorState !== ActiveDirectoryErrorState.MACHINE_NAME_TOO_LONG;
  }

  getMachineNameError(locale: string, errorState: ActiveDirectoryErrorState):
      string {
    if (errorState === ActiveDirectoryErrorState.MACHINE_NAME_TOO_LONG) {
      return this.i18nDynamic(locale, 'adJoinErrorMachineNameTooLong');
    }
    if (errorState === ActiveDirectoryErrorState.MACHINE_NAME_INVALID) {
      if (this.machineNameInputPattern) {
        return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalidFormat');
      }
    }
    return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalid');
  }

  onKeydownUnlockPassword(e: KeyboardEvent): void {
    if (e.key === 'Enter') {
      if (this.getUnlockPasswordInput().value.length === 0) {
        this.onSkipClicked();
      } else {
        this.onUnlockPasswordEntered();
      }
    }
    this.errorState = ActiveDirectoryErrorState.NONE;
  }

  onKeydownMachineNameInput(e: KeyboardEvent): void {
    this.errorState = ActiveDirectoryErrorState.NONE;
    if (e.key === 'Enter') {
      this.switchTo('userInput') || this.switchTo('passwordInput') ||
          this.onSubmit();
    }
  }

  onKeydownUserInput(e: KeyboardEvent): void {
    this.errorState = ActiveDirectoryErrorState.NONE;
    if (e.key === 'Enter') {
      this.switchTo('passwordInput') || this.onSubmit();
    }
  }

  userNameObserver(): void {
    if (this.userRealm && this.userName &&
        this.userName.endsWith(this.userRealm)) {
      this.userName = this.userName.replace(this.userRealm, '');
    }
  }

  domainHidden(userRealm: string, userName: string): boolean {
    return !userRealm || (userName.length > 0 && userName.includes('@'));
  }

  onKeydownAuthPasswordInput(e: KeyboardEvent): void {
    this.errorState = ActiveDirectoryErrorState.NONE;
    if (e.key === 'Enter') {
      this.onSubmit();
    }
  }

  switchTo(inputId: string): boolean {
    const inputElement = this.getInputElementById(inputId);
    if (!inputElement.disabled && inputElement.value.length === 0) {
      inputElement.focus();
      return true;
    }
    return false;
  }

  machineNameInvalidObserver(isInvalid: boolean): void {
    this.setErrorStateActiveDirectory(
        isInvalid, ActiveDirectoryErrorState.MACHINE_NAME_INVALID);
  }

  userInvalidObserver(isInvalid: boolean): void {
    this.setErrorStateActiveDirectory(
        isInvalid, ActiveDirectoryErrorState.BAD_USERNAME);
  }

  authPasswordInvalidObserver(isInvalid: boolean): void {
    this.setErrorStateActiveDirectory(
        isInvalid, ActiveDirectoryErrorState.BAD_AUTH_PASSWORD);
  }

  unlockPasswordInvalidObserver(isInvalid: boolean): void {
    this.setErrorStateActiveDirectory(
        isInvalid, ActiveDirectoryErrorState.BAD_UNLOCK_PASSWORD);
  }

  setErrorStateActiveDirectory(
      isInvalid: boolean, error: ActiveDirectoryErrorState): void {
    if (this.errorStateLocked) {
      return;
    }
    this.errorStateLocked = true;
    if (isInvalid) {
      this.errorState = error;
    } else {
      this.errorState = ActiveDirectoryErrorState.NONE;
    }
    this.errorStateLocked = false;
  }

  disabledObserver(disabled: boolean): void {
    const credsStep = this.shadowRoot?.querySelector<OobeAdaptiveDialog>(
        '#credsStep',
    );
    assert(credsStep instanceof OobeAdaptiveDialog);
    if (disabled) {
      credsStep.classList.add('full-disabled');
    } else {
      credsStep.classList.remove('full-disabled');
    }
  }

  private getInputElementById(inputId: string): CrInputElement {
    const input = this.shadowRoot?.querySelector<CrInputElement>(
        '#' + inputId,
    );
    assert(input instanceof CrInputElement);
    return input;
  }

  private getUserInput(): CrInputElement {
    return this.getInputElementById('userInput');
  }

  private getPasswordInput(): CrInputElement {
    return this.getInputElementById('passwordInput');
  }

  private getUnlockPasswordInput(): CrInputElement {
    return this.getInputElementById('unlockPasswordInput');
  }

  private getMachineNameInput(): CrInputElement {
    return this.getInputElementById('machineNameInput');
  }

  private getOrgUnitInput(): CrInputElement {
    return this.getInputElementById('orgUnitInput');
  }

  private getSelectElementById(selectId: string): HTMLSelectElement {
    const selectElement = this.shadowRoot?.querySelector<HTMLSelectElement>(
        '#' + selectId,
    );
    assert(selectElement instanceof HTMLSelectElement);
    return selectElement;
  }

  private getEncryptionList(): HTMLSelectElement {
    return this.getSelectElementById('encryptionList');
  }

  private getJoinConfigSelect(): HTMLSelectElement {
    return this.getSelectElementById('joinConfigSelect');
  }

  private getMoreOptionsDialog(): CrDialogElement {
    const moreOptionsDialog = this.shadowRoot?.querySelector<CrDialogElement>(
        '#moreOptionsDlg',
    );
    assert(moreOptionsDialog instanceof CrDialogElement);
    return moreOptionsDialog;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OfflineAdLogin.is]: OfflineAdLogin;
  }
}

customElements.define(OfflineAdLogin.is, OfflineAdLogin);
