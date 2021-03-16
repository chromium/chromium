// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */

(function() {

// Possible error states of the screen. Must be in the same order as
// ActiveDirectoryErrorState enum values.
/** @enum {number} */ var ACTIVE_DIRECTORY_ERROR_STATE = {
  NONE: 0,
  MACHINE_NAME_INVALID: 1,
  MACHINE_NAME_TOO_LONG: 2,
  BAD_USERNAME: 3,
  BAD_AUTH_PASSWORD: 4,
  BAD_UNLOCK_PASSWORD: 5,
};

const adLoginStep = {
  UNLOCK: 'unlock',
  CREDS: 'creds',
};

var DEFAULT_ENCRYPTION_TYPES = 'strong';

/** @typedef {Iterable<{value: string, title: string, selected: boolean,
 *                      subtitle: string}>} */
var EncryptionSelectListType;

/** @typedef {{name: string, ad_username: ?string, ad_password: ?string,
 *             computer_ou: ?string, encryption_types: ?string,
 *             computer_name_validation_regex: ?string}}
 */
var JoinConfigType;

Polymer({
  is: 'offline-ad-login-element',

  behaviors: [
    OobeI18nBehavior,
    LoginScreenBehavior,
    MultiStepBehavior
  ],

  EXTERNAL_API: [
    'reset',
    'setErrorState',
  ],

  properties: {
    /**
     * Whether the UI disabled.
     */
    disabled: {type: Boolean, value: false, observer: 'disabledObserver_'},
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
    userName: {type: String, value: '', observer: 'userNameObserver_'},
    /**
     * Label for the user input.
     */
    userNameLabel: {type: String, value: ''},
    /**
     * ID of localized welcome message on top of the UI.
     */
    adWelcomeMessageKey: String,
    /**
     * Error message for the machine name input.
     */
    machineNameError: {type: String, value: ''},
    /**
     * Error state of the UI.
     */
    errorState: {
      type: Number,
      value: ACTIVE_DIRECTORY_ERROR_STATE.NONE,
      observer: 'errorStateObserver_'
    },
    /**
     * Whether machine name input should be invalid.
     */
    machineNameInvalid:
        {type: Boolean, value: false, observer: 'machineNameInvalidObserver_'},
    /**
     * Whether username input should be invalid.
     */
    userInvalid:
        {type: Boolean, value: false, observer: 'userInvalidObserver_'},
    /**
     * Whether user password input should be invalid.
     */
    authPasswordInvalid:
        {type: Boolean, value: false, observer: 'authPasswordInvalidObserver_'},
    /**
     * Whether unlock password input should be invalid.
     */
    unlockPasswordInvalid: {
      type: Boolean,
      value: false,
      observer: 'unlockPasswordInvalidObserver_'
    },

    /**
     * Selected domain join configuration option.
     * @private {!JoinConfigType|undefined}
     */
    selectedConfigOption_: {type: Object, value: {}},

    /**
     * Verification pattern for the machine name input.
     * @private {string}
     */
    machineNameInputPattern_: {
      type: String,
      computed: 'getMachineNameInputPattern_(selectedConfigOption_)',
    },

    encryptionValue_: String,
  },

  observers: [
    'calculateUserInputValue_(selectedConfigOption_)',
  ],

  UI_STEPS: adLoginStep,

  defaultUIStep() {
    return adLoginStep.CREDS;
  },

  /** @private Used for 'More options' dialog. */
  storedOrgUnit_: String,

  /** @private Used for 'More options' dialog. */
  storedEncryption_: String,

  /**
   * Previous selected domain join configuration option.
   * @private {!JoinConfigType|undefined}
   */
  previousSelectedConfigOption_: undefined,

  /**
   * Maps encryption value to subtitle message.
   * @private {Object<string,string>}
   * */
  encryptionValueToSubtitleMap: Object,

  /**
   * Contains preselected default encryption. Does not show the warning sign for
   * that one.
   * @private
   * */
  defaultEncryption: String,

  /**
   * List of domain join configuration options.
   * @private {!Array<JoinConfigType>|undefined}
   */
  joinConfigOptions_: undefined,

  /**
   * Mutex on errorState. True when errorState is being updated from the C++
   * side.
   * @private {boolean}
   */
  errorStateLocked_: false,

  /**
   * True when we skip unlock step and show back button option.
   * @private {boolean}
   */
  backToUnlockButtonVisible_: false,

  /**
   * True when join configurations are visible.
   * @private {boolean}
   */
  joinConfigVisible_: false,

  /** @override */
  ready() {
    if (this.isDomainJoin) {
      this.setupEncList();
    } else {
      this.initializeLoginScreen('ActiveDirectoryLoginScreen', {
        resetAllowed: true,
      });
    }
  },

  onBeforeShow(data) {
    if (data) {
      this.realm = data['realm'];
      if ('emailDomain' in data)
        this.userRealm = '@' + data['emailDomain'];
    }
    if (!this.adWelcomeMessageKey)
      this.adWelcomeMessageKey = 'loginWelcomeMessage';
    this.focus();
  },

  /**
   * @param {string} username
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   */
  setErrorState(username, errorState) {
    this.userName = username;
    this.errorState = errorState;
    this.loading = false;
  },

  reset() {
    this.$.userInput.value = '';
    this.$.passwordInput.value = '';
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
  },

  setupEncList() {
    var list = /** @type {!EncryptionSelectListType}>} */
        (loadTimeData.getValue('encryptionTypesList'));
    for (var item of list) {
      this.encryptionValueToSubtitleMap[item.value] = item.subtitle;
      delete item.subtitle;
    }
    list = /** @type {!SelectListType} */ (list);
    setupSelect(
        this.$.encryptionList, list, this.onEncryptionSelected_.bind(this));
    this.defaultEncryption = /** @type {!string} */ (getSelectedValue(list));
    this.encryptionValue_ = this.defaultEncryption;
    this.machineNameError =
        loadTimeData.getString('adJoinErrorMachineNameInvalid');
  },

  focus() {
    if (this.uiStep === adLoginStep.UNLOCK) {
      this.$.unlockPasswordInput.focus();
    } else if (this.isDomainJoin && !this.$.machineNameInput.value) {
      this.$.machineNameInput.focus();
    } else if (!this.$.userInput.value) {
      this.$.userInput.focus();
    } else {
      this.$.passwordInput.focus();
    }
  },

  errorStateObserver_() {
    if (this.errorStateLocked_)
      return;
    // Prevent updateErrorStateOnInputInvalidStateChange_ from changing
    // errorState.
    this.errorStateLocked_ = true;
    this.machineNameInvalid =
        this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID ||
        this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG;
    this.userInvalid =
        this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.BAD_USERNAME;
    this.authPasswordInvalid =
        this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.BAD_AUTH_PASSWORD;
    this.unlockPasswordInvalid =
        this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.BAD_UNLOCK_PASSWORD;

    // Clear password.
    if (this.errorState == ACTIVE_DIRECTORY_ERROR_STATE.NONE)
      this.$.passwordInput.value = '';
    this.errorStateLocked_ = false;
  },

  encryptionSubtitle_() {
    return this.encryptionValueToSubtitleMap[this.encryptionValue_];
  },

  isEncryptionStrong_() {
    return this.encryptionValue_ == this.defaultEncryption;
  },

  /**
   * @param {Array<JoinConfigType>} options
   */
  setJoinConfigurationOptions(options) {
    this.backToUnlockButtonVisible_ = false;
    if (!options || options.length < 1) {
      this.joinConfigVisible_ = false;
      return;
    }
    this.joinConfigOptions_ = options;
    var selectList = [];
    for (var i = 0; i < options.length; ++i) {
      selectList.push({title: options[i].name, value: i});
    }
    setupSelect(
        this.$.joinConfigSelect, selectList,
        this.onJoinConfigSelected_.bind(this));
    this.onJoinConfigSelected_(this.$.joinConfigSelect.value);
    this.joinConfigVisible_ = true;
  },

  /** @private */
  onSubmit_() {
    if (this.disabled)
      return;

    if (this.isDomainJoin) {
      this.machineNameInvalid = !this.$.machineNameInput.validate();
      if (this.machineNameInvalid)
        return;
    }

    this.userInvalid = !this.$.userInput.validate();
    if (this.userInvalid)
      return;

    this.authPasswordInvalid = !this.$.passwordInput.validate();
    if (this.authPasswordInvalid)
      return;

    var user = /** @type {string} */ (this.$.userInput.value);
    if (!user.includes('@') && this.userRealm)
      user += this.userRealm;
    var msg = {
      'distinguished_name': this.$.orgUnitInput.value,
      'username': user,
      'password': this.$.passwordInput.value
    };
    if (this.isDomainJoin) {
      msg['machine_name'] = this.$.machineNameInput.value;
      msg['encryption_types'] = this.$.encryptionList.value;
    }
    if (this.isDomainJoin) {
      this.fire('authCompleted', msg);
    } else {
      this.loading = true;
      chrome.send('completeAdAuthentication', [msg.username, msg.password]);
    }
  },

  /** @private */
  onBackButton_() {
    this.userActed('cancel');
  },

  /** @private */
  onMoreOptionsClicked_() {
    this.disabled = true;
    this.fire('dialogShown');
    this.storedOrgUnit_ = this.$.orgUnitInput.value;
    this.storedEncryption_ = this.$.encryptionList.value;
    this.$.moreOptionsDlg.showModal();
    this.$.orgUnitInput.focus();
  },

  /** @private */
  onMoreOptionsConfirmTap_() {
    this.storedOrgUnit_ = null;
    this.storedEncryption_ = null;
    this.$.moreOptionsDlg.close();
  },

  /** @private */
  onMoreOptionsCancelTap_() {
    this.$.moreOptionsDlg.close();
  },

  /** @private */
  onMoreOptionsClosed_() {
    if (this.storedOrgUnit_ != null)
      this.$.orgUnitInput.value = this.storedOrgUnit_;
    if (this.storedEncryption_ != null) {
      this.$.encryptionList.value = this.storedEncryption_;
      this.encryptionValue_ = this.$.encryptionList.value;
    }
    this.fire('dialogHidden');
    this.disabled = false;
    this.$.moreOptionsBtn.focus();
  },

  /** @private */
  onUnlockPasswordEntered_() {
    var msg = {
      'unlock_password': this.$.unlockPasswordInput.value,
    };
    this.fire('unlockPasswordEntered', msg);
  },

  /** @private */
  onSkipClicked_() {
    this.backToUnlockButtonVisible_ = true;
    this.setUIStep(adLoginStep.CREDS);
    this.focus();
  },

  /** @private */
  onBackToUnlock_() {
    if (this.disabled)
      return;
    this.setUIStep(adLoginStep.UNLOCK);
    this.focus();
  },

  /**
   * @private
   * @param {!string} value
   * */
  onEncryptionSelected_(value) {
    this.encryptionValue_ = value;
  },

  /** @private */
  onJoinConfigSelected_(value) {
    if (this.selectedConfigOption_ == this.joinConfigOptions_[value])
      return;
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    this.previousSelectedConfigOption_ = this.selectedConfigOption_;
    this.selectedConfigOption_ = this.joinConfigOptions_[value];
    var option = this.selectedConfigOption_;
    var encryptionTypes =
        option['encryption_types'] || DEFAULT_ENCRYPTION_TYPES;
    if (!(encryptionTypes in this.encryptionValueToSubtitleMap)) {
      encryptionTypes = DEFAULT_ENCRYPTION_TYPES;
    }
    this.encryptionValue_ = encryptionTypes;
    this.focus();
  },
  /**
   * Returns pattern for checking machine name input.
   *
   * @param {Object} option Value of this.selectedConfigOption_.
   * @return {string} Regular expression.
   * @private
   */
  getMachineNameInputPattern_(option) {
    return option['computer_name_validation_regex'];
  },

  /**
   * Sets username according to |option|.
   * @param {!JoinConfigType} option Value of this.selectedConfigOption_;
   * @private
   */
  calculateUserInputValue_(option) {
    this.userName =
        this.calculateInputValue_('userInput', 'ad_username', option);
  },

  /**
   * Returns new input value when selected config option is changed.
   *
   * @param {string} inputElementId Id of the input element.
   * @param {string} key Input element key
   * @param {!JoinConfigType} option Value of this.selectedConfigOption_;
   * @return {string} New input value.
   * @private
   */
  calculateInputValue_(inputElementId, key, option) {
    if (option && key in option)
      return option[key];

    if (this.previousSelectedConfigOption_ &&
        key in this.previousSelectedConfigOption_)
      return '';

    // No changes.
    return this.$[inputElementId].value;
  },

  /**
   * Returns true if input with the given key should be disabled.
   *
   * @param {boolean} disabledAll Value of this.disabled.
   * @param {string} key Input key.
   * @param {Object} option Value of this.selectedConfigOption_;
   * @private
   */
  isInputDisabled_(key, option, disabledAll) {
    return disabledAll || (key in option);
  },

  /**
   * Returns true if "Machine name is invalid" error should be displayed.
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   */
  isMachineNameInvalid_(errorState) {
    return errorState != ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG;
  },

  getMachineNameError_(locale, errorState) {
    if (errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG)
      return this.i18nDynamic(locale, 'adJoinErrorMachineNameTooLong');
    if (errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID) {
      if (this.machineNameInputPattern_) {
        return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalidFormat');
      }
    }
    return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalid');
  },

  i18nUpdateLocale() {
    this.setupEncList();
    OobeI18nBehaviorImpl.i18nUpdateLocale.call(this);
  },

  onKeydownUnlockPassword_(e) {
    if (e.key == 'Enter') {
      if (this.$.unlockPasswordInput.value.length == 0)
        this.onSkipClicked_();
      else
        this.onUnlockPasswordEntered_();
    }
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
  },

  onKeydownMachineNameInput_(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter') {
      this.switchTo_('userInput') || this.switchTo_('passwordInput') ||
          this.onSubmit_();
    }
  },

  onKeydownUserInput_(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter')
      this.switchTo_('passwordInput') || this.onSubmit_();
  },

  userNameObserver_() {
    if (this.userRealm && this.userName &&
        this.userName.endsWith(this.userRealm)) {
      this.userName = this.userName.replace(this.userRealm, '');
    }
  },

  domainHidden(userRealm, userName) {
    return !userRealm || (userName && userName.includes('@'));
  },

  onKeydownAuthPasswordInput_(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter')
      this.onSubmit_();
  },

  switchTo_(inputId) {
    if (!this.$[inputId].disabled && this.$[inputId].value.length == 0) {
      this.$[inputId].focus();
      return true;
    }
    return false;
  },

  machineNameInvalidObserver_(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID);
  },

  userInvalidObserver_(isInvalid) {
    this.setErrorState_(isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_USERNAME);
  },

  authPasswordInvalidObserver_(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_AUTH_PASSWORD);
  },

  unlockPasswordInvalidObserver_(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_UNLOCK_PASSWORD);
  },

  setErrorState_(isInvalid, error) {
    if (this.errorStateLocked_)
      return;
    this.errorStateLocked_ = true;
    if (isInvalid)
      this.errorState = error;
    else
      this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    this.errorStateLocked_ = false;
  },

  disabledObserver_(disabled) {
    if (disabled)
      this.$.credsStep.classList.add('full-disabled');
    else
      this.$.credsStep.classList.remove('full-disabled');
  },
});
})();
