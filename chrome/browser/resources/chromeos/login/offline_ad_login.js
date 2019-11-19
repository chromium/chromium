// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying AD domain joining and AD
 * Authenticate user screens.
 */
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
  is: 'offline-ad-login',

  behaviors: [I18nBehavior, OobeDialogHostBehavior],

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
     * Whether the unlock option should be shown.
     */
    unlockPasswordStep: {type: Boolean, value: false, observer: 'focus'},
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
     * Welcome message on top of the UI.
     */
    adWelcomeMessage: String,
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

  /** @override */
  ready: function() {
    if (!this.isDomainJoin)
      return;
    this.setupEncList();
  },

  setupEncList: function() {
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

  focus: function() {
    if (this.unlockPasswordStep) {
      this.$.unlockPasswordInput.focus();
    } else if (this.isDomainJoin && !this.$.machineNameInput.value) {
      this.$.machineNameInput.focus();
    } else if (!this.$.userInput.value) {
      this.$.userInput.focus();
    } else {
      this.$.passwordInput.focus();
    }
  },

  errorStateObserver_: function() {
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

  encryptionSubtitle_: function() {
    return this.encryptionValueToSubtitleMap[this.encryptionValue_];
  },

  isEncryptionStrong_: function() {
    return this.encryptionValue_ == this.defaultEncryption;
  },

  /**
   * @param {Array<JoinConfigType>} options
   */
  setJoinConfigurationOptions: function(options) {
    this.$.backToUnlockButton.hidden = true;
    if (!options || options.length < 1) {
      this.$.joinConfig.hidden = true;
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
    this.$.joinConfig.hidden = false;
  },

  /** @private */
  onSubmit_: function() {
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
    this.fire('authCompleted', msg);
  },

  /** @private */
  onBackButton_: function() {
    this.$.passwordInput.value = '';
    this.fire('cancel');
  },

  /** @private */
  onMoreOptionsClicked_: function() {
    this.disabled = true;
    this.fire('dialogShown');
    this.storedOrgUnit_ = this.$.orgUnitInput.value;
    this.storedEncryption_ = this.$.encryptionList.value;
    this.$.moreOptionsDlg.showModal();
    this.$.orgUnitInput.focus();
  },

  /** @private */
  onMoreOptionsConfirmTap_: function() {
    this.storedOrgUnit_ = null;
    this.storedEncryption_ = null;
    this.$.moreOptionsDlg.close();
  },

  /** @private */
  onMoreOptionsCancelTap_: function() {
    this.$.moreOptionsDlg.close();
  },

  /** @private */
  onMoreOptionsClosed_: function() {
    if (this.storedOrgUnit_ != null)
      this.$.orgUnitInput.value = this.storedOrgUnit_;
    if (this.storedEncryption_ != null) {
      this.$.encryptionList.value = this.storedEncryption_;
      this.encryptionValue_ = this.$.encryptionList.value;
    }
    this.fire('dialogHidden');
    this.disabled = false;
    this.focus();
  },

  /** @private */
  onUnlockPasswordEntered_: function() {
    var msg = {
      'unlock_password': this.$.unlockPasswordInput.value,
    };
    this.fire('unlockPasswordEntered', msg);
  },

  /** @private */
  onSkipClicked_: function() {
    this.$.backToUnlockButton.hidden = false;
    this.unlockPasswordStep = false;
  },

  /** @private */
  onBackToUnlock_: function() {
    if (this.disabled)
      return;
    this.unlockPasswordStep = true;
  },

  /**
   * @private
   * @param {!string} value
   * */
  onEncryptionSelected_: function(value) {
    this.encryptionValue_ = value;
  },

  /** @private */
  onJoinConfigSelected_: function(value) {
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
  getMachineNameInputPattern_: function(option) {
    return option['computer_name_validation_regex'];
  },

  /**
   * Sets username according to |option|.
   * @param {!JoinConfigType} option Value of this.selectedConfigOption_;
   * @private
   */
  calculateUserInputValue_: function(option) {
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
  calculateInputValue_: function(inputElementId, key, option) {
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
  isInputDisabled_: function(key, option, disabledAll) {
    return disabledAll || (key in option);
  },

  /**
   * Returns true if "Machine name is invalid" error should be displayed.
   * @param {ACTIVE_DIRECTORY_ERROR_STATE} errorState
   */
  isMachineNameInvalid_: function(errorState) {
    return errorState != ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG;
  },

  getMachineNameError_: function(locale, errorState) {
    if (errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_TOO_LONG)
      return this.i18nDynamic(locale, 'adJoinErrorMachineNameTooLong');
    if (errorState == ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID) {
      if (this.machineNameInputPattern_) {
        return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalidFormat');
      }
    }
    return this.i18nDynamic(locale, 'adJoinErrorMachineNameInvalid');
  },

  i18nUpdateLocale: function() {
    this.setupEncList();
    I18nBehavior.i18nUpdateLocale.call(this);
  },

  onKeydownUnlockPassword_: function(e) {
    if (e.key == 'Enter') {
      if (this.$.unlockPasswordInput.value.length == 0)
        this.onSkipClicked_();
      else
        this.onUnlockPasswordEntered_();
    }
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
  },

  onKeydownMachineNameInput_: function(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter') {
      this.switchTo_('userInput') || this.switchTo_('passwordInput') ||
          this.onSubmit_();
    }
  },

  onKeydownUserInput_: function(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter')
      this.switchTo_('passwordInput') || this.onSubmit_();
  },

  userNameObserver_: function() {
    if (this.userRealm && this.userName &&
        this.userName.endsWith(this.userRealm)) {
      this.userName = this.userName.replace(this.userRealm, '');
    }
  },

  domainHidden: function(userRealm, userName) {
    return !userRealm || (userName && userName.includes('@'));
  },

  onKeydownAuthPasswordInput_: function(e) {
    this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    if (e.key == 'Enter')
      this.onSubmit_();
  },

  switchTo_: function(inputId) {
    if (!this.$[inputId].disabled && this.$[inputId].value.length == 0) {
      this.$[inputId].focus();
      return true;
    }
    return false;
  },

  machineNameInvalidObserver_: function(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.MACHINE_NAME_INVALID);
  },

  userInvalidObserver_: function(isInvalid) {
    this.setErrorState_(isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_USERNAME);
  },

  authPasswordInvalidObserver_: function(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_AUTH_PASSWORD);
  },

  unlockPasswordInvalidObserver_: function(isInvalid) {
    this.setErrorState_(
        isInvalid, ACTIVE_DIRECTORY_ERROR_STATE.BAD_UNLOCK_PASSWORD);
  },

  setErrorState_: function(isInvalid, error) {
    if (this.errorStateLocked_)
      return;
    this.errorStateLocked_ = true;
    if (isInvalid)
      this.errorState = error;
    else
      this.errorState = ACTIVE_DIRECTORY_ERROR_STATE.NONE;
    this.errorStateLocked_ = false;
  },

  disabledObserver_: function(disabled) {
    if (disabled)
      this.$.credsStep.classList.add('full-disabled');
    else
      this.$.credsStep.classList.remove('full-disabled');
  },
});
