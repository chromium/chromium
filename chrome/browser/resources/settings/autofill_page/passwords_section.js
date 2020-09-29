// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-section' is the collapsible section containing
 * the list of saved passwords as well as the list of sites that will never
 * save any passwords.
 */

/** @typedef {!{model: !{item: !chrome.passwordsPrivate.ExceptionEntry}}} */
let ExceptionEntryEntryEvent;

import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import {IronA11yKeysBehavior} from 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import '../controls/extension_controlled_indicator.m.js';
import '../controls/settings_toggle_button.m.js';
import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.m.js';
import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.m.js';
import '../prefs/prefs.m.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.m.js';
import {routes} from '../route.js';
import {MergeExceptionsStoreCopiesBehavior} from './merge_exceptions_store_copies_behavior.js';
import {MergePasswordsStoreCopiesBehavior} from './merge_passwords_store_copies_behavior.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {Router} from '../router.m.js';
import '../settings_shared_css.m.js';
import '../site_favicon.js';
import {PasswordCheckBehavior} from './password_check_behavior.js';
import './password_list_item.js';
import './passwords_list_handler.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import './passwords_export_dialog.js';
import './passwords_shared_css.js';
import './avatar_icon.js';
// <if expr="chromeos">
import '../controls/password_prompt_dialog.m.js';
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>

/**
 * Checks if an HTML element is an editable. An editable is either a text
 * input or a text area.
 * @param {!Element} element
 * @return {boolean}
 */
function isEditable(element) {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(element.type)));
}

Polymer({
  is: 'passwords-section',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    MergeExceptionsStoreCopiesBehavior,
    MergePasswordsStoreCopiesBehavior,
    PasswordCheckBehavior,
    IronA11yKeysBehavior,
    GlobalScrollTargetBehavior,
    PrefsBehavior,
  ],

  properties: {

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },

    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @override */
    subpageRoute: {
      type: Object,
      value: routes.PASSWORDS,
    },

    /** The target of the key bindings defined below. */
    keyEventTarget: {
      type: Object,
      value: () => document,
    },

    /** Filter on the saved passwords and exceptions. */
    filter: {
      type: String,
      value: '',
    },

    // <if expr="not chromeos">
    /** @private */
    storedAccounts_: Array,
    // </if>

    /** @private */
    signedIn_: {
      type: Boolean,
      value: true,
      computed: 'computeSignedIn_(syncStatus_, storedAccounts_)',
    },

    /** @private */
    eligibleForAccountStorage_: {
      type: Boolean,
      value: false,
      computed: 'computeEligibleForAccountStorage_(' +
          'syncStatus_, signedIn_, syncPrefs_)',
    },

    /** @private */
    hasNeverCheckedPasswords_: {
      type: Boolean,
      computed: 'computeHasNeverCheckedPasswords_(status)',
    },

    /** @private */
    hasSavedPasswords_: {
      type: Boolean,
      computed:
          'computeHasSavedPasswords_(savedPasswords, savedPasswords.splices)',
    },

    /**
     * Used to decide the text on the button leading to 'device passwords' page.
     * @private
     */
    numberOfDevicePasswords_: {
      type: Number,
      computed: 'computeNumberOfDevicePasswords_(savedPasswords, ' +
          'savedPasswords.splices)',
    },

    /** @private */
    hasPasswordExceptions_: {
      type: Boolean,
      computed: 'computeHasPasswordExceptions_(passwordExceptions)',
    },

    /** @private */
    shouldShowBanner_: {
      type: Boolean,
      value: true,
      computed: 'computeShouldShowBanner_(hasLeakedCredentials_,' +
          'signedIn_, hasNeverCheckedPasswords_, hasSavedPasswords_)',
    },

    /**
     * Whether the edit dialog and removal notification should show
     * information about which location(s) a password is stored.
     * @private
     */
    shouldShowStorageDetails_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowStorageDetails_(' +
          'eligibleForAccountStorage_, isOptedInForAccountStorage_)',
    },

    /**
     * Whether the entry point leading to the device passwords page should be
     * shown for a user who is already eligible for account storage.
     * @private
     */
    shouldShowDevicePasswordsLink_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowDevicePasswordsLink_(' +
          'isOptedInForAccountStorage_, numberOfDevicePasswords_)',
    },

    /** @private */
    hasLeakedCredentials_: {
      type: Boolean,
      computed: 'computeHasLeakedCredentials_(leakedPasswords)',
    },

    /** @private */
    hidePasswordsLink_: {
      type: Boolean,
      computed: 'computeHidePasswordsLink_(syncPrefs_, syncStatus_, ' +
          'eligibleForAccountStorage_)',
    },

    /** @private */
    showImportPasswords_: {
      type: Boolean,
      value() {
        return loadTimeData.valueExists('showImportPasswords') &&
            loadTimeData.getBoolean('showImportPasswords');
      }
    },

    /** @private */
    accountStorageFeatureEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableAccountStorage');
      }
    },

    /** @private */
    devicePasswordsLinkLabel_: {
      type: String,
      value: '',
      computed: 'computeDevicePasswordsLinkLabel_(numberOfDevicePasswords_)',
    },

    /** @private */
    profileEmail_: {
      type: String,
      value: '',
      computed: 'getFirstStoredAccountEmail_(storedAccounts_)',
    },

    /**
     * The currently selected profile icon as CSS image set.
     * @private
     */
    profileIcon_: String,

    /** @private */
    isOptedInForAccountStorage_: Boolean,

    /** @private {SyncPrefs} */
    syncPrefs_: Object,

    /** @private {SyncStatus} */
    syncStatus_: Object,

    /** @private {!MultiStorePasswordUiEntry} */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    // <if expr="chromeos">
    /** @private */
    showPasswordPromptDialog_: Boolean,

    /** @private {BlockingRequestManager} */
    tokenRequestManager_: Object,
    // </if>
  },

  keyBindings: {
    // <if expr="is_macosx">
    'meta+z': 'onUndoKeyBinding_',
    // </if>
    // <if expr="not is_macosx">
    'ctrl+z': 'onUndoKeyBinding_',
    // </if>
  },

  /**
   * A stack of the elements that triggered dialog to open and should therefore
   * receive focus when that dialog is closed. The bottom of the stack is the
   * element that triggered the earliest open dialog and top of the stack is the
   * element that triggered the most recent (i.e. active) dialog. If no dialog
   * is open, the stack is empty.
   * @private {!Array<Element>}
   */
  activeDialogAnchorStack_: [],

  /** @private {?PasswordManagerProxy} */
  passwordManager_: null,

  /** @private {?function(boolean):void} */
  setIsOptedInForAccountStorageListener_: null,

  /** @private {?function(!Array<PasswordManagerProxy.ExceptionEntry>):void} */
  setPasswordExceptionsListener_: null,

  /** @override */
  attached() {
    // Create listener functions.
    const setIsOptedInForAccountStorageListener = optedIn => {
      this.isOptedInForAccountStorage_ = optedIn;
    };

    this.setIsOptedInForAccountStorageListener_ =
        setIsOptedInForAccountStorageListener;

    // Set the manager. These can be overridden by tests.
    this.passwordManager_ = PasswordManagerImpl.getInstance();

    // <if expr="chromeos">
    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager_| will immediately
    // resolve requests.
    if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
      this.tokenRequestManager_ = new BlockingRequestManager();
    } else {
      this.tokenRequestManager_ =
          new BlockingRequestManager(this.openPasswordPromptDialog_.bind(this));
    }
    // </if>

    // Request initial data.
    this.passwordManager_.isOptedInForAccountStorage().then(
        setIsOptedInForAccountStorageListener);

    // Listen for changes.
    this.passwordManager_.addAccountStorageOptInStateListener(
        setIsOptedInForAccountStorageListener);

    const syncBrowserProxy = SyncBrowserProxyImpl.getInstance();

    const syncStatusChanged = syncStatus => this.syncStatus_ = syncStatus;
    syncBrowserProxy.getSyncStatus().then(syncStatusChanged);
    this.addWebUIListener('sync-status-changed', syncStatusChanged);

    const syncPrefsChanged = syncPrefs => this.syncPrefs_ = syncPrefs;
    this.addWebUIListener('sync-prefs-changed', syncPrefsChanged);
    syncBrowserProxy.sendSyncPrefsChanged();

    // For non-ChromeOS, also check whether accounts are available.
    // <if expr="not chromeos">
    const storedAccountsChanged = accounts => this.storedAccounts_ = accounts;
    syncBrowserProxy.getStoredAccounts().then(storedAccountsChanged);
    this.addWebUIListener('stored-accounts-updated', storedAccountsChanged);
    // </if>

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });
  },

  /** @override */
  detached() {
    this.passwordManager_.removeAccountStorageOptInStateListener(
        assert(this.setIsOptedInForAccountStorageListener_));
  },

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn ?
        !this.syncStatus_.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeEligibleForAccountStorage_() {
    // The user must have signed in but should have sync disabled
    // (|!this.syncStatus_.signedin|). They should not be using a custom
    // passphrase to encrypt their sync data, since there's no way for account
    // storage users to input their passphrase and decrypt the passwords.
    return this.accountStorageFeatureEnabled_ &&
        (!!this.syncStatus_ && !this.syncStatus_.signedIn) && this.signedIn_ &&
        (!this.syncPrefs_ || !this.syncPrefs_.encryptAllData);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasSavedPasswords_() {
    return this.savedPasswords.length > 0;
  },

  /**
   * @return {number}
   * @private
   */
  computeNumberOfDevicePasswords_() {
    return this.savedPasswords.filter(p => p.isPresentOnDevice()).length;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeHasPasswordExceptions_() {
    return this.passwordExceptions.length > 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowBanner_() {
    return this.signedIn_ && this.hasSavedPasswords_ &&
        this.hasNeverCheckedPasswords_ && !this.hasLeakedCredentials_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowStorageDetails_() {
    return this.eligibleForAccountStorage_ && this.isOptedInForAccountStorage_;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowDevicePasswordsLink_() {
    return this.isOptedInForAccountStorage_ &&
        (this.numberOfDevicePasswords_ > 0);
  },

  /**
   * hide the link to the user's Google Account if:
   *  a) the link is embedded in the account storage message OR
   *  b) the user is signed out (or signed-in but has encrypted passwords)
   * @return {boolean}
   * @private
   */
  computeHidePasswordsLink_() {
    return this.eligibleForAccountStorage_ ||
        (!!this.syncStatus_ && !!this.syncStatus_.signedIn &&
         !!this.syncPrefs_ && !!this.syncPrefs_.encryptAllData);
  },

  /**
   * @private
   * @return {boolean}
   */
  computeHasLeakedCredentials_() {
    return this.leakedPasswords.length > 0;
  },

  /**
   * @private
   * @return {boolean}
   */
  computeHasNeverCheckedPasswords_() {
    return !this.status.elapsedTimeSinceLastCheck;
  },

  /**
   * @private
   * @return {string}
   */
  computeDevicePasswordsLinkLabel_() {
    return this.numberOfDevicePasswords_ === 1 ?
        this.i18n('devicePasswordsLinkLabelSingular') :
        this.i18n(
            'devicePasswordsLinkLabelPlural', this.numberOfDevicePasswords_);
  },

  /**
   * Shows the check passwords sub page.
   * @private
   */
  onCheckPasswordsClick_() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    this.passwordManager_.recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS);
  },

  /**
   * Shows the 'device passwords' page.
   * @private
   */
  onDevicePasswordsLinkClicked_() {
    Router.getInstance().navigateTo(routes.DEVICE_PASSWORDS);
  },

  // <if expr="chromeos">
  /**
   * When this event fired, it means that the password-prompt-dialog succeeded
   * in creating a fresh token in the quickUnlockPrivate API. Because new tokens
   * can only ever be created immediately following a GAIA password check, the
   * passwordsPrivate API can now safely grant requests for secure data (i.e.
   * saved passwords) for a limited time. This observer resolves the request,
   * triggering a callback that requires a fresh auth token to succeed and that
   * was provided to the BlockingRequestManager by another DOM element seeking
   * secure data.
   *
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e - Contains
   *     newly created auth token. Note that its precise value is not relevant
   *     here, only the facts that it's created.
   * @private
   */
  onTokenObtained_(e) {
    assert(e.detail);
    this.tokenRequestManager_.resolve();
  },

  /** @private */
  onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /** @private */
  openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement());
    this.showPasswordPromptDialog_ = true;
  },
  // </if>

  /**
   * @param {string} filter
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_(filter) {
    if (!filter) {
      return this.savedPasswords.slice();
    }

    return this.savedPasswords.filter(
        p => [p.urls.shown, p.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  },

  /**
   * @param {string} filter
   * @return {function(!chrome.passwordsPrivate.ExceptionEntry): boolean}
   * @private
   */
  passwordExceptionFilter_(filter) {
    return exception => exception.urls.shown.toLowerCase().includes(
               filter.toLowerCase());
  },

  /**
   * Handle the shortcut to undo a removal of passwords/exceptions. This must
   * be handled here and not at the PasswordsListHandler level because that
   * component does not know about exception deletions.
   * @param {!Event} event
   * @private
   */
  onUndoKeyBinding_(event) {
    const activeElement = getDeepActiveElement();
    // If the focused element is editable (e.g. search box) the undo event
    // should be handled there and not here.
    if (!activeElement || !isEditable(activeElement)) {
      this.passwordManager_.undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  },

  /**
   * Fires an event that should delete the password exception.
   * @param {!{model: !{item: !chrome.passwordsPrivate.ExceptionEntry}}} e
   * The polymer event.
   * @private
   */
  onRemoveExceptionButtonTap_(e) {
    const exception = e.model.item;
    /** @type {!Array<number>} */
    const allExceptionIds = [];
    if (exception.isPresentInAccount()) {
      allExceptionIds.push(exception.accountId);
    }
    if (exception.isPresentOnDevice()) {
      allExceptionIds.push(exception.deviceId);
    }
    this.passwordManager_.removeExceptions(allExceptionIds);
  },

  /**
   * Opens the export/import action menu.
   * @private
   */
  onImportExportMenuTap_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.exportImportMenu);
    const target =
        /** @type {!HTMLElement} */ (this.$$('#exportImportMenuButton'));

    menu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
  },

  /**
   * Fires an event that should trigger the password import process.
   * @private
   */
  onImportTap_() {
    this.passwordManager_.importPasswords();
    this.$.exportImportMenu.close();
  },

  /**
   * Opens the export passwords dialog.
   * @private
   */
  onExportTap_() {
    this.showPasswordsExportDialog_ = true;
    this.$.exportImportMenu.close();
  },

  /** @private */
  onPasswordsExportDialogClosed_() {
    this.showPasswordsExportDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  },

  /** @private */
  onOptIn_: function() {
    this.passwordManager_.optInForAccountStorage(true);
  },

  /** @private */
  onOptOut_: function() {
    this.passwordManager_.optInForAccountStorage(false);
  },

  /**
   * @private
   * @return {boolean}
   */
  showImportOrExportPasswords_() {
    return this.hasSavedPasswords_ || this.showImportPasswords_;
  },

  /**
   * Return the first available stored account. This is useful when trying to
   * figure out the account logged into the content area which seems to always
   * be first even if multiple accounts are available.
   * @return {string} The email address of the first stored account or an empty
   *     string.
   * @private
   */
  getFirstStoredAccountEmail_() {
    return !!this.storedAccounts_ && this.storedAccounts_.length > 0 ?
        this.storedAccounts_[0].email :
        '';
  },

  /**
   * @param {!Map<string, string>} newConfig
   * @param {?Map<string, string>} oldConfig
   * @private
   */
  focusConfigChanged_(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should
    // only fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-autofill-page>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    this.focusConfig.set(assert(routes.CHECK_PASSWORDS).path, () => {
      focusWithoutInk(assert(this.$$('#icon')));
    });
  },
});
