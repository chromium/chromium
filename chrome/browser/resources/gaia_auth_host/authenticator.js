// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="saml_handler.js">
// Note: webview_event_manager.js is already included by saml_handler.js.

// clang-format off
// #import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js'
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {$, appendParam} from 'chrome://resources/js/util.m.js';
// #import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

// #import {SamlHandler, OnHeadersReceivedDetails} from './saml_handler.m.js';
// #import {WebviewEventManager} from './webview_event_manager.m.js';
// #import {PasswordAttributes} from './saml_password_attributes.m.js';
// clang-format on

/**
 * @fileoverview An UI component to authenticate to Chrome. The component hosts
 * IdP web pages in a webview. A client who is interested in monitoring
 * authentication events should subscribe itself via addEventListener(). After
 * initialization, call {@code load} to start the authentication flow.
 *
 * See go/cros-auth-design for details on Google API.
 */

cr.define('cr.login', function() {
  /* #ignore */ 'use strict';

  /**
   * Credentials passed with 'authCompleted' message.
   * @typedef {{
   *   email: string,
   *   gaiaId: string,
   *   password: string,
   *   usingSAML: boolean,
   *   publicSAML: boolean,
   *   chooseWhatToSync: boolean,
   *   skipForNow: boolean,
   *   sessionIndex: string,
   *   trusted: boolean,
   *   services: Array,
   *   passwordAttributes: !PasswordAttributes
   * }}
   */
  /* #export */ let AuthCompletedCredentials;

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
   *   gaiaPath: string,
   *   emailDomain: string,
   *   showTos: string,
   *   extractSamlPasswordAttributes: boolean,
   *   flow: string,
   *   ignoreCrOSIdpSetting: boolean,
   *   enableGaiaActionButtons: boolean,
   *   enterpriseEnrollmentDomain: string,
   *   samlAclUrl: string
   * }}
   */
  /* #export */ let AuthParams;

  // TODO(rogerta): should use gaia URL from GaiaUrls::gaia_url() instead
  // of hardcoding the prod URL here.  As is, this does not work with staging
  // environments.
  const IDP_ORIGIN = 'https://accounts.google.com/';
  const SIGN_IN_HEADER = 'google-accounts-signin';
  const EMBEDDED_FORM_HEADER = 'google-accounts-embedded';
  const LOCATION_HEADER = 'location';
  const SERVICE_ID = 'chromeoslogin';
  const EMBEDDED_SETUP_CHROMEOS_ENDPOINT_V2 = 'embedded/setup/v2/chromeos';
  const SAML_REDIRECTION_PATH = 'samlredirect';
  const BLANK_PAGE_URL = 'about:blank';

  /**
   * The source URL parameter for the constrained signin flow.
   */
  /* #export */ const CONSTRAINED_FLOW_SOURCE = 'chrome';

  /**
   * Enum for the authorization mode, must match AuthMode defined in
   * chrome/browser/ui/webui/inline_login_ui.cc.
   * @enum {number}
   */
  /* #export */ const AuthMode = {DEFAULT: 0, OFFLINE: 1, DESKTOP: 2};

  /**
   * Enum for the authorization type.
   * @enum {number}
   */
  /* #export */ const AuthFlow = {DEFAULT: 0, SAML: 1};

  /**
   * Supported Authenticator params.
   * @type {!Array<string>}
   * @const
   */
  const SUPPORTED_PARAMS = [
    'gaiaId',        // Obfuscated GAIA ID to skip the email prompt page
                     // during the re-auth flow.
    'gaiaUrl',       // Gaia url to use.
    'gaiaPath',      // Gaia path to use without a leading slash.
    'hl',            // Language code for the user interface.
    'service',       // Name of Gaia service.
    'frameUrl',      // Initial frame URL to use. If empty defaults to
                     // gaiaUrl.
    'constrained',   // Whether the extension is loaded in a constrained
                     // window.
    'clientId',      // Chrome client id.
    'needPassword',  // Whether the host is interested in getting a password.
                     // If this set to |false|, |confirmPasswordCallback| is
                     // not called before dispatching |authCopleted|.
                     // Default is |true|.
    'flow',          // One of 'default', 'enterprise', or 'theftprotection'.
    'enterpriseDisplayDomain',     // Current domain name to be displayed.
    'enterpriseEnrollmentDomain',  // Domain in which hosting device is (or
                                   // should be) enrolled.
    'emailDomain',                 // Value used to prefill domain for email.
    'chromeType',                // Type of Chrome OS device, e.g. "chromebox".
    'clientVersion',             // Version of the Chrome build.
    'platformVersion',           // Version of the OS build.
    'releaseChannel',            // Installation channel.
    'endpointGen',               // Current endpoint generation.
    'menuEnterpriseEnrollment',  // Enables "Enterprise enrollment" menu item.
    'lsbReleaseBoard',           // Chrome OS Release board name
    'isFirstUser',               // True if this is non-enterprise device,
                                 // and there are no users yet.
    'obfuscatedOwnerId',         // Obfuscated device owner ID, if needed.
    'extractSamlPasswordAttributes',  // If enabled attempts to extract password
                                      // attributes from the SAML response.
    'ignoreCrOSIdpSetting',  // If set to true, causes Gaia to ignore 3P
                             // SAML IdP SSO redirection policies (and
                             // redirect to SAML IdPs by default).

    // The email fields allow for the following possibilities:
    //
    // 1/ If 'email' is not supplied, then the email text field is blank and the
    // user must type an email to proceed.
    //
    // 2/ If 'email' is supplied, and 'readOnlyEmail' is truthy, then the email
    // is hardcoded and the user cannot change it.  The user is asked for
    // password.  This is useful for re-auth scenarios, where chrome needs the
    // user to authenticate for a specific account and only that account.
    //
    // 3/ If 'email' is supplied, and 'readOnlyEmail' is falsy, gaia will
    // prefill the email text field using the given email address, but the user
    // can still change it and then proceed.  This is used on desktop when the
    // user disconnects their profile then reconnects, to encourage them to use
    // the same account.
    'email',
    'readOnlyEmail',
    'realm',
    // If the authentication is done via external IdP, 'startsOnSamlPage'
    // indicates whether the flow should start on the IdP page.
    'startsOnSamlPage',
    // SAML assertion consumer URL, used to detect when Gaia-less SAML flows end
    // (e.g. for SAML managed guest sessions).
    'samlAclUrl',
  ];

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
      this.email_ = msg.email;
      if (this.authMode == AuthMode.DESKTOP) {
        this.password_ = msg.password;
      }
      this.isSamlUserPasswordless_ = null;

      this.chooseWhatToSync_ = msg.chooseWhatToSync;
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
      this.dispatchEvent(
          new CustomEvent('menuItemClicked', {detail: msg.item}));
    },
    'identifierEntered'(msg) {
      this.dispatchEvent(new CustomEvent(
          'identifierEntered',
          {detail: {accountIdentifier: msg.accountIdentifier}}));
    },
    'userInfo'(msg) {
      this.services_ = msg.services;
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
    }
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
  /* #export */ class Authenticator extends cr.EventTarget {
    /**
     * @param {!WebView|string} webview The webview element or its ID to host
     *     IdP web pages.
     */
    constructor(webview) {
      super();

      this.isLoaded_ = false;
      this.email_ = null;
      this.password_ = null;
      this.gaiaId_ = null, this.sessionIndex_ = null;
      this.chooseWhatToSync_ = false;
      this.skipForNow_ = false;
      this.authFlow = AuthFlow.DEFAULT;
      /** @type {AuthMode} */
      this.authMode = AuthMode.DEFAULT;
      this.dontResizeNonEmbeddedPages = false;

      this.authDomain = '';
      /**
       * @type {!cr.login.SamlHandler|undefined}
       * @private
       */
      this.samlHandler_ = undefined;
      this.videoEnabled = false;
      this.idpOrigin_ = null;
      this.initialFrameUrl_ = null;
      this.reloadUrl_ = null;
      this.trusted_ = true;
      this.readyFired_ = false;
      this.authCompletedFired_ = false;
      /**
       * @private {WebView|undefined}
       */
      this.webview_ = typeof webview == 'string' ?
          /** @type {WebView} */ ($(webview)) :
          webview;
      assert(this.webview_);
      this.enableGaiaActionButtons_ = false;
      this.webviewEventManager_ = WebviewEventManager.create();

      this.clientId_ = null;

      this.confirmPasswordCallback = null;
      this.noPasswordCallback = null;
      this.onePasswordCallback = null;
      this.insecureContentBlockedCallback = null;
      this.samlApiUsedCallback = null;
      this.recordSAMLProviderCallback = null;
      this.missingGaiaInfoCallback = null;
      /**
       * Callback allowing to request whether the specified user which
       * authenticates via SAML is a user without a password (neither a manually
       * entered one nor one provided via Credentials Passing API).
       * @type {?function(string, string, function(boolean))} Arguments are the
       * e-mail, the GAIA ID, and the response callback.
       */
      this.getIsSamlUserPasswordlessCallback = null;
      this.needPassword = true;
      this.services_ = null;
      /**
       * Caches the result of |getIsSamlUserPasswordlessCallback| invocation for
       * the current user. Null if no result is obtained yet.
       * @type {?boolean}
       * @private
       */
      this.isSamlUserPasswordless_ = null;
      /** @private {boolean} */
      this.isConstrainedWindow_ = false;
      this.samlAclUrl_ = null;

      window.addEventListener(
          'message', this.onMessageFromWebview_.bind(this), false);
      window.addEventListener('focus', this.onFocus_.bind(this), false);
      window.addEventListener('popstate', this.onPopState_.bind(this), false);

      /**
       * @type {boolean}
       * @private
       */
      this.isDomLoaded_ = document.readyState != 'loading';
      if (this.isDomLoaded_) {
        this.initializeAfterDomLoaded_();
      } else {
        document.addEventListener(
            'DOMContentLoaded', this.initializeAfterDomLoaded_.bind(this));
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
      this.chooseWhatToSync_ = false;
      this.skipForNow_ = false;
      this.sessionIndex_ = null;
      this.trusted_ = true;
      this.authFlow = AuthFlow.DEFAULT;
      this.samlHandler_.reset();
      this.videoEnabled = false;
      this.services_ = null;
      this.isSamlUserPasswordless_ = null;
    }

    /**
     * Resets the webview to the blank page.
     */
    resetWebview() {
      if (this.webview_.src && this.webview_.src != BLANK_PAGE_URL) {
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
          new cr.login.SamlHandler(this.webview_, false /* startsOnSamlPage */);
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'insecureContentBlocked',
          this.onInsecureContentBlocked_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'authPageLoaded',
          this.onAuthPageLoaded_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'videoEnabled', this.onVideoEnabled_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'apiPasswordAdded',
          this.onSamlApiPasswordAdded_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.samlHandler_, 'challengeMachineKeyRequired',
          this.onChallengeMachineKeyRequired_.bind(this));

      this.webviewEventManager_.addEventListener(
          this.webview_, 'droplink', this.onDropLink_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.webview_, 'newwindow', this.onNewWindow_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.webview_, 'contentload', this.onContentLoad_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.webview_, 'loadabort', this.onLoadAbort_.bind(this));
      this.webviewEventManager_.addEventListener(
          this.webview_, 'loadcommit', this.onLoadCommit_.bind(this));

      this.webviewEventManager_.addWebRequestEventListener(
          this.webview_.request.onCompleted,
          this.onRequestCompleted_.bind(this),
          {urls: ['<all_urls>'], types: ['main_frame']}, ['responseHeaders']);
      this.webviewEventManager_.addWebRequestEventListener(
          this.webview_.request.onHeadersReceived,
          this.onHeadersReceived_.bind(this),
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
     * @private
     */
    setWebviewPartition(newWebviewPartitionName) {
      if (!this.webview_.src) {
        // We have not navigated anywhere yet. Note that a webview's src
        // attribute does not allow a change back to "".
        this.webview_.partition = newWebviewPartitionName;
      } else if (this.webview_.partition != newWebviewPartitionName) {
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
      // gaiaUrl parameter is used for testing. Once defined, it is never
      // changed.
      this.idpOrigin_ = data.gaiaUrl || IDP_ORIGIN;
      this.isConstrainedWindow_ = data.constrained == '1';
      this.clientId_ = data.clientId;
      this.dontResizeNonEmbeddedPages = data.dontResizeNonEmbeddedPages;
      this.enableGaiaActionButtons_ = data.enableGaiaActionButtons;

      this.initialFrameUrl_ = this.constructInitialFrameUrl_(data);
      this.reloadUrl_ = data.frameUrl || this.initialFrameUrl_;
      this.samlAclUrl_ = data.samlAclUrl;
      // The email field is repurposed as public session email in SAML guest
      // mode, ie when frameUrl is not empty.
      if (data.samlAclUrl) {
        this.email_ = data.email;
      }

      if (data.startsOnSamlPage) {
        this.samlHandler_.startsOnSamlPage = true;
      }
      // Don't block insecure content for desktop flow because it lands on
      // http. Otherwise, block insecure content as long as gaia is https.
      this.samlHandler_.blockInsecureContent = authMode != AuthMode.DESKTOP &&
          this.idpOrigin_.startsWith('https://');
      this.samlHandler_.extractSamlPasswordAttributes =
          data.extractSamlPasswordAttributes;

      this.needPassword = !('needPassword' in data) || data.needPassword;

      this.webview_.contextMenus.onShow.addListener(function(e) {
        e.preventDefault();
      });

      this.webview_.src = this.reloadUrl_;
      this.isLoaded_ = true;
    }

    constructChromeOSAPIUrl_() {
      return this.idpOrigin_ + EMBEDDED_SETUP_CHROMEOS_ENDPOINT_V2;
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

    constructInitialFrameUrl_(data) {
      if (data.doSamlRedirect) {
        let url = this.idpOrigin_ + SAML_REDIRECTION_PATH;
        url = appendParam(url, 'domain', data.enterpriseEnrollmentDomain);
        url = appendParam(
            url, 'continue',
            data.gaiaUrl + 'programmatic_auth_chromeos?hl=' + data.hl +
                '&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin&' +
                'client_id=' + encodeURIComponent(data.clientId) +
                '&access_type=offline');

        return url;
      }

      let url;
      if (data.gaiaPath) {
        url = this.idpOrigin_ + data.gaiaPath;
      } else {
        url = this.constructChromeOSAPIUrl_();
      }

      if (data.chromeType) {
        url = appendParam(url, 'chrometype', data.chromeType);
      }
      if (data.clientId) {
        url = appendParam(url, 'client_id', data.clientId);
      }
      if (data.enterpriseDisplayDomain) {
        url = appendParam(url, 'manageddomain', data.enterpriseDisplayDomain);
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
        if (this.idpOrigin_ && currentUrl.lastIndexOf(this.idpOrigin_) == 0) {
          const headers = details.responseHeaders;
          for (let i = 0; headers && i < headers.length; ++i) {
            if (headers[i].name.toLowerCase() == EMBEDDED_FORM_HEADER) {
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
      if (history.state && history.state.url != url) {
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
      if (this.authMode == AuthMode.DESKTOP &&
          document.activeElement == document.body) {
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
     * 1) reads the authenticated user info from a signin header,
     * 2) signals the start of a saml flow upon receiving a saml header.
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
      if (currentUrl.lastIndexOf(this.idpOrigin_, 0) != 0) {
        return;
      }

      const headers = details.responseHeaders;
      for (let i = 0; headers && i < headers.length; ++i) {
        const header = headers[i];
        const headerName = header.name.toLowerCase();
        if (headerName == SIGN_IN_HEADER) {
          const headerValues = header.value.toLowerCase().split(',');
          const signinDetails = {};
          headerValues.forEach(function(e) {
            const pair = e.split('=');
            signinDetails[pair[0].trim()] = pair[1].trim();
          });
          // Removes "" around.
          this.email_ = signinDetails['email'].slice(1, -1);
          this.gaiaId_ = signinDetails['obfuscatedid'].slice(1, -1);
          this.sessionIndex_ = signinDetails['sessionindex'];
          this.isSamlUserPasswordless_ = null;
        } else if (headerName == LOCATION_HEADER) {
          // If the "choose what to sync" checkbox was clicked, then the
          // continue URL will contain a source=3 field.
          assert(header.value !== undefined);
          const location = decodeURIComponent(header.value);
          this.chooseWhatToSync_ = !!location.match(/(\?|&)source=3($|&)/);
        }
      }
    }

    /**
     * Returns true if given HTML5 message is received from the webview element.
     * @param {Object} e Payload of the received HTML5 message.
     */
    isGaiaMessage(e) {
      if (!this.isWebviewEvent_(e)) {
        return false;
      }

      // The event origin does not have a trailing slash.
      if (e.origin !=
          this.idpOrigin_.substring(0, this.idpOrigin_.length - 1)) {
        return false;
      }

      // Gaia messages must be an object with 'method' property.
      if (typeof e.data != 'object' || !e.data.hasOwnProperty('method')) {
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
      if (!this.isGaiaMessage(e)) {
        return;
      }

      const msg = e.data;
      if (msg.method in messageHandlers) {
        if (this.authCompletedFired_) {
          console.error(msg.method + ' message sent after auth completed');
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
     * @param {Object=} messageData Data to be attached to the message.
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
        // Invoke confirm password callback asynchronously because the
        // verification was based on messages and caller (GaiaSigninScreen)
        // does not expect it to be called immediately.
        // TODO(xiyuan): Change to synchronous call when iframe based code
        // is removed.
        const invokeConfirmPassword =
            (function() {
              this.confirmPasswordCallback(
                  this.email_, this.samlHandler_.scrapedPasswordCount);
            }).bind(this);
        window.setTimeout(invokeConfirmPassword, 0);
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
      // TODO(https://crbug.com/837107): remove this once API is fully
      // stabilized.
      // @example.com is used in tests.
      if (!this.services_ && !this.email_.endsWith('@gmail.com') &&
          !this.email_.endsWith('@example.com')) {
        console.warn('Forcing empty services.');
        this.services_ = [];
      }
      if (!this.services_) {
        return;
      }

      if (this.isSamlUserPasswordless_ === null &&
          this.authFlow == AuthFlow.SAML && this.email_ && this.gaiaId_ &&
          this.getIsSamlUserPasswordlessCallback) {
        // Start a request to obtain the |isSamlUserPasswordless_| value for
        // the current user. Once the response arrives, maybeCompleteAuth_()
        // will be called again.
        this.getIsSamlUserPasswordlessCallback(
            this.email_, this.gaiaId_,
            this.onGotIsSamlUserPasswordless_.bind(
                this, this.email_, this.gaiaId_));
        return;
      }

      if (this.recordSAMLProviderCallback && this.authFlow == AuthFlow.SAML) {
        // Makes distinction between different SAML providers
        this.recordSAMLProviderCallback(
            this.samlHandler_.x509certificate || '');
      }

      if (this.isSamlUserPasswordless_ && this.authFlow == AuthFlow.SAML &&
          this.email_ && this.gaiaId_) {
        // No password needed for this user, so complete immediately.
        this.onAuthCompleted_();
        return;
      }

      if (this.samlHandler_.samlApiUsed) {
        if (this.samlApiUsedCallback) {
          // Makes distinction between Gaia and Chrome Credentials Passing API
          // login to properly fill ChromeOS.SAML.ApiLogin metrics.
          this.samlApiUsedCallback(this.authFlow == AuthFlow.SAML);
        }
        this.password_ = this.samlHandler_.apiPasswordBytes;
        this.onAuthCompleted_();
        return;
      }

      if (this.samlHandler_.scrapedPasswordCount == 0) {
        if (this.noPasswordCallback) {
          this.noPasswordCallback(this.email_);
          return;
        }

        // Fall through to finish the auth flow even if this.needPassword
        // is true. This is because the flag is used as an intention to get
        // password when it is available but not a mandatory requirement.
        console.warn('Authenticator: No password scraped for SAML.');
      } else if (this.needPassword) {
        if (this.samlHandler_.scrapedPasswordCount == 1) {
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
     * Invoked when the result of |getIsSamlUserPasswordlessCallback| arrives.
     * @param {string} email
     * @param {string} gaiaId
     * @param {boolean} isSamlUserPasswordless
     * @private
     */
    onGotIsSamlUserPasswordless_(email, gaiaId, isSamlUserPasswordless) {
      // Compare the request's user identifier with the currently set one, in
      // order to ignore responses to old requests.
      if (this.email_ && this.email_ == email && this.gaiaId_ &&
          this.gaiaId_ == gaiaId) {
        this.isSamlUserPasswordless_ = isSamlUserPasswordless;
        this.maybeCompleteAuth_();
      }
    }

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
          typeof dict == 'object', 'FATAL: Bad %s type: %s', nameOfDict,
          typeof dict);
      for (const key in dict) {
        this.assertStringElement_(dict[key], nameOfDict, key);
      }
    }

    /** Asserts an element |elem| in a certain collection is a string. */
    assertStringElement_(elem, nameOfCollection, index) {
      console.assert(
          typeof elem == 'string', 'FATAL: Bad %s[%s] type: %s',
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
      // Chrome will crash on incorrect data type, so log some error message
      // here.
      if (this.services_) {
        this.assertStringArray_(this.services_, 'services');
      }
      if (this.isSamlUserPasswordless_ && this.authFlow == AuthFlow.SAML &&
          this.email_) {
        // In the passwordless case, the user data will be protected by non
        // password based mechanisms. Clear anything that got collected into
        // |password_|, if any.
        this.password_ = '';
      }
      let passwordAttributes = {};
      if (this.authFlow == AuthFlow.SAML &&
          this.samlHandler_.extractSamlPasswordAttributes &&
          !this.isSamlUserPasswordless_) {
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
              usingSAML: this.authFlow == AuthFlow.SAML,
              publicSAML: this.samlAclUrl_ || false,
              chooseWhatToSync: this.chooseWhatToSync_,
              skipForNow: this.skipForNow_,
              sessionIndex: this.sessionIndex_ || '',
              trusted: this.trusted_,
              services: this.services_ || [],
              passwordAttributes: passwordAttributes
            }
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
     * Invoked when |samlHandler_| fires 'videoEnabled' event.
     * @private
     */
    onVideoEnabled_(e) {
      this.videoEnabled = true;
    }

    /**
     * Invoked when |samlHandler_| fires 'apiPasswordAdded' event.
     * @private
     */
    onSamlApiPasswordAdded_(e) {
      // Saml API 'add' password might be received after the 'loadcommit'
      // event. In such case, maybeCompleteAuth_ should be attempted again if
      // GAIA ID is available.
      if (this.gaiaId_) {
        this.maybeCompleteAuth_();
      }
    }

    /**
     * Invoked when |samlHandler_| fires 'challengeMachineKeyRequired' event.
     * @private
     */
    onChallengeMachineKeyRequired_(e) {
      cr.sendWithPromise(
            'samlChallengeMachineKey', e.detail.url, e.detail.challenge)
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
      if (currentUrl.lastIndexOf(this.idpOrigin_) == 0) {
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

        if (this.authMode == AuthMode.DEFAULT) {
          chrome.send('metricsHandler:recordBooleanHistogram', [
            'ChromeOS.GAIA.AuthenticatorContentWindowNull',
            !this.webview_.contentWindow
          ]);
        }

        this.fireReadyEvent_();
        // Focus webview after dispatching event when webview is already
        // visible.
        this.webview_.focus();
      } else if (currentUrl == BLANK_PAGE_URL) {
        this.fireReadyEvent_();
      } else if (currentUrl == this.samlAclUrl_) {
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
      // Note: <webview> prints error message to console if |contentWindow| is
      // not defined.
      // TODO(dzhioev): remove the message. http://crbug.com/469522
      const webviewWindow = this.webview_.contentWindow;
      return !!webviewWindow && webviewWindow === e.source;
    }
  }

  // #cr_define_end
  /**
   * The current auth flow of the hosted auth page.
   * @type {AuthFlow}
   */
  cr.defineProperty(Authenticator, 'authFlow');

  /**
   * The domain name of the current auth page.
   * @type {string}
   */
  cr.defineProperty(Authenticator, 'authDomain');

  /**
   * True if the page has requested media access.
   * @type {boolean}
   */
  cr.defineProperty(Authenticator, 'videoEnabled');

  Authenticator.AuthFlow = AuthFlow;
  Authenticator.AuthMode = AuthMode;
  Authenticator.SUPPORTED_PARAMS = SUPPORTED_PARAMS;

  return {Authenticator: Authenticator};
});
