// Copyright 2020 The Chromium Authors
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
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import '../settings_shared.css.js';
import './avatar_icon.js';
import './passwords_shared.css.js';
import './password_list_item.js';
import './password_move_multiple_passwords_to_account_dialog.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {StoredAccount, SyncBrowserProxyImpl, SyncStatus} from '/shared/settings/people_page/sync_browser_proxy.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement, isUndoKeyboardEvent} from 'chrome://resources/js/util_ts.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, Router} from '../router.js';

import {MergePasswordsStoreCopiesMixin} from './merge_passwords_store_copies_mixin.js';
import {AccountStorageOptInStateChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {getTemplate} from './passwords_device_section.html.js';
import {PasswordsListHandlerElement} from './passwords_list_handler.js';

/**
 * Checks if an HTML element is an editable. An editable element is either a
 * text input or a text area.
 */
function isEditable(element: Element): boolean {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(
            (element as HTMLInputElement).type)));
}

export interface PasswordsDeviceSectionElement {
  $: {
    deviceAndAccountPasswordList: IronListElement,
    deviceOnlyPasswordList: IronListElement,
    moveMultiplePasswordsBanner: HTMLElement,
    passwordsListHandler: PasswordsListHandlerElement,
    toast: CrToastElement,
  };
}

const PasswordsDeviceSectionElementBase =
    MergePasswordsStoreCopiesMixin(GlobalScrollTargetMixin(
        WebUiListenerMixin(RouteObserverMixin(PolymerElement))));

export class PasswordsDeviceSectionElement extends
    PasswordsDeviceSectionElementBase {
  static get is() {
    return 'passwords-device-section';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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
       * Passwords displayed in the device-only subsection.
       */
      deviceOnlyPasswords_: {
        type: Array,
        value: () => [],
        computed:
            'computeDeviceOnlyPasswords_(savedPasswords, savedPasswords.splices)',
      },

      /**
       * Passwords displayed in the 'device and account' subsection.
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
       */
      allDevicePasswords_: {
        type: Array,
        value: () => [],
        computed: 'computeAllDevicePasswords_(savedPasswords.splices)',
        observer: 'onAllDevicePasswordsChanged_',
      },

      /**
       * Whether the entry point leading to the dialog to move multiple
       * passwords to the Google Account should be shown. It's shown only where
       * there is at least one password store on device.
       */
      shouldShowMoveMultiplePasswordsBanner_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowMoveMultiplePasswordsBanner_(' +
            'savedPasswords, savedPasswords.splices)',
      },


      lastFocused_: Object,
      listBlurred_: Boolean,
      accountEmail_: String,

      isUserAllowedToAccessPage_: {
        type: Boolean,
        computed:
            'computeIsUserAllowedToAccessPage_(signedIn_, syncDisabled_,' +
            'optedInForAccountStorage_)',
      },

      /**
       * Whether the user is signed in, one of the requirements to view this
       * page.
       */
      signedIn_: {
        type: Boolean,
        value: null,
      },

      /**
       * Whether Sync is disabled, one of the requirements to view this page.
       */
      syncDisabled_: {
        type: Boolean,
        value: null,
      },

      /**
       * Whether the user has opted in to the account-scoped password storage,
       * one of the requirements to view this page.
       */
      optedInForAccountStorage_: {
        type: Boolean,
        value: null,
      },

      showMoveMultiplePasswordsDialog_: Boolean,

      currentRoute_: {
        type: Object,
        value: null,
      },

      devicePasswordsLabel_: {
        type: String,
        value: '',
      },

      isPasswordViewPageEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('enablePasswordViewPage');
        },
        reflectToAttribute: true,
      },

      focusConfig: Object,
    };
  }

  static get observers() {
    return [
      'maybeRedirectToPasswordsPage_(isUserAllowedToAccessPage_, currentRoute_)',
    ];
  }

  subpageRoute: Route;
  filter: string;
  private deviceOnlyPasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private deviceAndAccountPasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private allDevicePasswords_: chrome.passwordsPrivate.PasswordUiEntry[];
  private shouldShowMoveMultiplePasswordsBanner_: boolean;
  private lastFocused_: chrome.passwordsPrivate.PasswordUiEntry;
  private listBlurred_: boolean;
  private accountEmail_: string;
  private isUserAllowedToAccessPage_: boolean;
  private signedIn_: boolean|null;
  private syncDisabled_: boolean|null;
  private optedInForAccountStorage_: boolean|null;
  private showMoveMultiplePasswordsDialog_: boolean;
  private currentRoute_: Route|null;
  private devicePasswordsLabel_: string;
  private isPasswordViewPageEnabled_: boolean;
  focusConfig: Map<string, string|(() => void)>;
  private accountStorageOptInStateListener_:
      AccountStorageOptInStateChangedListener|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.addListenersForAccountStorageRequirements_();
    this.currentRoute_ = Router.getInstance().currentRoute;

    const extractFirstStoredAccountEmail = (accounts: StoredAccount[]) => {
      this.accountEmail_ = accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstStoredAccountEmail);
    this.addWebUiListener(
        'stored-accounts-updated', extractFirstStoredAccountEmail);
  }

  override ready() {
    super.ready();

    document.addEventListener('keydown', (keyboardEvent: KeyboardEvent) => {
      if (isUndoKeyboardEvent(keyboardEvent)) {
        this.onUndoKeyBinding_(keyboardEvent);
      }
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    PasswordManagerImpl.getInstance().removeAccountStorageOptInStateListener(
        this.accountStorageOptInStateListener_!);
    this.accountStorageOptInStateListener_ = null;
  }

  private computeAllDevicePasswords_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    return this.savedPasswords.filter(
        p => p.storedIn !== chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT);
  }

  private computeDeviceOnlyPasswords_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    return this.savedPasswords.filter(
        p => p.storedIn === chrome.passwordsPrivate.PasswordStoreSet.DEVICE);
  }

  private computeDeviceAndAccountPasswords_():
      chrome.passwordsPrivate.PasswordUiEntry[] {
    return this.savedPasswords.filter(
        p => p.storedIn ===
            chrome.passwordsPrivate.PasswordStoreSet.DEVICE_AND_ACCOUNT);
  }

  private computeIsUserAllowedToAccessPage_(): boolean {
    // Only deny access when one of the requirements has already been computed
    // and is not satisfied.
    return (this.signedIn_ === null || !!this.signedIn_) &&
        (this.syncDisabled_ === null || !!this.syncDisabled_) &&
        (this.optedInForAccountStorage_ === null ||
         !!this.optedInForAccountStorage_);
  }

  private computeShouldShowMoveMultiplePasswordsBanner_(): boolean {
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
                         p1.urls.signonRealm === p2.urls.signonRealm)
                 .length === 1));
  }

  private async onAllDevicePasswordsChanged_() {
    this.devicePasswordsLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswordsToAccount', this.allDevicePasswords_.length);
  }

  /**
   * From RouteObserverMixin.
   */
  override currentRouteChanged(route: Route) {
    super.currentRouteChanged(route);
    this.currentRoute_ = route || null;
  }

  private addListenersForAccountStorageRequirements_() {
    const setSyncDisabled = (syncStatus: SyncStatus) => {
      this.syncDisabled_ = !syncStatus.signedIn;
    };
    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(setSyncDisabled);
    this.addWebUiListener('sync-status-changed', setSyncDisabled);

    const setSignedIn = (storedAccounts: StoredAccount[]) => {
      this.signedIn_ = storedAccounts.length > 0;
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(setSignedIn);
    this.addWebUiListener('stored-accounts-updated', setSignedIn);

    const setOptedIn = (optedInForAccountStorage: boolean) => {
      this.optedInForAccountStorage_ = optedInForAccountStorage;
    };
    PasswordManagerImpl.getInstance().isOptedInForAccountStorage().then(
        setOptedIn);
    PasswordManagerImpl.getInstance().addAccountStorageOptInStateListener(
        setOptedIn);
    this.accountStorageOptInStateListener_ = setOptedIn;
  }

  private isNonEmpty_(passwords: chrome.passwordsPrivate.PasswordUiEntry[]):
      boolean {
    return passwords.length > 0;
  }

  private getFilteredPasswords_(
      passwords: chrome.passwordsPrivate.PasswordUiEntry[],
      filter: string): chrome.passwordsPrivate.PasswordUiEntry[] {
    if (!filter) {
      return passwords.slice();
    }

    return passwords.filter(
        p => [p.urls.shown, p.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  }

  /**
   * Handle the undo shortcut.
   */
  // TODO(crbug.com/1102294): Consider grouping the ctrl-z related code into
  // a dedicated behavior.
  private onUndoKeyBinding_(event: Event) {
    const activeElement = getDeepActiveElement();
    if (!activeElement || !isEditable(activeElement)) {
      PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  }

  private onManageAccountPasswordsClicked_() {
    OpenWindowProxyImpl.getInstance().openUrl(
        loadTimeData.getString('googlePasswordManagerUrl'));
  }

  private onMoveMultiplePasswordsClick_() {
    this.showMoveMultiplePasswordsDialog_ = true;
  }

  private onMoveMultiplePasswordsDialogClose_() {
    if (this.shadowRoot!
            .querySelector(
                'password-move-multiple-passwords-to-account-dialog')!
            .wasConfirmed()) {
      this.$.toast.show();
    }
    this.showMoveMultiplePasswordsDialog_ = false;
  }

  private maybeRedirectToPasswordsPage_() {
    // The component can be attached even if the route is no longer
    // DEVICE_PASSWORDS, so check to avoid navigating when the user is viewing
    // other non-related pages.
    if (!this.isUserAllowedToAccessPage_ &&
        this.currentRoute_ === routes.DEVICE_PASSWORDS) {
      Router.getInstance().navigateTo(routes.PASSWORDS);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'passwords-device-section': PasswordsDeviceSectionElement;
  }
}

customElements.define(
    PasswordsDeviceSectionElement.is, PasswordsDeviceSectionElement);
