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

import {afterNextRender, html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import '../controls/extension_controlled_indicator.js';
import '../controls/settings_toggle_button.js';
import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {HatsBrowserProxyImpl, TrustSafetyInteraction} from '../hats_browser_proxy.js';
import {loadTimeData} from '../i18n_setup.js';
import {SyncBrowserProxyImpl, SyncPrefs, SyncStatus} from '../people_page/sync_browser_proxy.js';
import '../prefs/prefs.js';
import {PrefsBehavior} from '../prefs/prefs_behavior.js';
import {routes} from '../route.js';
import {MergeExceptionsStoreCopiesBehavior, MergeExceptionsStoreCopiesBehaviorInterface} from './merge_exceptions_store_copies_behavior.js';
import {MergePasswordsStoreCopiesBehavior, MergePasswordsStoreCopiesBehaviorInterface} from './merge_passwords_store_copies_behavior.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {Router} from '../router.js';
import '../settings_shared_css.js';
import '../site_favicon.js';
import {PasswordCheckMixin, PasswordCheckMixinInterface} from './password_check_mixin.js';
import './password_list_item.js';
import './passwords_list_handler.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import './passwords_export_dialog.js';
import './passwords_shared_css.js';
import './avatar_icon.js';
// <if expr="chromeos">
import '../controls/password_prompt_dialog.js';
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


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {MergePasswordsStoreCopiesBehaviorInterface}
 * @implements {MergeExceptionsStoreCopiesBehaviorInterface}
 * @implements {PasswordCheckMixinInterface}
 * @implements {WebUIListenerBehaviorInterface}
 */
const PasswordsSectionElementBase = mixinBehaviors(
    [
      I18nBehavior,
      WebUIListenerBehavior,
      MergeExceptionsStoreCopiesBehavior,
      MergePasswordsStoreCopiesBehavior,
      PrefsBehavior,
    ],
    PasswordCheckMixin(GlobalScrollTargetMixin(PolymerElement)));

/** @polymer */
class PasswordsSectionElement extends PasswordsSectionElementBase {
  static get is() {
    return 'passwords-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {

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

      /** Filter on the saved passwords and exceptions. */
      filter: {
        type: String,
        value: '',
        observer: 'announceSearchResults_',
      },

      /** @private */
      shownPasswordsCount_: {
        type: Number,
        value: 0,
      },

      /** @private */
      shownExceptionsCount_: {
        type: Number,
        value: 0,
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
       * Used to decide the text on the button leading to 'device passwords'
       * page.
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

      /**
       * Whether the entry point leading to enroll in trusted vault encryption
       * should be shown.
       * @private
       */
      shouldOfferTrustedVaultOptIn_: {
        type: Boolean,
        value: false,
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

      // <if expr="chromeos">
      /** @private */
      showPasswordPromptDialog_: Boolean,

      /** @private {BlockingRequestManager} */
      tokenRequestManager_: Object,
      // </if>

      /** @private */
      showPasswordsExportDialog_: Boolean,
    };
  }

  constructor() {
    super();

    /**
     * A stack of the elements that triggered dialog to open and should
     * therefore receive focus when that dialog is closed. The bottom of the
     * stack is the element that triggered the earliest open dialog and top of
     * the stack is the element that triggered the most recent (i.e. active)
     * dialog. If no dialog is open, the stack is empty.
     * @private {!Array<Element>}
     */
    this.activeDialogAnchorStack_ = [];

    /** @private {!PasswordManagerProxy} */
    this.passwordManager_ = PasswordManagerImpl.getInstance();


    /** @private {?function(boolean):void} */
    this.setIsOptedInForAccountStorageListener_ = null;

    /**
     * @private {?function(!Array<PasswordManagerProxy.ExceptionEntry>):void}
     */
    this.setPasswordExceptionsListener_ = null;
  }

  /** @override */
  ready() {
    super.ready();

    document.addEventListener('keydown', e => {
      const keyboardEvent = /** @type {!KeyboardEvent} */ (e);
      // <if expr="is_macosx">
      if (keyboardEvent.metaKey && keyboardEvent.key === 'z') {
        this.onUndoKeyBinding_(keyboardEvent);
      }
      // </if>
      // <if expr="not is_macosx">
      if (keyboardEvent.ctrlKey && keyboardEvent.key === 'z') {
        this.onUndoKeyBinding_(keyboardEvent);
      }
      // </if>
    });
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    // Create listener functions.
    const setIsOptedInForAccountStorageListener = optedIn => {
      this.isOptedInForAccountStorage_ = optedIn;
    };

    this.setIsOptedInForAccountStorageListener_ =
        setIsOptedInForAccountStorageListener;

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

    syncBrowserProxy.sendOfferTrustedVaultOptInChanged();
    this.addWebUIListener(
        'offer-trusted-vault-opt-in-changed', (offerOptIn) => {
          this.shouldOfferTrustedVaultOptIn_ = offerOptIn;
        });

    afterNextRender(this, function() {
      IronA11yAnnouncer.requestAvailability();
    });

    HatsBrowserProxyImpl.getInstance().trustSafetyInteractionOccurred(
        TrustSafetyInteraction.OPENED_PASSWORD_MANAGER);
  }

  /** @override */
  disconnectedCallback() {
    super.disconnectedCallback();

    this.passwordManager_.removeAccountStorageOptInStateListener(
        assert(this.setIsOptedInForAccountStorageListener_));
  }

  /**
   * @return {boolean}
   * @private
   */
  computeSignedIn_() {
    return !!this.syncStatus_ && !!this.syncStatus_.signedIn ?
        !this.syncStatus_.hasError :
        (!!this.storedAccounts_ && this.storedAccounts_.length > 0);
  }

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
  }

  /**
   * @return {boolean}
   * @private
   */
  computeHasSavedPasswords_() {
    return this.savedPasswords.length > 0;
  }

  /**
   * @return {number}
   * @private
   */
  computeNumberOfDevicePasswords_() {
    return this.savedPasswords.filter(p => p.isPresentOnDevice()).length;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeHasPasswordExceptions_() {
    return this.passwordExceptions.length > 0;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowBanner_() {
    return this.signedIn_ && this.hasSavedPasswords_ &&
        this.hasNeverCheckedPasswords_ && !this.hasLeakedCredentials_;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowStorageDetails_() {
    return this.eligibleForAccountStorage_ && this.isOptedInForAccountStorage_;
  }

  /**
   * @return {boolean}
   * @private
   */
  computeShouldShowDevicePasswordsLink_() {
    return this.isOptedInForAccountStorage_ &&
        (this.numberOfDevicePasswords_ > 0);
  }

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
  }

  /**
   * @private
   * @return {boolean}
   */
  computeHasLeakedCredentials_() {
    return this.leakedPasswords.length > 0;
  }

  /**
   * @private
   * @return {boolean}
   */
  computeHasNeverCheckedPasswords_() {
    return !this.status.elapsedTimeSinceLastCheck;
  }

  /**
   * Shows the check passwords sub page.
   * @private
   */
  onCheckPasswordsClick_() {
    Router.getInstance().navigateTo(
        routes.CHECK_PASSWORDS, new URLSearchParams('start=true'));
    this.passwordManager_.recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.PASSWORD_SETTINGS);
  }

  /**
   * Shows the page to opt in to trusted vault encryption.
   * @private
   */
  onTrustedVaultOptInClick_() {
    OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('trustedVaultOptInUrl'));
  }

  /**
   * Shows the 'device passwords' page.
   * @private
   */
  onDevicePasswordsLinkClicked_() {
    Router.getInstance().navigateTo(routes.DEVICE_PASSWORDS);
  }

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
  }

  /** @private */
  onPasswordPromptClosed_() {
    this.showPasswordPromptDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  }

  /** @private */
  openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement());
    this.showPasswordPromptDialog_ = true;
  }
  // </if>

  /**
   * @return {function(!MultiStorePasswordUiEntry): boolean}
   * @private
   */
  passwordFilter_() {
    return password => [password.urls.shown, password.username].some(
               term => term.toLowerCase().includes(
                   this.filter.trim().toLowerCase()));
  }

  /**
   * @return {function(!chrome.passwordsPrivate.ExceptionEntry): boolean}
   * @private
   */
  passwordExceptionFilter_() {
    return exception => exception.urls.shown.toLowerCase().includes(
               this.filter.trim().toLowerCase());
  }

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
  }

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
  }

  /**
   * Opens the export/import action menu.
   * @private
   */
  onImportExportMenuTap_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$.exportImportMenu);
    const target =
        /** @type {!HTMLElement} */ (
            this.shadowRoot.querySelector('#exportImportMenuButton'));

    menu.showAt(target);
    this.activeDialogAnchorStack_.push(target);
  }

  /**
   * Fires an event that should trigger the password import process.
   * @private
   */
  onImportTap_() {
    this.passwordManager_.importPasswords();
    this.$.exportImportMenu.close();
  }

  /**
   * Opens the export passwords dialog.
   * @private
   */
  onExportTap_() {
    this.showPasswordsExportDialog_ = true;
    this.$.exportImportMenu.close();
  }

  /** @private */
  onPasswordsExportDialogClosed_() {
    this.showPasswordsExportDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchorStack_.pop()));
  }

  /** @private */
  onOptIn_() {
    this.passwordManager_.optInForAccountStorage(true);
  }

  /** @private */
  onOptOut_() {
    this.passwordManager_.optInForAccountStorage(false);
  }

  /**
   * @private
   * @return {boolean}
   */
  showImportOrExportPasswords_() {
    return this.hasSavedPasswords_ || this.showImportPasswords_;
  }

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
  }

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
      focusWithoutInk(assert(this.shadowRoot.querySelector('#icon')));
    });
  }

  /** @private */
  announceSearchResults_() {
    if (!this.filter.trim()) {
      return;
    }
    setTimeout(() => {  // Async to allow list to update.
      IronA11yAnnouncer.requestAvailability();
      const total = this.shownPasswordsCount_ + this.shownExceptionsCount_;
      let text;
      switch (total) {
        case 0:
          text = this.i18n('noSearchResults');
          break;
        case 1:
          text = this.i18n('searchResultsSingular', this.filter);
          break;
        default:
          text =
              this.i18n('searchResultsPlural', total.toString(), this.filter);
      }
      this.dispatchEvent(new CustomEvent('iron-announce', {
        bubbles: true,
        composed: true,
        detail: {
          text: text,
        }
      }));
    }, 0);
  }
}

customElements.define(PasswordsSectionElement.is, PasswordsSectionElement);
