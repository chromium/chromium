// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-people-page' is the settings page containing sign-in settings.
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../../controls/settings_toggle_button.js';
import '../os_settings_page/os_settings_animated_pages.js';
import '../os_settings_page/os_settings_subpage.js';
import '../../settings_shared.css.js';
import '../parental_controls_page/parental_controls_page.js';
import './account_manager.js';
import './fingerprint_list.js';
import './lock_screen.js';
import './lock_screen_password_prompt_dialog.js';
import './users_page.js';
import './os_sync_controls.js';
import './os_signout_dialog.js';
import './os_sync_page.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {convertImageSequenceToPng} from 'chrome://resources/ash/common/cr_picture/png.js';
import {focusWithoutInk} from 'chrome://resources/ash/common/focus_without_ink_js.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/ash/common/i18n_behavior.js';
import {WebUIListenerBehavior, WebUIListenerBehaviorInterface} from 'chrome://resources/ash/common/web_ui_listener_behavior.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, flush, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Setting} from '../../mojom-webui/setting.mojom-webui.js';
import {ProfileInfoBrowserProxyImpl} from '../../people_page/profile_info_browser_proxy.js';
import {SyncBrowserProxyImpl} from '../../people_page/sync_browser_proxy.js';
import {DeepLinkingBehavior, DeepLinkingBehaviorInterface} from '../deep_linking_behavior.js';
import {getImage} from '../icon.js';
import {OSPageVisibility} from '../os_page_visibility.js';
import {routes} from '../os_route.js';
import {SettingsParentalControlsPageElement} from '../parental_controls_page/parental_controls_page.js';
import {RouteObserverBehavior, RouteObserverBehaviorInterface} from '../route_observer_behavior.js';
import {Route, Router} from '../router.js';

import {Account, AccountManagerBrowserProxyImpl} from './account_manager_browser_proxy.js';
import {LockStateBehavior, LockStateBehaviorInterface} from './lock_state_behavior.js';
import {getTemplate} from './os_people_page.html.js';

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {DeepLinkingBehaviorInterface}
 * @implements {RouteObserverBehaviorInterface}
 * @implements {I18nBehaviorInterface}
 * @implements {WebUIListenerBehaviorInterface}
 * @implements {LockStateBehaviorInterface}
 */
const OsSettingsPeoplePageElementBase = mixinBehaviors(
    [
      DeepLinkingBehavior,
      RouteObserverBehavior,
      I18nBehavior,
      WebUIListenerBehavior,
      LockStateBehavior,
    ],
    PolymerElement);

/** @polymer */
class OsSettingsPeoplePageElement extends OsSettingsPeoplePageElementBase {
  static get is() {
    return 'os-settings-people-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * The current sync status, supplied by SyncBrowserProxy.
       * @type {?SyncStatus}
       */
      syncStatus: Object,

      /**
       * Dictionary defining page visibility.
       * @type {!OSPageVisibility}
       */
      pageVisibility: Object,

      /**
       * Authentication token.
       * @private {!chrome.quickUnlockPrivate.TokenInfo|undefined}
       */
      authToken_: {
        type: Object,
        observer: 'onAuthTokenChanged_',
      },

      /**
       * The current profile icon URL. Usually a data:image/png URL.
       * @private
       */
      profileIconUrl_: String,

      /**
       * The current profile name, e.g. "John Cena".
       * @private
       */
      profileName_: String,

      /**
       * The current profile email, e.g. "john.cena@gmail.com".
       * @private
       */
      profileEmail_: String,

      /**
       * The label may contain additional text, for example:
       * "john.cena@gmail, + 2 more accounts".
       * @private
       */
      profileLabel_: String,

      /** @private */
      showSignoutDialog_: Boolean,

      /**
       * True if fingerprint settings should be displayed on this machine.
       * @private
       */
      fingerprintUnlockEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('fingerprintUnlockEnabled');
        },
        readOnly: true,
      },

      /**
       * True if Chrome OS Account Manager is enabled.
       * @private
       */
      isAccountManagerEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isAccountManagerEnabled');
        },
        readOnly: true,
      },

      /** @private */
      showParentalControls_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('showParentalControls') &&
              loadTimeData.getBoolean('showParentalControls');
        },
      },

      /** @private {!Map<string, string>} */
      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.SYNC) {
            map.set(routes.SYNC.path, '#sync-setup');
          }
          if (routes.LOCK_SCREEN) {
            map.set(routes.LOCK_SCREEN.path, '#lock-screen-subpage-trigger');
          }
          if (routes.ACCOUNTS) {
            map.set(
                routes.ACCOUNTS.path, '#manage-other-people-subpage-trigger');
          }
          if (routes.ACCOUNT_MANAGER) {
            map.set(
                routes.ACCOUNT_MANAGER.path,
                '#account-manager-subpage-trigger');
          }
          return map;
        },
      },

      /** @private {boolean} */
      showPasswordPromptDialog_: {
        type: Boolean,
        value: false,
      },

      /**
       * setModes_ is a partially applied function that stores the current auth
       * token. It's defined only when the user has entered a valid password.
       * @type {Object|undefined}
       * @private
       */
      setModes_: {
        type: Object,
      },

      /**
       * Used by DeepLinkingBehavior to focus this page's deep links.
       * @type {!Set<!Setting>}
       */
      supportedSettingIds: {
        type: Object,
        value: () => new Set([
          Setting.kSetUpParentalControls,

          // Perform Sync page deep links here since it's a shared page.
          Setting.kNonSplitSyncEncryptionOptions,
          Setting.kAutocompleteSearchesAndUrls,
          Setting.kMakeSearchesAndBrowsingBetter,
          Setting.kGoogleDriveSearchSuggestions,
        ]),
      },

    };
  }

  constructor() {
    super();

    /** @private {!SyncBrowserProxy} */
    this.syncBrowserProxy_ = SyncBrowserProxyImpl.getInstance();

    /**
     * The timeout ID to pass to clearTimeout() to cancel auth token
     * invalidation.
     * @private {number|undefined}
     */
    this.clearAccountPasswordTimeoutId_ = undefined;
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();

    if (this.isAccountManagerEnabled_) {
      // If we have the Google Account manager, use GAIA name and icon.
      this.addWebUIListener(
          'accounts-changed', this.updateAccounts_.bind(this));
      this.updateAccounts_();
    } else {
      // Otherwise use the Profile name and icon.
      ProfileInfoBrowserProxyImpl.getInstance().getProfileInfo().then(
          this.handleProfileInfo_.bind(this));
      this.addWebUIListener(
          'profile-info-changed', this.handleProfileInfo_.bind(this));
    }

    this.syncBrowserProxy_.getSyncStatus().then(
        this.handleSyncStatus_.bind(this));
    this.addWebUIListener(
        'sync-status-changed', this.handleSyncStatus_.bind(this));
  }

  /** @private */
  onPasswordRequested_() {
    this.showPasswordPromptDialog_ = true;
  }

  // Invalidate the token to trigger a password re-prompt. Used for PIN auto
  // submit when too many attempts were made when using PrefStore based PIN.
  onInvalidateTokenRequested_() {
    this.authToken_ = undefined;
  }

  /** @private */
  onPasswordPromptDialogClose_() {
    this.showPasswordPromptDialog_ = false;
    if (!this.setModes_) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  /**
   * Helper function for manually showing deep links on this page.
   * @param {!Setting} settingId
   * @param {!function():?Element} getElementCallback
   * @private
   */
  afterRenderShowDeepLink_(settingId, getElementCallback) {
    // Wait for element to load.
    afterNextRender(this, () => {
      const deepLinkElement = getElementCallback();
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
  }

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    switch (settingId) {
      // Manually show the deep links for settings nested within elements.
      case Setting.kSetUpParentalControls:
        this.afterRenderShowDeepLink_(
            settingId,
            /**
             * TODO(crbug/1315757) settings-parental-controls-page is in TS so
             * suppress the closure compilation error for getSetupButton()
             * until this page is converted to TS.
             * @suppress {missingProperties}
             */
            () => {
              const parentalPage = this.shadowRoot.querySelector(
                  'settings-parental-controls-page');
              return parentalPage && parentalPage.getSetupButton();
            });
        // Stop deep link attempt since we completed it manually.
        return false;

      // Handle the settings within the old sync page since its a shared
      // component.
      case Setting.kNonSplitSyncEncryptionOptions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?OsSettingsSyncPageElement} */ (
              this.shadowRoot.querySelector('os-settings-sync-page'));
          // Expand the encryption collapse.
          syncPage.forceEncryptionExpanded = true;
          flush();
          return syncPage && syncPage.getEncryptionOptions() &&
              syncPage.getEncryptionOptions().getEncryptionsRadioButtons();
        });
        return false;

      case Setting.kAutocompleteSearchesAndUrls:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?OsSettingsSyncPageElement} */ (
              this.shadowRoot.querySelector('os-settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getSearchSuggestToggle();
        });
        return false;

      case Setting.kMakeSearchesAndBrowsingBetter:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?OsSettingsSyncPageElement} */ (
              this.shadowRoot.querySelector('os-settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getUrlCollectionToggle();
        });
        return false;

      case Setting.kGoogleDriveSearchSuggestions:
        this.afterRenderShowDeepLink_(settingId, () => {
          const syncPage = /** @type {?OsSettingsSyncPageElement} */ (
              this.shadowRoot.querySelector('os-settings-sync-page'));
          return syncPage && syncPage.getPersonalizationOptions() &&
              syncPage.getPersonalizationOptions().getDriveSuggestToggle();
        });
        return false;

      default:
        // Continue with deep linking attempt.
        return true;
    }
  }

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @param {!Route=} oldRoute
   * @protected
   */
  currentRouteChanged(route, oldRoute) {
    if (Router.getInstance().getCurrentRoute() === routes.OS_SIGN_OUT) {
      // If the sync status has not been fetched yet, optimistically display
      // the sign-out dialog. There is another check when the sync status is
      // fetched. The dialog will be closed when the user is not signed in.
      if (this.syncStatus && !this.syncStatus.signedIn) {
        Router.getInstance().navigateToPreviousRoute();
      } else {
        this.showSignoutDialog_ = true;
      }
    }

    // The old sync page is a shared subpage, so we handle deep links for
    // both this page and the sync page. Not ideal.
    if (route === routes.SYNC || route === routes.OS_PEOPLE) {
      this.attemptDeepLink();
    }
  }

  /**
   * @param {!CustomEvent<!chrome.quickUnlockPrivate.TokenInfo>} e
   * @private
   * */
  onAuthTokenObtained_(e) {
    this.authToken_ = e.detail;
  }

  /**
   * @return {string}
   * @private
   */
  getSyncAndGoogleServicesSubtext_() {
    if (this.syncStatus && this.syncStatus.hasError &&
        this.syncStatus.statusText) {
      return this.syncStatus.statusText;
    }
    return '';
  }

  /**
   * Handler for when the profile's icon and name is updated.
   * @private
   * @param {!ProfileInfo} info
   */
  handleProfileInfo_(info) {
    this.profileName_ = info.name;
    // Extract first frame from image by creating a single frame PNG using
    // url as input if base64 encoded and potentially animated.
    if (info.iconUrl.startsWith('data:image/png;base64')) {
      this.profileIconUrl_ = convertImageSequenceToPng([info.iconUrl]);
      return;
    }
    this.profileIconUrl_ = info.iconUrl;
  }

  /**
   * Handler for when the account list is updated.
   * @private
   */
  async updateAccounts_() {
    const /** @type {!Array<Account>} */ accounts =
        await AccountManagerBrowserProxyImpl.getInstance().getAccounts();
    // The user might not have any GAIA accounts (e.g. guest mode or Active
    // Directory). In these cases the profile row is hidden, so there's nothing
    // to do.
    if (accounts.length === 0) {
      return;
    }
    this.profileName_ = accounts[0].fullName;
    this.profileEmail_ = accounts[0].email;
    this.profileIconUrl_ = accounts[0].pic;

    await this.setProfileLabel(accounts);
  }

  /**
   * @param {!Array<Account>} accounts
   * @private
   */
  async setProfileLabel(accounts) {
    // Template: "$1 Google accounts" with correct plural of "account".
    const labelTemplate = await sendWithPromise(
        'getPluralString', 'profileLabel', accounts.length);

    // Final output: "X Google accounts"
    this.profileLabel_ = loadTimeData.substituteString(
        labelTemplate, accounts[0].email, accounts.length);
  }

  /**
   * Handler for when the sync state is pushed from the browser.
   * @param {?SyncStatus} syncStatus
   * @private
   */
  handleSyncStatus_(syncStatus) {
    this.syncStatus = syncStatus;

    // When ChromeOSAccountManager is disabled, fall back to using the sync
    // username ("alice@gmail.com") as the profile label.
    if (!this.isAccountManagerEnabled_ && syncStatus && syncStatus.signedIn &&
        syncStatus.signedInUsername) {
      this.profileLabel_ = syncStatus.signedInUsername;
    }
  }

  /** @private */
  onDisconnectDialogClosed_(e) {
    this.showSignoutDialog_ = false;
    focusWithoutInk(assert(this.shadowRoot.querySelector('#disconnectButton')));

    if (Router.getInstance().getCurrentRoute() === routes.OS_SIGN_OUT) {
      Router.getInstance().navigateToPreviousRoute();
    }
  }

  /** @private */
  onDisconnectTap_() {
    Router.getInstance().navigateTo(routes.OS_SIGN_OUT);
  }

  /** @private */
  onSyncTap_() {
    // Users can go to sync subpage regardless of sync status.
    Router.getInstance().navigateTo(routes.SYNC);
  }

  /**
   * @param {!Event} e
   * @private
   */
  onAccountManagerTap_(e) {
    if (this.isAccountManagerEnabled_) {
      Router.getInstance().navigateTo(routes.ACCOUNT_MANAGER);
    }
  }

  /**
   * @param {string} iconUrl
   * @return {string} A CSS image-set for multiple scale factors.
   * @private
   */
  getIconImageSet_(iconUrl) {
    return getImage(iconUrl);
  }

  /**
   * @return {string}
   * @private
   */
  getProfileName_() {
    if (this.isAccountManagerEnabled_) {
      return loadTimeData.getString('osProfileName');
    }
    return this.profileName_;
  }

  /**
   * @param {!SyncStatus} syncStatus
   * @return {boolean} Whether to show the "Sign in to Chrome" button.
   * @private
   */
  showSignin_(syncStatus) {
    return loadTimeData.getBoolean('signinAllowed') && !syncStatus.signedIn;
  }

  /** @private */
  onAuthTokenChanged_() {
    if (this.authToken_ === undefined) {
      this.setModes_ = undefined;
    } else {
      this.setModes_ = (modes, credentials, onComplete) => {
        this.quickUnlockPrivate.setModes(
            this.authToken_.token, modes, credentials, () => {
              let result = true;
              if (chrome.runtime.lastError) {
                console.error(
                    'setModes failed: ' + chrome.runtime.lastError.message);
                result = false;
              }
              onComplete(result);
            });
      };
    }

    if (this.clearAccountPasswordTimeoutId_) {
      clearTimeout(this.clearAccountPasswordTimeoutId_);
    }
    if (this.authToken_ === undefined) {
      return;
    }
    // Clear |this.authToken_| after
    // |this.authToken_.tokenInfo.lifetimeSeconds|.
    // Subtract time from the expiration time to account for IPC delays.
    // Treat values less than the minimum as 0 for testing.
    const IPC_SECONDS = 2;
    const lifetimeMs = this.authToken_.lifetimeSeconds > IPC_SECONDS ?
        (this.authToken_.lifetimeSeconds - IPC_SECONDS) * 1000 :
        0;
    this.clearAccountPasswordTimeoutId_ = setTimeout(() => {
      this.authToken_ = undefined;
    }, lifetimeMs);
  }
}

customElements.define(
    OsSettingsPeoplePageElement.is, OsSettingsPeoplePageElement);
