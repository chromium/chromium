// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-smb-share-dialog' is a component for adding an SMB Share.
 *
 * This component can only be used once to add an SMB share, and must be
 * destroyed when finished, and re-created when shown again.
 */

import '//resources/ash/common/cr_elements/cr_button/cr_button.js';
import '//resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/ash/common/cr_elements/cr_input/cr_input.js';
import '//resources/ash/common/cr_elements/cr_searchable_drop_down/cr_searchable_drop_down.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/ash/common/cr_elements/cr_shared_style.css.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import '//resources/ash/common/cr_elements/md_select.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/cros_components/checkbox/checkbox.js';

import {I18nBehavior} from '//resources/ash/common/i18n_behavior.js';
import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/ash/common/web_ui_listener_behavior.js';
import {Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './add_smb_share_dialog.html.js';
import {SmbAuthMethod, SmbBrowserProxy, SmbBrowserProxyImpl, SmbMountResult} from './smb_browser_proxy.js';

/** @enum{number} */
const MountErrorType = {
  NO_ERROR: 0,
  CREDENTIAL_ERROR: 1,
  PATH_ERROR: 2,
  GENERAL_ERROR: 3,
};

/**
 * Regular expression that matches SMB share URLs of the form
 * smb://server/share or \\server\share. This is a coarse regexp intended for
 * quick UI feedback and does not reject all invalid URLs.
 *
 * @type {!RegExp}
 */
const SMB_SHARE_URL_REGEX =
    /^((smb:\/\/[^\/]+\/[^\/].*)|(\\\\[^\\]+\\[^\\].*))$/;

Polymer({
  is: 'add-smb-share-dialog',

  _template: getTemplate(),

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    lastUrl: {
      type: String,
      value: '',
    },

    shouldOpenFileManagerAfterMount: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    mountUrl_: {
      type: String,
      value: '',
      observer: 'onURLChanged_',
    },

    /** @private {string} */
    mountName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    username_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    password_: {
      type: String,
      value: '',
    },
    /** @private {!Array<string>}*/
    discoveredShares_: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @private */
    discoveryActive_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isKerberosEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isKerberosEnabled');
      },
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private */
    authenticationMethod_: {
      type: String,
      value() {
        // SSO not supported in guest mode. TODO(crbug/1186188): Enable SSO
        // option for MGS on file share UI, after fixing authentication error.
        if (loadTimeData.getBoolean('isGuest')) {
          return SmbAuthMethod.CREDENTIALS;
        }

        // SSO only supported if Kerberos is enabled by policy.
        if (loadTimeData.getBoolean('isKerberosEnabled')) {
          return SmbAuthMethod.KERBEROS;
        }

        return SmbAuthMethod.CREDENTIALS;
      },
      observer: 'onAuthenticationMethodChanged_',
    },

    /** @private */
    generalErrorText_: String,

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {!MountErrorType} */
    currentMountError_: {
      type: Number,
      value: MountErrorType.NO_ERROR,
    },
  },

  /** @private {?SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.browserProxy_.startDiscovery();
    this.$.dialog.showModal();

    this.addWebUIListener('on-shares-found', this.onSharesFound_.bind(this));
    this.mountUrl_ = this.lastUrl;
  },

  /** @private */
  cancel_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAddButtonTap_() {
    this.resetErrorState_();
    const saveCredentialsCheckbox = this.$$(
        this.isCrosComponentsEnabled_() ? '#saveCredentialsCheckboxJelly' :
                                          '#saveCredentialsCheckbox');
    this.inProgress_ = true;
    this.browserProxy_
        .smbMount(
            this.mountUrl_, this.mountName_.trim(), this.username_,
            this.password_, this.authenticationMethod_,
            this.shouldOpenFileManagerAfterMount,
            saveCredentialsCheckbox.checked)
        .then(result => {
          this.onAddShare_(result);
        });
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onURLChanged_(newValue, oldValue) {
    this.resetErrorState_();
    const parts = this.mountUrl_.split('\\');
    this.mountName_ = parts[parts.length - 1];
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onAuthenticationMethodChanged_(newValue, oldValue) {
    this.resetErrorState_();
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddShare_() {
    return !!this.mountUrl_ && !this.inProgress_ && this.isShareUrlValid_();
  },

  /**
   * @param {!Array<string>} newSharesDiscovered New shares that have been
   * discovered since the last call.
   * @param {boolean} done Whether share discovery has finished.
   * @private
   */
  onSharesFound_(newSharesDiscovered, done) {
    this.discoveredShares_ = this.discoveredShares_.concat(newSharesDiscovered);
    this.discoveryActive_ = !done;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialUI_() {
    return this.authenticationMethod_ === SmbAuthMethod.CREDENTIALS;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowAuthenticationUI_() {
    // SSO not supported in guest mode. TODO(crbug/1186188): Enable SSO option
    // for MGS on file share UI, after fixing authentication error.
    if (this.isGuest_) {
      return false;
    }

    // SSO only supported if Kerberos is enabled by policy.
    return this.isKerberosEnabled_;
  },

  /**
   * @param {SmbMountResult} result
   * @private
   */
  onAddShare_(result) {
    this.inProgress_ = false;

    // Success case. Close dialog.
    if (result === SmbMountResult.SUCCESS) {
      this.dispatchEvent(new CustomEvent(
          'smb-successfully-mounted-once', {bubbles: true, composed: true}));

      this.$.dialog.close();
      return;
    }

    switch (result) {
      // Credential Error
      case SmbMountResult.AUTHENTICATION_FAILED:
        if (this.authenticationMethod_ === SmbAuthMethod.KERBEROS) {
          this.setGeneralError_(
              loadTimeData.getString('smbShareAddedAuthFailedMessage'));
        } else {
          this.setCredentialError_(
              loadTimeData.getString('smbShareAddedAuthFailedMessage'));
        }
        break;
      case SmbMountResult.INVALID_USERNAME:
        this.setCredentialError_(
            loadTimeData.getString('smbShareAddedInvalidUsernameMessage'));
        break;

      // Path Errors
      case SmbMountResult.NOT_FOUND:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedNotFoundMessage'));
        break;
      case SmbMountResult.INVALID_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidURLMessage'));
        break;
      case SmbMountResult.INVALID_SSO_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidSSOURLMessage'));
        break;

      // General Errors
      case SmbMountResult.UNSUPPORTED_DEVICE:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedUnsupportedDeviceMessage'));
        break;
      case SmbMountResult.MOUNT_EXISTS:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedMountExistsMessage'));
        break;
      case SmbMountResult.TOO_MANY_OPENED:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedTooManyMountsMessage'));
        break;
      default:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedErrorMessage'));
    }
  },

  /** @private */
  resetErrorState_() {
    this.currentMountError_ = MountErrorType.NO_ERROR;
    this.$.address.errorMessage = '';
    this.$.password.errorMessage = '';
    this.generalErrorText_ = '';
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setCredentialError_(errorMessage) {
    this.$.password.errorMessage = errorMessage;
    this.currentMountError_ = MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setGeneralError_(errorMessage) {
    this.generalErrorText_ = errorMessage;
    this.currentMountError_ = MountErrorType.GENERAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setPathError_(errorMessage) {
    this.$.address.errorMessage = errorMessage;
    this.currentMountError_ = MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialError_() {
    return this.currentMountError_ === MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGeneralError_() {
    return this.currentMountError_ === MountErrorType.GENERAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPathError_() {
    return this.currentMountError_ === MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  isShareUrlValid_() {
    if (!this.mountUrl_ || this.shouldShowPathError_()) {
      return false;
    }
    return SMB_SHARE_URL_REGEX.test(this.mountUrl_);
  },

  isCrosComponentsEnabled_() {
    return !!loadTimeData.getBoolean('isCrosComponentsEnabled');
  },
});
