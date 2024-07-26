// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// <if expr="not chromeos_ash">
import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {$, appendParam} from 'chrome://resources/js/util.js';
// </if>
// <if expr="chromeos_ash">
import {assert} from 'chrome://resources/ash/common/assert.js';
import {sendWithPromise} from 'chrome://resources/ash/common/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {$, appendParam} from 'chrome://resources/ash/common/util.js';

// </if>

import {OnHeadersReceivedDetails, SamlHandler} from './saml_handler.js';
import {PasswordAttributes} from './saml_password_attributes.js';
import {WebviewEventManager} from './webview_event_manager.js';
//clang-format on

/**
 * @fileoverview An UI component to authenticate to Chrome. The component hosts
 * IdP web pages in a webview. A client who is interested in monitoring
 * authentication events should subscribe itself via addEventListener(). After
 * initialization, call {@code load} to start the authentication flow.
 *
 * See go/cros-auth-design for details on Google API.
 */

  /**
   * Individual sync trusted vault key.
   * @typedef {{
   *   keyMaterial: ArrayBuffer,
   *   version: number,
   * }}
   */
export let SyncTrustedVaultKey;

/**
 * Individual sync trusted recovery method.
 * @typedef {{
 *   publicKey: ArrayBuffer,
 *   type: number,
 * }}
 */
export let SyncTrustedRecoveryMethod;

/**
 * Sync trusted vault encryption keys optionally passed with 'authCompleted'
 * message.
 * @typedef {{
 *   obfuscatedGaiaId: string,
 *   encryptionKeys: Array<SyncTrustedVaultKey>,
 *   trustedRecoveryMethods: Array<SyncTrustedRecoveryMethod>
 * }}
 */
export let SyncTrustedVaultKeys;

/**
 * Credentials passed with 'authCompleted' message.
 * `isAvailableInArc` field is optional and is used only on Chrome OS.
 * @typedef {{
 *   email: string,
 *   gaiaId: string,
 *   password: string,
 *   usingSAML: boolean,
 *   publicSAML: boolean,
 *   skipForNow: boolean,
 *   sessionIndex: string,
 *   trusted: boolean,
 *   services: Array,
 *   passwordAttributes: !PasswordAttributes,
 *   syncTrustedVaultKeys: !SyncTrustedVaultKeys,
 *   isAvailableInArc: (boolean|undefined),
 * }}
 */
export let AuthCompletedCredentials;

/**
 * Parameters for the authorization flow.
 * @typedef {{
 *   hl: string,
 *   gaiaUrl: string,
 *   authMode: AuthMode,
 *   isLoginPrimaryAccount: boolean,
 *   email: string,
 *   constrained: string,
 *   platformVersion: string,
 *   readOnlyEmail: boolean,
 *   service: string,
 *   dontResizeNonEmbeddedPages: boolean,
 *   clientId: string,
 *   clientVersion: (string|undefined),
 *   gaiaPath: string,
 *   emailDomain: string,
 *   showTos: string,
 *   extractSamlPasswordAttributes: boolean,
 *   flow: string,
 *   ignoreCrOSIdpSetting: boolean,
 *   enableGaiaActionButtons: boolean,
 *   forceDarkMode: boolean,
 *   enterpriseEnrollmentDomain: string,
 *   samlAclUrl: string,
 *   isSupervisedUser: boolean,
 *   isDeviceOwner: boolean,
 *   needPassword: (boolean|undefined),
 *   ssoProfile: string,
 *   urlParameterToAutofillSAMLUsername: string,
 *   frameUrl: URL,
 *   isFirstUser : (boolean|undefined),
 *   recordAccountCreation : (boolean|undefined),
 *   autoReloadAttempts : number,
 * }}
 */
export let AuthParams;

const SIGN_IN_HEADER = 'google-accounts-signin';
const EMBEDDED_FORM_HEADER = 'google-accounts-embedded';
const SERVICE_ID = 'chromeoslogin';
const BLANK_PAGE_URL = 'about:blank';

const GAIA_DONE_ELAPSED_TIME = 'ChromeOS.Gaia.Done.ElapsedTime';
const GAIA_CREATE_ACCOUNT_FIRST_USER =
      'ChromeOS.Gaia.CreateAccount.IsFirstUser';
const GAIA_DONE_OOBE_NEW_ACCOUNT =
      'ChromeOS.Gaia.Done.Oobe.NewAccount';

// Metric names for messages we get from Gaia.
const GAIA_MESSAGE_SAML_USER_INFO = 'ChromeOS.Gaia.Message.Saml.UserInfo';
const GAIA_MESSAGE_GAIA_USER_INFO = 'ChromeOS.Gaia.Message.Gaia.UserInfo';
const GAIA_MESSAGE_SAML_CLOSE_VIEW = 'ChromeOS.Gaia.Message.Saml.CloseView';
const GAIA_MESSAGE_GAIA_CLOSE_VIEW = 'ChromeOS.Gaia.Message.Gaia.CloseView';

/**
 * The source URL parameter for the constrained signin flow.
 */
export const CONSTRAINED_FLOW_SOURCE = 'chrome';

/**
 * Enum for the authorization mode, must match AuthMode defined in
 * chrome/browser/ui/webui/inline_login_ui.cc.
 * @enum {number}
 */
export const AuthMode = {
  DEFAULT: 0,
  OFFLINE: 1,
  DESKTOP: 2,
};

/**
 * Enum for the authorization type.
 * @enum {number}
 */
export const AuthFlow = {
  DEFAULT: 0,
  SAML: 1,
};

/**
 * Supported Authenticator params.
 * @type {!Array<string>}
 * @const
 */
export const SUPPORTED_PARAMS = [
  'gaiaId',        // Obfuscated GAIA ID to skip the email prompt page
                   // during the re-auth flow.
  'gaiaUrl',       // Gaia url to use.
  'gaiaPath',      // Gaia path to use without a leading slash.
  'hl',            // Language code for the user interface.
  'service',       // Name of Gaia service.
  'frameUrl',      // Initial frame URL to use. If empty defaults to
                   // gaiaUrl.
  'constrained',   // Whether authentication happens in a constrained
                   // window.
  'clientId',      // Chrome client id.
  'needPassword',  // Whether the host is interested in getting a password.
                   // If this set to |false|, |confirmPasswordCallback| is
                   // not called before dispatching |authCopleted|.
                   // Default is |true|.
  'flow',          // One of 'default', 'enterprise', or
                   // 'cfm' or 'enterpriseLicense'.
  'enterpriseDomainManager',     // Manager of the current domain. Can be
                                 // either a domain name (foo.com) or an email
                                 // address (admin@foo.com).
  'enterpriseEnrollmentDomain',  // Domain in which hosting device is (or
                                 // should be) enrolled.
  'emailDomain',                 // Value used to prefill domain for email.
  'chromeType',                  // Type of Chrome OS device, e.g. "chromebox".
  'clientVersion',               // Version of the Chrome build.
  'platformVersion',             // Version of the OS build.
  'releaseChannel',              // Installation channel.
  'endpointGen',                 // Current endpoint generation.
  'menuEnterpriseEnrollment',    // Enables "Enterprise enrollment" menu item.
  'lsbReleaseBoard',             // Chrome OS Release board name
  'isFirstUser',                 // True if this is non-enterprise device,
                                 // and there are no users yet.
  'obfuscatedOwnerId',           // Obfuscated device owner ID, if needed.
  'extractSamlPasswordAttributes',  // If enabled attempts to extract password
                                    // attributes from the SAML response.
  'ignoreCrOSIdpSetting',           // If set to true, causes Gaia to ignore 3P
                                    // SAML IdP SSO redirection policies (and
                                    // redirect to SAML IdPs by default).
  'ssoProfile',  // An identifier for the device's managing OU's
                 // SAML SSO setting. Used by the login screen to
                 // pass to Gaia.
  // The email can be passed to Gaia to let it know which user is trying to
  // sign in. Gaia behavior can be different depending on the `gaiaPath`: it
  // can either simply prefill the email field, but still allow modifying it,
  // or it can proceed straight to the authentication challenge for the
  // corresponding account, not allowing the user to modify the email.
  'email',
   // Determines which URL parameter will be used to pass the email to Gaia.
   // TODO(b/292087570): misleading name, should be either renamed or
   // removed completely (need to confirm if email_hint URL parameter
   // is still relevant for some flows).
  'readOnlyEmail',
  'realm',
  // If the authentication is done via external IdP, 'startsOnSamlPage'
  // indicates whether the flow should start on the IdP page.
  'startsOnSamlPage',
  // SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
  // (e.g. for SAML managed guest sessions).
  'samlAclUrl',
  'isSupervisedUser',  // True if the user is supervised user.
  'isDeviceOwner',     // True if the user is device owner.
  'doSamlRedirect',    // True if the authentication is done via external IdP.
  'rart',              // Encrypted reauth request token.
  // Url parameter name for SAML IdP web page which is used to autofill the
  // username.
  'urlParameterToAutofillSAMLUsername',
  'forceDarkMode',
  // A tri-state value which indicates the support level for passwordless login.
  // Refer to `GaiaView::PasswordlessSupportLevel` for details.
  'pwl',
  // Control if the account creation during sign in flow should be handled.
  'recordAccountCreation',
  // Url parameter for the number of automatic reloads done to the
  // authentication flow to avoid login page timeout. Added for
  // `DeviceAuthenticationFlowAutoReloadInterval` policy.
  'autoReloadAttempts',
];

// Timeout in ms to wait for the message from Gaia indicating end of the flow.
// Could be userInfo (The message is used to extract user services and to
// define whether or not the account is a child one) or closeView (specific
// message to indicate the end of the flow).
const GAIA_DONE_WAIT_TIMEOUT_MS = 5 * 1000;

/**
 * Extract domain name from an URL.
 * @param {string} url An URL string.
 * @return {string} The host name of the URL.
 */
function extractDomain(url) {
  const a = document.createElement('a');
  a.href = url;
  return a.hostname;
}

/**
 * Handlers for the HTML5 messages received from Gaia.
 * Each handler is a function that receives the Authenticator as 'this',
 * and the data field of the HTML5 Payload.
 */
const messageHandlers = {
  'attemptLogin'(msg) {
    this.setEmail_(msg.email);
    if (this.authMode === AuthMode.DESKTOP) {
      this.password_ = msg.password;
    }

    // We need to dispatch only first event, before user enters password.
    this.dispatchEvent(new CustomEvent('attemptLogin', {detail: msg.email}));
  },
  'dialogShown'(msg) {
    this.dispatchEvent(new Event('dialogShown'));
  },
  'dialogHidden'(msg) {
    this.dispatchEvent(new Event('dialogHidden'));
  },
  'backButton'(msg) {
    this.dispatchEvent(new CustomEvent('backButton', {detail: msg.show}));
  },
  'getAccounts'(msg) {
    this.dispatchEvent(new Event('getAccounts'));
  },
  'showView'(msg) {
    this.dispatchEvent(new Event('showView'));
  },
  'menuItemClicked'(msg) {
    this.dispatchEvent(new CustomEvent('menuItemClicked', {detail: msg.item}));
  },
  'identifierEntered'(msg) {
    this.setEmail_(msg.accountIdentifier);
    this.dispatchEvent(new CustomEvent(
        'identifierEntered',
        {detail: {accountIdentifier: msg.accountIdentifier}}));
  },
  'userInfo'(msg) {
    this.services_ = msg.services;
    this.servicesProvided_ = true;
    if (!this.authCompletedFired_) {
      const metric = this.authFlow === AuthFlow.SAML ?
          GAIA_MESSAGE_SAML_USER_INFO :
          GAIA_MESSAGE_GAIA_USER_INFO;
      chrome.send('metricsHandler:recordBooleanHistogram', [metric, true]);
    }
    if (this.email_ && this.gaiaId_ && this.sessionIndex_) {
      this.maybeCompleteAuth_();
    }
  },
  'showIncognito'(msg) {
    this.dispatchEvent(new Event('showIncognito'));
  },
  'setPrimaryActionLabel'(msg) {
    if (!this.enableGaiaActionButtons_) {
      return;
    }
    this.dispatchEvent(
        new CustomEvent('setPrimaryActionLabel', {detail: msg.value}));
  },
  'setPrimaryActionEnabled'(msg) {
    if (!this.enableGaiaActionButtons_) {
      return;
    }
    this.dispatchEvent(
        new CustomEvent('setPrimaryActionEnabled', {detail: msg.value}));
  },
  'setSecondaryActionLabel'(msg) {
    if (!this.enableGaiaActionButtons_) {
      return;
    }
    this.dispatchEvent(
        new CustomEvent('setSecondaryActionLabel', {detail: msg.value}));
  },
  'setSecondaryActionEnabled'(msg) {
    if (!this.enableGaiaActionButtons_) {
      return;
    }
    this.dispatchEvent(
        new CustomEvent('setSecondaryActionEnabled', {detail: msg.value}));
  },
  'setAllActionsEnabled'(msg) {
    if (!this.enableGaiaActionButtons_) {
      return;
    }
    this.dispatchEvent(
        new CustomEvent('setAllActionsEnabled', {detail: msg.value}));
  },
  'removeUserByEmail'(msg) {
    this.dispatchEvent(
        new CustomEvent('removeUserByEmail', {detail: msg.email}));
  },
  'exit'(msg) {
    this.dispatchEvent(new CustomEvent('exit'));
  },
  'syncTrustedVaultKeys'(msg) {
    this.syncTrustedVaultKeys_ = msg.value;
  },
  'closeView'(msg) {
    if (!this.authCompletedFired_) {
      if (!this.services_) {
        console.error('Authenticator: UserInfo should come before closeView');
      }
      const metric = this.authFlow === AuthFlow.SAML ?
          GAIA_MESSAGE_SAML_CLOSE_VIEW :
          GAIA_MESSAGE_GAIA_CLOSE_VIEW;
      chrome.send('metricsHandler:recordBooleanHistogram', [metric, true]);
    }

    this.closeViewReceived_ = true;
    if (this.email_ && this.gaiaId_ && this.sessionIndex_) {
      this.maybeCompleteAuth_();
    }
  },
  'getDeviceId'(msg) {
    this.dispatchEvent(new Event('getDeviceId'));
  },
};

/**
 * Old or not supported on Chrome OS messages.
 * @type {!Array<string>}
 * @const
 */
const IGNORED_MESSAGES_FROM_GAIA = [
  'clearOldAttempts',
  'showConfirmCancel',
];

/**
 * Initializes the authenticator component.
 */
export class Authenticator extends EventTarget {
  /**
   * @param {!WebView|string} webview The webview element or its ID to host
   *     IdP web pages.
   */
  constructor(webview) {
    super();

    /** @private {AuthFlow} The current auth flow of the hosted page.*/
    this.authFlow_ = AuthFlow.DEFAULT;
    /** @private {string} The domain name of the current auth page. */
    this.authDomain_ = '';
    /** @private {boolean}  Whether media access was requested. */
    this.videoEnabled_ = false;

    this.isLoaded_ = false;
    this.email_ = null;
    this.password_ = null;
    this.gaiaId_ = null, this.sessionIndex_ = null;
    this.skipForNow_ = false;
    /** @type {AuthMode} */
    this.authMode = AuthMode.DEFAULT;
    this.dontResizeNonEmbeddedPages = false;
    this.isFirstUser_ = false;
    this.isNewAccount = false;

    /**
     * @type {!SamlHandler|undefined}
     * @private
     */
    this.samlHandler_ = undefined;
    this.idpOrigin_ = null;
    this.initialFrameUrl_ = null;
    this.reloadUrl_ = null;
    this.trusted_ = true;
    this.readyFired_ = false;
    this.authCompletedFired_ = false;
    /**
     * @private {WebView|undefined}
     */
    this.webview_ = typeof webview === 'string' ?
        /** @type {WebView} */ ($(webview)) :
        webview;
    assert(this.webview_);
    this.enableGaiaActionButtons_ = false;
    this.webviewEventManager_ = new WebviewEventManager();

    this.clientId_ = null;

    this.confirmPasswordCallback = null;
    this.noPasswordCallback = null;
    this.onePasswordCallback = null;
    this.insecureContentBlockedCallback = null;
    this.samlApiUsedCallback = null;
    this.recordSamlProviderCallback = null;
    this.missingGaiaInfoCallback = null;
    this.needPassword = true;
    this.services_ = null;
    this.servicesProvided_ = false;
    this.waitApiPasswordConfirm_ = false;
    this.gaiaDoneTimer_ = null;
    /** @private {boolean} */
    this.isConstrainedWindow_ = false;
    this.samlAclUrl_ = null;
    /** @private {?SyncTrustedVaultKeys} */
    this.syncTrustedVaultKeys_ = null;
    this.closeViewReceived_ = false;
    this.gaiaStartTime = null;

    window.addEventListener(
        'message', e => this.onMessageFromWebview_(e), false);
    window.addEventListener('focus', () => this.onFocus_(), false);
    window.addEventListener('popstate', e => this.onPopState_(e), false);

    /**
     * @type {boolean}
     * @private
     */
    this.isDomLoaded_ = document.readyState !== 'loading';
    if (this.isDomLoaded_) {
      this.initializeAfterDomLoaded_();
    } else {
      document.addEventListener(
          'DOMContentLoaded', () => this.initializeAfterDomLoaded_());
    }
  }

  /** @return {AuthFlow} */
  get authFlow() {
    return this.authFlow_;
  }

  /**
   * Dispatches 'authFlowChange' event if the value changes.
   * @param {AuthFlow} value
   */
  set authFlow(value) {
    const previous = this.authFlow_;
    if (value !== previous) {
      this.authFlow_ = value;
      this.dispatchEvent(new CustomEvent('authFlowChange', {
        bubbles: true,
        composed: true,
        detail: {oldValue: previous, newValue: value},
      }));
    }
  }

  /** @return {string} */
  get authDomain() {
    return this.authDomain_;
  }

  /**
   * Dispatches 'authDomainChange' event if the value changes.
   * @param {string} domain
   */
  set authDomain(domain) {
    const previous = this.authDomain_;
    if (domain !== previous) {
      this.authDomain_ = domain;
      this.dispatchEvent(new CustomEvent('authDomainChange', {
        bubbles: true,
        composed: true,
        detail: {oldValue: previous, newValue: domain},
      }));
    }
  }

  /** @return {boolean} */
  get videoEnabled() {
    return this.videoEnabled_;
  }

  /**
   * Dispatches 'videoEnabledChange' event if the value changes.
   * @param {boolean} enabled
   */
  set videoEnabled(enabled) {
    const previous = this.videoEnabled_;
    if (enabled !== previous) {
      this.videoEnabled_ = enabled;
      this.dispatchEvent(new CustomEvent('videoEnabledChange', {
        bubbles: true,
        composed: true,
        detail: {oldValue: previous, newValue: enabled},
      }));
    }
  }

  /**
   * Reinitializes authentication parameters so that a failed login attempt
   * would not result in an infinite loop.
   */
  resetStates() {
    this.isLoaded_ = false;
    this.email_ = null;
    this.gaiaId_ = null;
    this.password_ = null;
    this.readyFired_ = false;
    this.skipForNow_ = false;
    this.sessionIndex_ = null;
    this.trusted_ = true;
    this.authFlow = AuthFlow.DEFAULT;
    this.samlHandler_.reset();
    this.videoEnabled = false;
    this.services_ = null;
    this.servicesProvided_ = false;
    this.waitApiPasswordConfirm_ = false;
    this.maybeClearGaiaTimeout_();
    this.syncTrustedVaultKeys_ = null;
    this.closeViewReceived_ = false;
    this.disableAllActions_();
  }

  /**
   * Resets the webview to the blank page.
   */
  resetWebview() {
    if (this.webview_.src && this.webview_.src !== BLANK_PAGE_URL) {
      this.webview_.src = BLANK_PAGE_URL;
    }
  }

  /**
   * Completes the part of the initialization that should happen after the
   * page's DOM has loaded.
   * @private
   */
  initializeAfterDomLoaded_() {
    this.isDomLoaded_ = true;
    this.bindToWebview_();
  }

  /**
   * Binds this authenticator to the current |webview_|.
   * @private
   */
  bindToWebview_() {
    assert(this.webview_);
    assert(this.webview_.request);
    assert(!this.samlHandler_);

    this.samlHandler_ =
        new SamlHandler(this.webview_, false /* startsOnSamlPage */);
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'insecureContentBlocked',
        e => this.onInsecureContentBlocked_(e));
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'authPageLoaded', e => this.onAuthPageLoaded_(e));
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'videoEnabled', () => this.videoEnabled = true);
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'apiPasswordAdded',
        e => this.onSamlApiPasswordAdded_(e));
    this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'apiAccountCreated',
          e => this.onSamlApiAccountCreated_(e));
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'apiPasswordConfirmed',
        e => this.onSamlApiPasswordConfirmed_(e));
    this.webviewEventManager_.addEventListener(
        this.samlHandler_, 'challengeMachineKeyRequired',
        e => this.onChallengeMachineKeyRequired_(e));

    this.webviewEventManager_.addEventListener(
        this.webview_, 'droplink', e => this.onDropLink_(e));
    this.webviewEventManager_.addEventListener(
        this.webview_, 'newwindow', e => this.onNewWindow_(e));
    this.webviewEventManager_.addEventListener(
        this.webview_, 'contentload', e => this.onContentLoad_(e));
    this.webviewEventManager_.addEventListener(
        this.webview_, 'loadabort', e => this.onLoadAbort_(e));
    this.webviewEventManager_.addEventListener(
        this.webview_, 'loadcommit', e => this.onLoadCommit_(e));

    this.webviewEventManager_.addWebRequestEventListener(
        this.webview_.request.onCompleted,
        details => this.onRequestCompleted_(details),
        {urls: ['<all_urls>'], types: ['main_frame']}, ['responseHeaders']);
    this.webviewEventManager_.addWebRequestEventListener(
        this.webview_.request.onHeadersReceived,
        details => this.onHeadersReceived_(details),
        {urls: ['<all_urls>'], types: ['main_frame', 'xmlhttprequest']},
        ['responseHeaders']);
  }

  /**
   * Unbinds this Authenticator from the currently bound webview.
   * @private
   */
  unbindFromWebview_() {
    assert(this.webview_);
    assert(this.samlHandler_);

    this.webviewEventManager_.removeAllListeners();

    this.webview_ = undefined;
    this.samlHandler_.unbindFromWebview();
    this.samlHandler_ = undefined;
  }

  /**
   * Re-binds to another webview.
   * @param {WebView} webview the new webview to be used by this
   *     Authenticator.
   * @private
   */
  rebindWebview_(webview) {
    if (!this.isDomLoaded_) {
      // We haven't bound to the previously set webview yet, so simply update
      // |webview_| to use the new element during the delayed initialization.
      this.webview_ = webview;
      return;
    }
    this.unbindFromWebview_();
    assert(!this.webview_);
    this.webview_ = webview;
    this.bindToWebview_();
  }

  /**
   * Copies attributes between nodes.
   * @param {!Element} fromNode source to copy attributes from
   * @param {!Element} toNode target to copy attributes to
   * @param {!Set<string>} skipAttributes specifies attributes to be skipped
   * @private
   */
  copyAttributes_(fromNode, toNode, skipAttributes) {
    for (let i = 0; i < fromNode.attributes.length; ++i) {
      const attribute = fromNode.attributes[i];
      if (!skipAttributes.has(attribute.nodeName)) {
        toNode.setAttribute(attribute.nodeName, attribute.nodeValue);
      }
    }
  }

  /**
   * Changes the 'partition' attribute of |webview_|. If |webview_| has
   * already navigated, this function re-creates it since the storage
   * partition of an active renderer process cannot change.
   * @param {string} newWebviewPartitionName the new partition
   */
  setWebviewPartition(newWebviewPartitionName) {
    if (!this.webview_.src) {
      // We have not navigated anywhere yet. Note that a webview's src
      // attribute does not allow a change back to "".
      this.webview_.partition = newWebviewPartitionName;
    } else if (this.webview_.partition !== newWebviewPartitionName) {
      // The webview has already navigated. We have to re-create it.
      const webivewParent = this.webview_.parentElement;

      // Copy all attributes except for partition and src from the previous
      // webview. Use the specified |newWebviewPartitionName|.
      const newWebview = document.createElement('webview');
      this.copyAttributes_(
          this.webview_, newWebview, new Set(['src', 'partition']));
      newWebview.partition = newWebviewPartitionName;

      webivewParent.replaceChild(newWebview, this.webview_);

      this.rebindWebview_(/** @type {WebView} */ (newWebview));
    }
  }

  /**
   * Loads the authenticator component with the given parameters.
   * @param {AuthMode} authMode Authorization mode.
   * @param {AuthParams} data Parameters for the authorization flow.
   */
  load(authMode, data) {
    this.authMode = authMode;
    this.resetStates();
    this.authCompletedFired_ = false;
    this.idpOrigin_ = data.gaiaUrl;
    this.isConstrainedWindow_ = data.constrained === '1';
    this.clientId_ = data.clientId;
    this.dontResizeNonEmbeddedPages = data.dontResizeNonEmbeddedPages;
    this.enableGaiaActionButtons_ = data.enableGaiaActionButtons;

    this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
    this.reloadUrl_ = data.frameUrl || this.initialFrameUrl_;
    this.samlAclUrl_ = data.samlAclUrl;
    this.setEmail_(data.email);

    if (data.startsOnSamlPage) {
      this.samlHandler_.startsOnSamlPage = true;
    }

    // True if this is non-enterprise device and there are no users yet.
    this.isFirstUser_ = !!data.isFirstUser;

    // Enable or disable handling account create message from Gaia.
    this.samlHandler_.shouldHandleAccountCreationMessage =
        !!data.recordAccountCreation;

    // Don't block insecure content for desktop flow because it lands on
    // http. Otherwise, block insecure content as long as gaia is https.
    this.samlHandler_.blockInsecureContent =
        authMode !== AuthMode.DESKTOP && this.idpOrigin_.startsWith('https://');
    this.samlHandler_.extractSamlPasswordAttributes =
        data.extractSamlPasswordAttributes;
    this.samlHandler_.urlParameterToAutofillSAMLUsername =
        data.urlParameterToAutofillSAMLUsername;

    this.needPassword = !('needPassword' in data) || data.needPassword;

    this.webview_.contextMenus.onShow.addListener(function(e) {
      e.preventDefault();
    });

    this.webview_.src = this.reloadUrl_;
    this.isLoaded_ = true;
    this.isNewAccount = false;
  }

  /**
   * Reloads the authenticator component.
   */
  reload() {
    this.resetStates();
    this.authCompletedFired_ = false;
    this.webview_.src = this.reloadUrl_;
    this.isLoaded_ = true;
  }

  /**
   * Called in response to 'getAccounts' event.
   * @param {Array<string>} accounts list of emails
   */
  getAccountsResponse(accounts) {
    this.sendMessageToWebview('accountsListed', accounts);
  }

  /**
   * Called in response to 'getDeviceId' event.
   * @param {string} deviceId Device ID.
   */
  getDeviceIdResponse(deviceId) {
    this.sendMessageToWebview('deviceIdFetched', deviceId);
  }

  constructInitialFrameUrl_(data) {
    assert(this.idpOrigin_ !== undefined, "this.idpOrigin_ must be defined");
    assert(data.gaiaPath !== undefined, "data.gaiaPath must be defined");
    let url = this.idpOrigin_ + data.gaiaPath;

    if (data.doSamlRedirect) {
      url = appendParam(url, 'domain', data.enterpriseEnrollmentDomain);
      if (data.ssoProfile) {
        url = appendParam(url, 'sso_profile', data.ssoProfile);
      }
      url = appendParam(
          url, 'continue',
          data.gaiaUrl + 'programmatic_auth_chromeos?hl=' + data.hl +
              '&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin&' +
              'client_id=' + encodeURIComponent(data.clientId) +
              '&access_type=offline');
      if (data.rart) {
        url = appendParam(url, 'rart', data.rart);
      }
      if (data.autoReloadAttempts) {
        url = appendParam(url, 'auto_reload_attempts', data.autoReloadAttempts);
      }

      return url;
    }

    if (data.chromeType) {
      url = appendParam(url, 'chrometype', data.chromeType);
    }
    if (data.clientId) {
      url = appendParam(url, 'client_id', data.clientId);
    }
    if (data.enterpriseDomainManager) {
      url = appendParam(url, 'devicemanager', data.enterpriseDomainManager);
    }
    if (data.clientVersion) {
      url = appendParam(url, 'client_version', data.clientVersion);
    }
    if (data.platformVersion) {
      url = appendParam(url, 'platform_version', data.platformVersion);
    }
    if (data.releaseChannel) {
      url = appendParam(url, 'release_channel', data.releaseChannel);
    }
    if (data.endpointGen) {
      url = appendParam(url, 'endpoint_gen', data.endpointGen);
    }
    if (data.menuEnterpriseEnrollment) {
      url = appendParam(url, 'mi', 'ee');
    }

    if (data.lsbReleaseBoard) {
      url = appendParam(url, 'chromeos_board', data.lsbReleaseBoard);
    }
    if (data.isFirstUser) {
      url = appendParam(url, 'is_first_user', 'true');
    }
    if (data.obfuscatedOwnerId) {
      url = appendParam(url, 'obfuscated_owner_id', data.obfuscatedOwnerId);
    }
    if (data.hl) {
      url = appendParam(url, 'hl', data.hl);
    }
    if (data.gaiaId) {
      url = appendParam(url, 'user_id', data.gaiaId);
    }
    if (data.email) {
      if (data.readOnlyEmail) {
        url = appendParam(url, 'Email', data.email);
      } else {
        url = appendParam(url, 'email_hint', data.email);
      }
    }
    if (this.isConstrainedWindow_) {
      url = appendParam(url, 'source', CONSTRAINED_FLOW_SOURCE);
    }
    if (data.flow) {
      url = appendParam(url, 'flow', data.flow);
    }
    if (data.emailDomain) {
      // Use 'hd' (hosted domain) as the argument to show an email domain.
      url = appendParam(url, 'hd', data.emailDomain);
    }
    if (data.showTos) {
      url = appendParam(url, 'show_tos', data.showTos);
    }
    if (data.ignoreCrOSIdpSetting === true) {
      url = appendParam(url, 'ignoreCrOSIdpSetting', 'true');
    }
    if (data.enableGaiaActionButtons) {
      url = appendParam(url, 'use_native_navigation', '1');
    }
    if (data.isSupervisedUser) {
      url = appendParam(url, 'is_supervised', '1');
    }
    if (data.isDeviceOwner) {
      url = appendParam(url, 'is_device_owner', '1');
    }
    if (data.rart) {
      url = appendParam(url, 'rart', data.rart);
    }
    if (data.forceDarkMode) {
      url = appendParam(url, 'color_scheme', 'dark');
    }
    if (data.pwl) {
      url = appendParam(url, 'pwl', data.pwl);
    }
    if (data.autoReloadAttempts) {
      url = appendParam(url, 'auto_reload_attempts', data.autoReloadAttempts);
    }

    return url;
  }

  /**
   * Dispatches the 'ready' event if it hasn't been dispatched already for the
   * current content.
   * @private
   */
  fireReadyEvent_() {
    if (!this.readyFired_) {
      this.dispatchEvent(new Event('ready'));
      this.readyFired_ = true;
    }
  }

  /**
   * Invoked when a main frame request in the webview has completed.
   * @private
   */
  onRequestCompleted_(details) {
    const currentUrl = details.url;

    if (!currentUrl.startsWith('https')) {
      this.trusted_ = false;
    }

    if (this.isConstrainedWindow_) {
      let isEmbeddedPage = false;
      if (this.idpOrigin_ && currentUrl.startsWith(this.idpOrigin_)) {
        const headers = details.responseHeaders;
        for (let i = 0; headers && i < headers.length; ++i) {
          if (headers[i].name.toLowerCase() === EMBEDDED_FORM_HEADER) {
            isEmbeddedPage = true;
            break;
          }
        }
      }

      // In some cases, non-embedded pages should not be resized.  For
      // example, on desktop when reauthenticating for purposes of unlocking
      // a profile, resizing would cause a browser window to open in the
      // system profile, which is not allowed.
      if (!isEmbeddedPage && !this.dontResizeNonEmbeddedPages) {
        this.dispatchEvent(new CustomEvent('resize', {detail: currentUrl}));
        return;
      }
    }

    this.updateHistoryState_(currentUrl);
  }

  /**
   * Manually updates the history. Invoked upon completion of a webview
   * navigation.
   * @param {string} url Request URL.
   * @private
   */
  updateHistoryState_(url) {
    if (history.state && history.state.url !== url) {
      history.pushState({url: url}, '');
    } else {
      history.replaceState({url: url}, '');
    }
  }

  /**
   * Invoked when the sign-in page takes focus.
   * @private
   */
  onFocus_() {
    if (this.authMode === AuthMode.DESKTOP &&
        document.activeElement === document.body) {
      this.webview_.focus();
    }
  }

  /**
   * Invoked when the history state is changed.
   * @param {!Event} e The popstate event being triggered.
   * @private
   */
  onPopState_(e) {
    const state = e.state;
    if (state && state.url) {
      this.webview_.src = state.url;
    }
  }

  /**
   * Invoked when headers are received in the main frame of the webview. It
   * reads the authenticated user info from a signin header.
   * @param {OnHeadersReceivedDetails} details
   * @private
   */
  onHeadersReceived_(details) {
    if (this.authCompletedFired_) {
      // SIGN_IN_HEADER could be sent more thane once. Sometimes already
      // after authentication completed. Return here to avoid triggering
      // maybeCompleteAuth which shows "create your password screen" because
      // scraped passwords are wiped at that point.
      return;
    }
    const currentUrl = details.url;
    if (this.idpOrigin_ === null || this.idpOrigin_ === undefined ||
      !currentUrl.startsWith(this.idpOrigin_)) {
      return;
    }

    const headers = details.responseHeaders;
    for (let i = 0; headers && i < headers.length; ++i) {
      const header = headers[i];
      const headerName = header.name.toLowerCase();
      if (headerName === SIGN_IN_HEADER) {
        const headerValues = header.value.toLowerCase().split(',');
        const signinDetails = {};
        headerValues.forEach(function(e) {
          const pair = e.split('=');
          signinDetails[pair[0].trim()] = pair[1].trim();
        });
        // Removes "" around.
        const email = signinDetails['email'].slice(1, -1);
        this.setEmail_(email);
        this.gaiaId_ = signinDetails['obfuscatedid'].slice(1, -1);
        this.sessionIndex_ = signinDetails['sessionindex'];
      }
    }
  }

  /**
   * Returns true if given HTML5 message is received from `this.idpOrigin_` -
   * which is usually Gaia.
   * @param {Object} e Payload of the received HTML5 message.
   */
  isGaiaMessage_(e) {
    if (!this.isWebviewEvent_(e)) {
      return false;
    }

    // The event origin does not have a trailing slash, while `idpOrigin_` does.
    // Strip the trailing slash from `idpOrigin_` before comparison.
    if (e.origin !== this.idpOrigin_.substring(0, this.idpOrigin_.length - 1)) {
      return false;
    }

    // Gaia messages must be an object with 'method' property.
    if (typeof e.data !== 'object' || !e.data.hasOwnProperty('method')) {
      return false;
    }
    return true;
  }

  /**
   * Invoked when an HTML5 message is received from the webview element.
   * @param {Object} e Payload of the received HTML5 message.
   * @private
   */
  onMessageFromWebview_(e) {
    if (!this.isGaiaMessage_(e)) {
      return;
    }

    const msg = e.data;
    if (msg.method in messageHandlers) {
      if (this.authCompletedFired_) {
        console.warn(msg.method + ' message sent after auth completed');
      }
      messageHandlers[msg.method].call(this, msg);
    } else if (!IGNORED_MESSAGES_FROM_GAIA.includes(msg.method)) {
      console.warn('Unrecognized message from GAIA: ' + msg.method);
    }
  }

  /**
   * Invoked to send a HTML5 message with attached data to the webview
   * element.
   * @param {string} messageType Type of the HTML5 message.
   * @param {string|Object=} messageData Data to be attached to the message.
   */
  sendMessageToWebview(messageType, messageData = null) {
    const currentUrl = this.webview_.src;
    let payload = undefined;
    if (messageData) {
      payload = {type: messageType, data: messageData};
    } else {
      // TODO(crbug.com/1116343): Use new message format when it will be
      // available in production.
      payload = messageType;
    }

    this.webview_.contentWindow.postMessage(payload, currentUrl);
  }

  /**
   * Invoked by the hosting page to verify the Saml password.
   */
  verifyConfirmedPassword(password) {
    if (!this.samlHandler_.verifyConfirmedPassword(password)) {
      this.confirmPasswordCallback(
          this.email_, this.samlHandler_.scrapedPasswordCount);
      return;
    }

    this.password_ = password;
    this.onAuthCompleted_();
  }

  /**
   * Check Saml flow and start password confirmation flow if needed.
   * Otherwise, continue with auto completion.
   * @private
   */
  maybeCompleteAuth_() {
    if (this.authCompletedFired_) {
      return;
    }
    const missingGaiaInfo =
        !this.email_ || !this.gaiaId_ || !this.sessionIndex_;
    if (missingGaiaInfo && !this.skipForNow_) {
      if (this.missingGaiaInfoCallback) {
        this.missingGaiaInfoCallback();
      }

      this.webview_.src = this.initialFrameUrl_;
      return;
    }

    // Could be set either by `userInfo` message or by the
    // `onGaiaDoneTimeout_`.
    const userInfoAvailable = !!this.services_;

    const gaiaDone = userInfoAvailable && this.closeViewReceived_ &&
        !this.waitApiPasswordConfirm_;

    if (gaiaDone) {
      this.maybeRecordGaiaElapsedTime_();
      this.maybeRecordAccountFreshnessInOobe_();
      this.maybeClearGaiaTimeout_();
    } else if (this.gaiaDoneTimer_) {
      // Early out if `gaiaDoneTimer_` is running.
      return;
    } else {
      this.gaiaStartTime = Date.now();
      // Start `gaiaDoneTimer_` if Gaia is not yet done.
      this.gaiaDoneTimer_ = window.setTimeout(
          () => this.onGaiaDoneTimeout_(), GAIA_DONE_WAIT_TIMEOUT_MS);
      return;
    }

    if (this.recordSamlProviderCallback && this.authFlow === AuthFlow.SAML) {
      // Makes distinction between different SAML providers
      this.recordSamlProviderCallback(this.samlHandler_.x509certificate || '');
    }

    if (this.samlHandler_.samlApiUsed) {
      if (this.samlApiUsedCallback) {
        // Makes distinction between Gaia and Chrome Credentials Passing API
        // login to properly fill ChromeOS.SAML.ApiLogin metrics.
        this.samlApiUsedCallback(this.authFlow === AuthFlow.SAML);
      }
      this.password_ = this.samlHandler_.apiPasswordBytes;
      this.onAuthCompleted_();
      return;
    }

    if (this.samlHandler_.scrapedPasswordCount === 0) {
      if (this.noPasswordCallback) {
        this.noPasswordCallback(this.email_);
        return;
      }

      // Fall through to finish the auth flow even if this.needPassword
      // is true. This is because the flag is used as an intention to get
      // password when it is available but not a mandatory requirement.
      console.warn('Authenticator: No password scraped for SAML.');
    } else if (this.needPassword) {
      if (this.samlHandler_.scrapedPasswordCount === 1) {
        // If we scraped exactly one password, we complete the
        // authentication right away.
        this.password_ = this.samlHandler_.firstScrapedPassword;
        if (this.onePasswordCallback) {
          this.onePasswordCallback();
        }
        this.onAuthCompleted_();
        return;
      }

      if (this.confirmPasswordCallback) {
        // Confirm scraped password. The flow follows in
        // verifyConfirmedPassword.
        this.confirmPasswordCallback(
            this.email_, this.samlHandler_.scrapedPasswordCount);
        return;
      }
    }

    this.onAuthCompleted_();
  }

  /**
   * Invoked to complete the authentication using the password the user
   * enters manually for SAML IdPs that do not use Chrome Credentials Passing
   * API and we couldn't scrape their password input.
   */
  completeAuthWithManualPassword(password) {
    this.password_ = password;
    this.onAuthCompleted_();
  }

  /**
  /**
   * Asserts the |arr| which is known as |nameOfArr| is an array of strings.
   * @private
   */
  assertStringArray_(arr, nameOfArr) {
    console.assert(
        Array.isArray(arr), 'FATAL: Bad %s type: %s', nameOfArr, typeof arr);
    for (let i = 0; i < arr.length; ++i) {
      this.assertStringElement_(arr[i], nameOfArr, i);
    }
  }

  /**
   * Asserts the |dict| which is known as |nameOfDict| is a dict of strings.
   * @private
   */
  assertStringDict_(dict, nameOfDict) {
    console.assert(
        typeof dict === 'object', 'FATAL: Bad %s type: %s', nameOfDict,
        typeof dict);
    for (const key in dict) {
      this.assertStringElement_(dict[key], nameOfDict, key);
    }
  }

  /** Asserts an element |elem| in a certain collection is a string. */
  assertStringElement_(elem, nameOfCollection, index) {
    console.assert(
        typeof elem === 'string', 'FATAL: Bad %s[%s] type: %s',
        nameOfCollection, index, typeof elem);
  }

  /**
   * Invoked to process authentication completion.
   * @private
   */
  onAuthCompleted_() {
    assert(
        this.skipForNow_ ||
        (this.email_ && this.gaiaId_ && this.sessionIndex_));
    let scrapedPasswords = [];
    if (this.authFlow === AuthFlow.SAML && !this.samlHandler_.samlApiUsed) {
      scrapedPasswords = this.samlHandler_.scrapedPasswords;
    }
    // Chrome will crash on incorrect data type, so log some error message
    // here.
    if (this.services_) {
      this.assertStringArray_(this.services_, 'services');
    }
    let passwordAttributes = {};
    if (this.authFlow === AuthFlow.SAML &&
        this.samlHandler_.extractSamlPasswordAttributes) {
      passwordAttributes = this.samlHandler_.passwordAttributes;
    }
    this.assertStringDict_(passwordAttributes, 'passwordAttributes');
    this.dispatchEvent(new CustomEvent(
        'authCompleted',
        // TODO(rsorokin): get rid of the stub values.
        {
          detail: {
            email: this.email_ || '',
            gaiaId: this.gaiaId_ || '',
            password: this.password_ || '',
            usingSAML: this.authFlow === AuthFlow.SAML,
            scrapedSAMLPasswords: scrapedPasswords,
            publicSAML: this.samlAclUrl_ || false,
            skipForNow: this.skipForNow_,
            sessionIndex: this.sessionIndex_ || '',
            trusted: this.trusted_,
            services: this.services_ || [],
            servicesProvided: this.servicesProvided_,
            passwordAttributes: passwordAttributes,
            syncTrustedVaultKeys: this.syncTrustedVaultKeys_ || {},
          },
        }));
    this.resetStates();
    this.authCompletedFired_ = true;
  }

  /**
   * Invoked when |samlHandler_| fires 'insecureContentBlocked' event.
   * @private
   */
  onInsecureContentBlocked_(e) {
    if (!this.isLoaded_) {
      return;
    }

    if (this.insecureContentBlockedCallback) {
      this.insecureContentBlockedCallback(e.detail.url);
    } else {
      console.error('Authenticator: Insecure content blocked.');
    }
  }

  /**
   * Invoked when |samlHandler_| fires 'authPageLoaded' event.
   * @private
   */
  onAuthPageLoaded_(e) {
    if (!this.isLoaded_) {
      return;
    }

    if (!e.detail.isSAMLPage) {
      return;
    }

    this.authFlow = AuthFlow.SAML;

    this.webview_.focus();
    this.fireReadyEvent_();
  }

  /**
   * Invoked when |samlHandler_| fires 'apiPasswordAdded' event. Could be from
   * 3rd-party SAML IdP or Gaia which also uses the API.
   * @private
   */
  onSamlApiPasswordAdded_(e) {
    this.dispatchEvent(new Event('apiPasswordAdded'));
    this.waitApiPasswordConfirm_ = true;

    // Saml API 'add' password might be received after the 'loadcommit'
    // event. In such case, maybeCompleteAuth_ should be attempted again if
    // GAIA ID is available.
    if (this.gaiaId_) {
      this.maybeCompleteAuth_();
    }
  }

  /**
   * Invoked when |samlHandler_| fires 'apiAccountCreated' event.
   * @private
   */
  onSamlApiAccountCreated_(e) {
    this.isNewAccount = true;
    this.recordAccountCreated_();
  }

  /**
   * Invoked when |samlHandler_| fires 'apiPasswordConfirmed' event. Could be
   * from 3rd-party SAML IdP or Gaia which also uses the API.
   * @private
   */
  onSamlApiPasswordConfirmed_(e) {
    this.waitApiPasswordConfirm_ = false;
    if (this.gaiaId_) {
      this.maybeCompleteAuth_();
    }
  }

  /**
   * Invoked when |samlHandler_| fires 'challengeMachineKeyRequired' event.
   * @private
   */
  onChallengeMachineKeyRequired_(e) {
    sendWithPromise('samlChallengeMachineKey', e.detail.url, e.detail.challenge)
        .then(e.detail.callback);
  }

  /**
   * Invoked when a link is dropped on the webview.
   * @private
   */
  onDropLink_(e) {
    this.dispatchEvent(new CustomEvent('dropLink', {detail: e.url}));
  }

  /**
   * Invoked when the webview attempts to open a new window.
   * @private
   */
  onNewWindow_(e) {
    this.dispatchEvent(new CustomEvent('newWindow', {detail: e}));
  }

  /**
   * Invoked when a new document is loaded.
   * @private
   */
  onContentLoad_(e) {
    if (this.isConstrainedWindow_) {
      // Signin content in constrained windows should not zoom. Isolate the
      // webview from the zooming of other webviews using the 'per-view'
      // zoom mode, and then set it to 100% zoom.
      this.webview_.setZoomMode('per-view');
      this.webview_.setZoom(1);
    }

    // Posts a message to IdP pages to initiate communication.
    const currentUrl = this.webview_.src;
    if (this.idpOrigin_ && currentUrl.startsWith(this.idpOrigin_)) {
      const msg = {
        'method': 'handshake',
      };

      // |this.webview_.contentWindow| may be null after network error
      // screen is shown. See crbug.com/770999.
      if (this.webview_.contentWindow) {
        this.webview_.contentWindow.postMessage(msg, currentUrl);
      } else {
        console.error('Authenticator: contentWindow is null.');
      }

      this.fireReadyEvent_();
      // Focus webview after dispatching event when webview is already
      // visible.
      this.webview_.focus();
    } else if (currentUrl === BLANK_PAGE_URL) {
      this.fireReadyEvent_();
    } else if (currentUrl === this.samlAclUrl_) {
      this.skipForNow_ = true;
      this.onAuthCompleted_();
    }
  }

  /**
   * Invoked when the webview fails loading a page.
   * @private
   */
  onLoadAbort_(e) {
    if (this.samlHandler_.isIntentionalAbort()) {
      return;
    }

    // Ignore errors from subframe loads, as these should not cause an error
    // screen to be displayed. When a subframe load is triggered, it means
    // that the main frame load has succeeded, so the host is reachable in
    // general.
    if (!e.isTopLevel) {
      return;
    }

    this.dispatchEvent(new CustomEvent(
        'loadAbort', {detail: {error_code: e.code, src: e.url}}));
  }

  /**
   * Invoked when the webview navigates withing the current document.
   * @private
   */
  onLoadCommit_(e) {
    if (this.gaiaId_) {
      this.maybeCompleteAuth_();
    }
    if (e.isTopLevel) {
      this.authDomain = extractDomain(e.url);
    }
  }

  /**
   * Returns |true| if event |e| was sent from the hosted webview.
   * @private
   */
  isWebviewEvent_(e) {
    const webviewWindow = this.webview_.contentWindow;
    return !!webviewWindow && webviewWindow === e.source;
  }

  /**
   * Callback for the user info message waiting timeout.
   * @private
   */
  onGaiaDoneTimeout_() {
    if (!this.services_) {
      console.warn('Gaia done timeout: Forcing empty services.');
      this.services_ = [];
      const metric = this.authFlow === AuthFlow.SAML ?
          GAIA_MESSAGE_SAML_USER_INFO :
          GAIA_MESSAGE_GAIA_USER_INFO;
      chrome.send('metricsHandler:recordBooleanHistogram', [metric, false]);
    }

    if (!this.closeViewReceived_) {
      console.warn('Gaia done timeout: closeView was not called.');
      this.closeViewReceived_ = true;

      const metric = this.authFlow === AuthFlow.SAML ?
          GAIA_MESSAGE_SAML_CLOSE_VIEW :
          GAIA_MESSAGE_GAIA_CLOSE_VIEW;
      chrome.send('metricsHandler:recordBooleanHistogram', [metric, false]);
    }

    if (this.waitApiPasswordConfirm_) {
      // Log duplicates the log from the saml handler. The message is used by
      // the tast test to catch failures.
      console.warn('SamlHandler.onAPICall_: API password was not confirmed');
      this.samlHandler_.recordPasswordNotConfirmedError();
      this.waitApiPasswordConfirm_ = false;
    }

    this.maybeClearGaiaTimeout_();
    this.maybeCompleteAuth_();
  }

  /**
   * @private
   */
  maybeRecordGaiaElapsedTime_() {
    if (!this.gaiaStartTime) {
      return;
    }
    chrome.send('metricsHandler:recordTime', [
      GAIA_DONE_ELAPSED_TIME,
      Date.now() - this.gaiaStartTime,
    ]);
    this.gaiaStartTime = null;
  }

  /**
   * Record if the sign-in account in Oobe is an existing account or new
   * account.
   * @private
   */
  maybeRecordAccountFreshnessInOobe_() {
      // Record the metric if the record new account feature
      // flag is enabled. This metric is recorded only for the sign-in
      // event happens in Oobe.
      if (!this.samlHandler_.shouldHandleAccountCreationMessage ||
          !this.isFirstUser_) {
        return;
      }
      chrome.send('metricsHandler:recordBooleanHistogram', [
        GAIA_DONE_OOBE_NEW_ACCOUNT,
        this.isNewAccount
      ]);
      this.isNewAccount = false;
    }

  /**
   * Record new account creation.
   * @private
   */
  recordAccountCreated_() {
    // Record true account is created during the first sign in event
    // and false if another account existed.
    // TODO (b/307591058): add metric to track if account is created
    // during login or not.
    chrome.send('metricsHandler:recordBooleanHistogram',[
      GAIA_CREATE_ACCOUNT_FIRST_USER,
      this.isFirstUser_
    ]);
  }

  /**
   * @private
   */
  maybeClearGaiaTimeout_() {
    if (!this.gaiaDoneTimer_) {
      return;
    }
    window.clearTimeout(this.gaiaDoneTimer_);
    this.gaiaDoneTimer_ = null;
  }

  /**
   * Disables all navigation actions until explicitly re-enabled by GAIA.
   * @private
   */
  disableAllActions_() {
    this.dispatchEvent(
        new CustomEvent('setAllActionsEnabled', {detail: false}));
  }

  /**
   * Set the user's email.
   * @param {string} email New email value.
   * @private
   */
  setEmail_(email) {
    this.email_ = email;
    this.samlHandler_.email = email;
  }
}
