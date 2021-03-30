// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-device-section' represents the page containing the
 * list of passwords which have at least one copy on the user device. The page
 * is only displayed for users of the account-scoped passwords storage. If other
 * users try to access it, they will be redirected to PasswordsSection.
 *
 * This page is *not* displayed on ChromeOS.
 */

import './passwords_list_handler.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../settings_shared_css.js';
import './avatar_icon.js';
import './passwords_shared_css.js';
import './password_list_item.js';
import './password_move_multiple_passwords_to_account_dialog.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yKeysBehavior} from 'chrome://resources/polymer/v3_0/iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetBehavior} from '../global_scroll_target_behavior.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {StoredAccount, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.js';

import {MergePasswordsStoreCopiesBehavior} from './merge_passwords_store_copies_behavior.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordManagerImpl} from './password_manager_proxy.js';

/**
 * Checks if an HTML element is an editable. An editable element is either a
 * text input or a text area.
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
  is: 'passwords-device-section',

  _template: html`{__html_template__}`,

  behaviors: [
    MergePasswordsStoreCopiesBehavior,
    I18nBehavior,
    IronA11yKeysBehavior,
    GlobalScrollTargetBehavior,
    WebUIListenerBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** @override */
    subpageRoute: {
      type: Object,
      value: routes.DEVICE_PASSWORDS,
    },

    /** Search filter on the saved passwords. */
    filter: {
      type: String,
      value: '',
    },

    /**
     * The target of the key bindings defined below.
     * @type {EventTarget}
     */
    keyEventTarget: {
      type: Object,
      value: () => document,
    },

    /**
     * Passwords displayed in the device-only subsection.
     * @private {!Array<!MultiStorePasswordUiEntry>}
     */
    deviceOnlyPasswords_: {
      type: Array,
      value: () => [],
      computed:
          'computeDeviceOnlyPasswords_(savedPasswords, savedPasswords.splices)',
    },

    /**
     * Passwords displayed in the 'device and account' subsection.
     * @private {!Array<!MultiStorePasswordUiEntry>}
     */
    deviceAndAccountPasswords_: {
      type: Array,
      value: () => [],
      computed: 'computeDeviceAndAccountPasswords_(savedPasswords, ' +
          'savedPasswords.splices)',
    },

    /**
     * Passwords displayed in both the device-only and 'device and account'
     * subsections.
     * @private {!Array<!MultiStorePasswordUiEntry>}
     */
    allDevicePasswords_: {
      type: Array,
      value: () => [],
      computed: 'computeAllDevicePasswords_(savedPasswords.splices)',
      observer: 'onAllDevicePasswordsChanged_',
    },

    /**
     * Whether the entry point leading to the dialog to move multiple passwords
     * to the Google Account should be shown. It's shown only where there is at
     * least one password store on device.
     * @private
     */
    shouldShowMoveMultiplePasswordsBanner_: {
      type: Boolean,
      value: false,
      computed: 'computeShouldShowMoveMultiplePasswordsBanner_(' +
          'savedPasswords, savedPasswords.splices)',
    },


    /** @private {!MultiStorePasswordUiEntry} */
    lastFocused_: Object,

    /** @private */
    listBlurred_: Boolean,

    /** @private */
    accountEmail_: String,

    /** @private */
    isUserAllowedToAccessPage_: {
      type: Boolean,
      computed: 'computeIsUserAllowedToAccessPage_(signedIn_, syncDisabled_,' +
          'optedInForAccountStorage_)',
    },

    /**
     * Whether the user is signed in, one of the requirements to view this page.
     * @private {boolean?}
     */
    signedIn_: {
      type: Boolean,
      value: null,
    },

    /**
     * Whether Sync is disabled, one of the requirements to view this page.
     * @private {boolean?}
     */
    syncDisabled_: {
      type: Boolean,
      value: null,
    },

    /**
     * Whether the user has opted in to the account-scoped password storage, one
     * of the requirements to view this page.
     * @private {boolean?}
     */
    optedInForAccountStorage_: {
      type: Boolean,
      value: null,
    },

    /** @private */
    showMoveMultiplePasswordsDialog_: Boolean,

    /** @private {Route?} */
    currentRoute_: {
      type: Object,
      value: null,
    },

    /** @private */
    devicePasswordsLabel_: {
      type: String,
      value: '',
    },

  },

  keyBindings: {
    // <if expr="is_macosx">
    'meta+z': 'onUndoKeyBinding_',
    // </if>
    // <if expr="not is_macosx">
    'ctrl+z': 'onUndoKeyBinding_',
    // </if>
  },

  /** @private {!function(boolean): void} */
  accountStorageOptInStateListener_: Function,

  observers:
      ['maybeRedirectToPasswordsPage_(isUserAllowedToAccessPage_, ' +
       'currentRoute_)'],

  /** @override */
  attached() {
    this.addListenersForAccountStorageRequirements_();
    this.currentRoute_ = Router.getInstance().currentRoute;

    /** @type {!function(!Array<!StoredAccount>):void} */
    const extractFirstStoredAccountEmail = accounts => {
      this.accountEmail_ = accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstStoredAccountEmail);
    this.addWebUIListener(
        'stored-accounts-updated', extractFirstStoredAccountEmail);
  },

  /** @override */
  detached() {
    PasswordManagerImpl.getInstance().removeAccountStorageOptInStateListener(
        this.accountStorageOptInStateListener_);
  },

  /**
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  computeAllDevicePasswords_() {
    return this.savedPasswords.filter(p => p.isPresentOnDevice());
  },

  /**
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  computeDeviceOnlyPasswords_() {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && !p.isPresentInAccount());
  },

  /**
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  computeDeviceAndAccountPasswords_() {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && p.isPresentInAccount());
  },

  /**
   * @private
   * @return {boolean}
   */
  computeIsUserAllowedToAccessPage_() {
    // Only deny access when one of the requirements has already been computed
    // and is not satisfied.
    return (this.signedIn_ === null || !!this.signedIn_) &&
        (this.syncDisabled_ === null || !!this.syncDisabled_) &&
        (this.optedInForAccountStorage_ === null ||
         !!this.optedInForAccountStorage_);
  },

  /**
   * @private
   * @return {boolean}
   */
  computeShouldShowMoveMultiplePasswordsBanner_() {
    if (!loadTimeData.getBoolean('enableMovingMultiplePasswordsToAccount')) {
      return false;
    }

    if (this.allDevicePasswords_.length === 0) {
      return false;
    }

    // Check if all username, and urls are unique. The existence of two entries
    // with the same url and username indicate that they must have conflicting
    // passwords, otherwise, they would have been deduped in
    // MergePasswordsStoreCopiesBehavior. This however may mistakenly exclude
    // users who have conflicting duplicates within the same store, which is an
    // acceptable compromise.
    return this.savedPasswords.every(
        p1 =>
            (this.savedPasswords
                 .filter(
                     p2 => p1.username === p2.username &&
                         p1.urls.origin === p2.urls.origin)
                 .length === 1));
  },

  /**
   * @private
   */
  async onAllDevicePasswordsChanged_() {
    this.devicePasswordsLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswordsToAccount', this.allDevicePasswords_.length);
  },

  /**
   * From RouteObserverBehavior.
   * @param {!Route|undefined} route
   * @protected
   */
  currentRouteChanged(route) {
    this.currentRoute_ = route || null;
  },

  /** @private */
  addListenersForAccountStorageRequirements_() {
    const setSyncDisabled = syncStatus => {
      this.syncDisabled_ = !syncStatus.signedIn;
    };
    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(setSyncDisabled);
    this.addWebUIListener('sync-status-changed', setSyncDisabled);

    const setSignedIn = storedAccounts => {
      this.signedIn_ = storedAccounts.length > 0;
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(setSignedIn);
    this.addWebUIListener('stored-accounts-updated', setSignedIn);

    const setOptedIn = optedInForAccountStorage => {
      this.optedInForAccountStorage_ = optedInForAccountStorage;
    };
    PasswordManagerImpl.getInstance().isOptedInForAccountStorage().then(
        setOptedIn);
    PasswordManagerImpl.getInstance().addAccountStorageOptInStateListener(
        setOptedIn);
    this.accountStorageOptInStateListener_ = setOptedIn;
  },

  /**
   * @param {!Array<!MultiStorePasswordUiEntry>} passwords
   * @return {boolean}
   * @private
   */
  isNonEmpty_(passwords) {
    return passwords.length > 0;
  },

  /**
   * @param {!Array<!MultiStorePasswordUiEntry>} passwords
   * @param {string} filter
   * @return {!Array<!MultiStorePasswordUiEntry>}
   * @private
   */
  getFilteredPasswords_(passwords, filter) {
    if (!filter) {
      return passwords.slice();
    }

    return passwords.filter(
        p => [p.urls.shown, p.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  },

  /**
   * Handle the undo shortcut.
   * @param {!Event} event
   * @private
   */
  // TODO(crbug.com/1102294): Consider grouping the ctrl-z related code into
  // a dedicated behavior.
  onUndoKeyBinding_(event) {
    const activeElement = getDeepActiveElement();
    if (!activeElement || !isEditable(activeElement)) {
      PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  },

  /** @private */
  onManageAccountPasswordsClicked_() {
    OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('googlePasswordManagerUrl'));
  },

  /** @private */
  onMoveMultiplePasswordsTap_() {
    this.showMoveMultiplePasswordsDialog_ = true;
  },

  /** @private */
  onMoveMultiplePasswordsDialogClose_() {
    if ((this.$$('password-move-multiple-passwords-to-account-dialog'))
            .wasConfirmed()) {
      this.$.toast.show();
    }
    this.showMoveMultiplePasswordsDialog_ = false;
  },

  /** @private */
  maybeRedirectToPasswordsPage_() {
    // The component can be attached even if the route is no longer
    // DEVICE_PASSWORDS, so check to avoid navigating when the user is viewing
    // other non-related pages.
    if (!this.isUserAllowedToAccessPage_ &&
        this.currentRoute_ === routes.DEVICE_PASSWORDS) {
      Router.getInstance().navigateTo(routes.PASSWORDS);
    }
  },

});
