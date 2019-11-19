// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType} from './cloud_print_interface.js';
import {parseCloudDestination, parseInvitation} from './data/cloud_parsers.js';
import {CloudOrigins, DestinationOrigin} from './data/destination.js';
import {Invitation} from './data/invitation.js';
import {NativeLayer} from './native_layer.js';


/** @implements {CloudPrintInterface} */
export class CloudPrintInterfaceJS {
  /**
   * API to the Google Cloud Print service.
   * @param {string} baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   * @param {!NativeLayer} nativeLayer Native layer used to get
   *     Auth2 tokens.
   * @param {boolean} isInAppKioskMode Whether the print preview is in App
   *     Kiosk mode.
   * @param {string} uiLocale The UI locale.
   */
  constructor(baseUrl, nativeLayer, isInAppKioskMode, uiLocale) {
    /**
     * The base URL of the Google Cloud Print API.
     * @private {string}
     */
    this.baseUrl_ = baseUrl;

    /**
     * Used to get Auth2 tokens.
     * @private {!NativeLayer}
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Whether Print Preview is in App Kiosk mode; use only printers available
     * for the device and disable cookie destinations.
     * @private {boolean}
     */
    this.isInAppKioskMode_ = isInAppKioskMode;

    /**
     * The UI locale, used to get printer information in the correct locale
     * from Google Cloud Print.
     * @private {string}
     */
    this.uiLocale_ = uiLocale;

    /**
     * Currently logged in users (identified by email) mapped to the Google
     * session index.
     * @private {!Object<number>}
     */
    this.userSessionIndex_ = {};

    /**
     * Stores last received XSRF tokens for each user account. Sent as
     * a parameter with every request.
     * @private {!Object<string>}
     */
    this.xsrfTokens_ = {};

    /**
     * Outstanding cloud destination search requests.
     * @private {!Array<!CloudPrintRequest>}
     */
    this.outstandingCloudSearchRequests_ = [];

    // <if expr="chromeos">
    /**
     * Promise that will be resolved when the access token for
     * DestinationOrigin.DEVICE is available. Null if there is no request
     * currently pending.
     * @private {?Promise<string>}
     */
    this.accessTokenRequestPromise_ = null;
    // </if>

    /** @private {!EventTarget} */
    this.eventTarget_ = new EventTarget();
  }

  /** @override */
  areCookieDestinationsDisabled() {
    return this.isInAppKioskMode_;
  }

  /** @override */
  getEventTarget() {
    return this.eventTarget_;
  }

  /** @override */
  isCloudDestinationSearchInProgress() {
    return this.outstandingCloudSearchRequests_.length > 0;
  }

  /** @override */
  search(opt_account, opt_origin) {
    const account = opt_account || '';
    let origins = opt_origin ? [opt_origin] : CloudOrigins;
    if (this.isInAppKioskMode_) {
      origins = origins.filter(function(origin) {
        return origin != DestinationOrigin.COOKIES;
      });
    }
    this.abortSearchRequests_(origins);
    this.search_(true, account, origins);
    this.search_(false, account, origins);
  }

  /**
   * Sends Google Cloud Print search API requests.
   * @param {boolean} isRecent Whether to search for only recently used
   *     printers.
   * @param {string} account Account the search is sent for. It matters for
   *     COOKIES origin only, and can be empty (sent on behalf of the primary
   *     user in this case).
   * @param {!Array<!DestinationOrigin>} origins Origins to
   *     search printers for.
   * @private
   */
  search_(isRecent, account, origins) {
    const params = [
      new HttpParam('connection_status', 'ALL'),
      new HttpParam('client', 'chrome'), new HttpParam('use_cdd', 'true')
    ];
    if (isRecent) {
      params.push(new HttpParam('q', '^recent'));
    }
    origins.forEach(function(origin) {
      const cpRequest = this.buildRequest_(
          'GET', 'search', params, origin, account,
          this.onSearchDone_.bind(this, isRecent));
      this.outstandingCloudSearchRequests_.push(cpRequest);
      this.sendOrQueueRequest_(cpRequest);
    }, this);
  }

  /** @override */
  invites(account) {
    const params = [
      new HttpParam('client', 'chrome'),
    ];
    this.sendOrQueueRequest_(this.buildRequest_(
        'GET', 'invites', params, DestinationOrigin.COOKIES, account,
        this.onInvitesDone_.bind(this)));
  }

  /** @override */
  processInvite(invitation, accept) {
    const params = [
      new HttpParam('printerid', invitation.destination.id),
      new HttpParam('email', invitation.scopeId),
      new HttpParam('accept', accept ? 'true' : 'false'),
      new HttpParam('use_cdd', 'true'),
    ];
    this.sendOrQueueRequest_(this.buildRequest_(
        'POST', 'processinvite', params, invitation.destination.origin,
        invitation.destination.account,
        this.onProcessInviteDone_.bind(this, invitation, accept)));
  }

  /** @override */
  submit(destination, printTicket, documentTitle, data) {
    const result = VERSION_REGEXP_.exec(navigator.userAgent);
    let chromeVersion = 'unknown';
    if (result && result.length == 2) {
      chromeVersion = result[1];
    }
    const params = [
      new HttpParam('printerid', destination.id),
      new HttpParam('contentType', 'dataUrl'),
      new HttpParam('title', documentTitle),
      new HttpParam('ticket', printTicket),
      new HttpParam('content', 'data:application/pdf;base64,' + data),
      new HttpParam('tag', '__google__chrome_version=' + chromeVersion),
      new HttpParam('tag', '__google__os=' + navigator.platform)
    ];
    const cpRequest = this.buildRequest_(
        'POST', 'submit', params, destination.origin, destination.account,
        this.onSubmitDone_.bind(this));
    this.sendOrQueueRequest_(cpRequest);
  }

  /** @override */
  printer(printerId, origin, account) {
    const params = [
      new HttpParam('printerid', printerId), new HttpParam('use_cdd', 'true'),
      new HttpParam('printer_connection_status', 'true')
    ];
    this.sendOrQueueRequest_(this.buildRequest_(
        'GET', 'printer', params, origin, account || '',
        this.onPrinterDone_.bind(this, printerId)));
  }

  /**
   * Builds request to the Google Cloud Print API.
   * @param {string} method HTTP method of the request.
   * @param {string} action Google Cloud Print action to perform.
   * @param {Array<!HttpParam>} params HTTP parameters to include in the
   *     request.
   * @param {!DestinationOrigin} origin Origin for destination.
   * @param {?string} account Account the request is sent for. Can be
   *     {@code null} or empty string if the request is not cookie bound or
   *     is sent on behalf of the primary user.
   * @param {function(!CloudPrintRequest)} callback Callback to
   *     invoke when request completes.
   * @return {!CloudPrintRequest} Partially prepared request.
   * @private
   */
  buildRequest_(method, action, params, origin, account, callback) {
    const url = new URL(this.baseUrl_ + '/' + action);
    const searchParams = url.searchParams;
    if (origin == DestinationOrigin.COOKIES) {
      const xsrfToken = this.xsrfTokens_[account];
      if (!xsrfToken) {
        searchParams.append('xsrf', '');
        // TODO(rltoscano): Should throw an error if not a read-only action or
        // issue an xsrf token request.
      } else {
        searchParams.append('xsrf', xsrfToken);
      }
      if (account) {
        const index = this.userSessionIndex_[account] || 0;
        if (index > 0) {
          searchParams.append('authuser', index.toString());
        }
      }
    } else {
      searchParams.append('xsrf', '');
    }

    // Add locale
    searchParams.append('hl', this.uiLocale_);
    let body = null;
    if (params) {
      if (method == 'GET') {
        params.forEach(param => {
          searchParams.append(param.name, encodeURIComponent(param.value));
        });
      } else if (method == 'POST') {
        body = params.reduce(function(partialBody, param) {
          return partialBody + 'Content-Disposition: form-data; name=\"' +
              param.name + '\"\r\n\r\n' + param.value + '\r\n--' +
              MULTIPART_BOUNDARY_ + '\r\n';
        }, '--' + MULTIPART_BOUNDARY_ + '\r\n');
      }
    }

    const headers = {};
    headers['X-CloudPrint-Proxy'] = 'ChromePrintPreview';
    if (method == 'GET') {
      headers['Content-Type'] = URL_ENCODED_CONTENT_TYPE_;
    } else if (method == 'POST') {
      headers['Content-Type'] = MULTIPART_CONTENT_TYPE_;
    }

    const xhr = new XMLHttpRequest();
    xhr.open(method, url.toString(), true);
    xhr.withCredentials = (origin == DestinationOrigin.COOKIES);
    for (const header in headers) {
      xhr.setRequestHeader(header, headers[header]);
    }

    return new CloudPrintRequest(xhr, body, origin, account, callback);
  }

  /**
   * Sends a request to the Google Cloud Print API or queues if it needs to
   *     wait OAuth2 access token.
   * @param {!CloudPrintRequest} request Request to send or queue.
   * @private
   */
  sendOrQueueRequest_(request) {
    if (request.origin == DestinationOrigin.COOKIES) {
      this.sendRequest_(request);
      return;
    }

    // <if expr="chromeos">
    assert(request.origin == DestinationOrigin.DEVICE);
    if (this.accessTokenRequestPromise_ == null) {
      this.accessTokenRequestPromise_ = this.nativeLayer_.getAccessToken();
    }

    this.accessTokenRequestPromise_.then(
        this.onAccessTokenReady_.bind(this, request));
    // </if>
  }

  /**
   * Sends a request to the Google Cloud Print API.
   * @param {!CloudPrintRequest} request Request to send.
   * @private
   */
  sendRequest_(request) {
    request.xhr.onreadystatechange =
        this.onReadyStateChange_.bind(this, request);
    request.xhr.send(request.body);
  }

  /**
   * Creates an object containing information about the error based on the
   * request.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @return {!CloudPrintInterfaceErrorEventDetail} Information
   *     about the error.
   * @private
   */
  createErrorEventDetail_(request) {
    const status200 = request.xhr.status === 200;
    return {
      status: request.xhr.status,
      errorCode: status200 ? request.result['errorCode'] : 0,
      message: status200 ? request.result['message'] : '',
      origin: request.origin,
    };
  }

  /**
   * Fires an event with information about the new active user and logged in
   * users.
   * @param {string} activeUser The active user account.
   * @param {Array<string>=} users The currently logged in users. Omitted
   *     if the list of users has not changed.
   * @private
   */
  dispatchUserUpdateEvent_(activeUser, users) {
    this.eventTarget_.dispatchEvent(new CustomEvent(
        CloudPrintInterfaceEventType.UPDATE_USERS,
        {detail: {activeUser: activeUser, users: users}}));
  }

  /**
   * Updates user info and session index from the {@code request} response.
   * @param {!CloudPrintRequest} request Request to extract user
   *     info from.
   * @private
   */
  setUsers_(request) {
    if (request.origin == DestinationOrigin.COOKIES) {
      const users = request.result['request']['users'] || [];
      this.setUsers(users);
    }
  }

  /** @param {!Array<string>} users */
  setUsers(users) {
    this.userSessionIndex_ = {};
    for (let i = 0; i < users.length; i++) {
      this.userSessionIndex_[users[i]] = i;
    }
  }

  /**
   * Terminates search requests for requested {@code origins}.
   * @param {!Array<DestinationOrigin>} origins Origins
   *     to terminate search requests for.
   * @private
   */
  abortSearchRequests_(origins) {
    this.outstandingCloudSearchRequests_ =
        this.outstandingCloudSearchRequests_.filter(function(request) {
          if (origins.indexOf(request.origin) >= 0) {
            request.xhr.abort();
            return false;
          }
          return true;
        });
  }

  // <if expr="chromeos">
  /**
   * Called when a native layer receives access token. Assumes that the
   * destination type for this token is DestinationOrigin.DEVICE.
   * @param {CloudPrintRequest} request The pending request that
   *     requires the access token.
   * @param {string} accessToken The access token obtained.
   * @private
   */
  onAccessTokenReady_(request, accessToken) {
    assert(request.origin == DestinationOrigin.DEVICE);
    if (accessToken) {
      request.xhr.setRequestHeader('Authorization', 'Bearer ' + accessToken);
      this.sendRequest_(request);
    } else {  // No valid token.
      // Without abort status does not exist.
      request.xhr.abort();
      request.callback(request);
    }
    this.accessTokenRequestPromise_ = null;
  }
  // </if>

  /**
   * Called when the ready-state of a XML http request changes.
   * Calls the successCallback with the result or dispatches an ERROR event.
   * @param {!CloudPrintRequest} request Request that was changed.
   * @private
   */
  onReadyStateChange_(request) {
    if (request.xhr.readyState == 4) {
      if (request.xhr.status == 200) {
        request.result =
            /** @type {Object} */ (JSON.parse(request.xhr.responseText));
        if (request.origin == DestinationOrigin.COOKIES &&
            request.result['success']) {
          this.xsrfTokens_[request.result['request']['user']] =
              request.result['xsrf_token'];
        }
      }
      request.callback(request);
    }
  }

  /**
   * Called when the search request completes.
   * @param {boolean} isRecent Whether the search request was for recent
   *     destinations.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @private
   */
  onSearchDone_(isRecent, request) {
    let lastRequestForThisOrigin = true;
    this.outstandingCloudSearchRequests_ =
        this.outstandingCloudSearchRequests_.filter(function(item) {
          if (item != request && item.origin == request.origin) {
            lastRequestForThisOrigin = false;
          }
          return item != request;
        });
    let activeUser = '';
    if (request.origin == DestinationOrigin.COOKIES) {
      activeUser = request.result && request.result['request'] &&
          request.result['request']['user'];
    }
    if (request.xhr.status == 200 && request.result['success']) {
      // Extract printers.
      const printerListJson = request.result['printers'] || [];
      const printerList = [];
      printerListJson.forEach(function(printerJson) {
        try {
          printerList.push(
              parseCloudDestination(printerJson, request.origin, activeUser));
        } catch (err) {
          console.error('Unable to parse cloud print destination: ' + err);
        }
      });
      // Extract and store users.
      this.setUsers_(request);
      this.dispatchUserUpdateEvent_(
          activeUser, request.result['request']['users']);
      // Dispatch SEARCH_DONE event.
      this.eventTarget_.dispatchEvent(
          new CustomEvent(CloudPrintInterfaceEventType.SEARCH_DONE, {
            detail: {
              origin: request.origin,
              printers: printerList,
              isRecent: isRecent,
              user: activeUser,
              searchDone: lastRequestForThisOrigin,
            }
          }));
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request);
      errorEventDetail.user = activeUser;
      errorEventDetail.searchDone = lastRequestForThisOrigin;
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SEARCH_FAILED,
          {detail: errorEventDetail}));
    }
  }

  /**
   * Called when invitations search request completes.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @private
   */
  onInvitesDone_(request) {
    const activeUser = (request.result && request.result['request'] &&
                        request.result['request']['user']) ||
        '';
    if (request.xhr.status == 200 && request.result['success']) {
      // Extract invitations.
      const invitationListJson = request.result['invites'] || [];
      const invitationList = [];
      invitationListJson.forEach(function(invitationJson) {
        try {
          invitationList.push(parseInvitation(invitationJson, activeUser));
        } catch (e) {
          console.error('Unable to parse invitation: ' + e);
        }
      });
      // Dispatch INVITES_DONE event.
      this.eventTarget_.dispatchEvent(
          new CustomEvent(CloudPrintInterfaceEventType.INVITES_DONE, {
            detail: {
              invitations: invitationList,
              user: activeUser,
            }
          }));
    } else {
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.INVITES_FAILED, {detail: activeUser}));
    }
  }

  /**
   * Called when invitation processing request completes.
   * @param {!Invitation} invitation Processed invitation.
   * @param {boolean} accept Whether this invitation was accepted or rejected.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @private
   */
  onProcessInviteDone_(invitation, accept, request) {
    const activeUser = (request.result && request.result['request'] &&
                        request.result['request']['user']) ||
        '';
    let printer = null;
    if (request.xhr.status == 200 && request.result['success'] && accept) {
      try {
        printer = parseCloudDestination(
            request.result['printer'], request.origin, activeUser);
      } catch (e) {
        console.error('Failed to parse cloud print destination: ' + e);
      }
    }
    this.eventTarget_.dispatchEvent(
        new CustomEvent(CloudPrintInterfaceEventType.PROCESS_INVITE_DONE, {
          detail: {
            printer: printer,
            invitation: invitation,
            accept: accept,
            user: activeUser,
          }
        }));
  }

  /**
   * Called when the submit request completes.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @private
   */
  onSubmitDone_(request) {
    if (request.xhr.status == 200 && request.result['success']) {
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SUBMIT_DONE,
          {detail: request.result['job']['id']}));
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request);
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SUBMIT_FAILED,
          {detail: errorEventDetail}));
    }
  }

  /**
   * Called when the printer request completes.
   * @param {string} destinationId ID of the destination that was looked up.
   * @param {!CloudPrintRequest} request Request that has been
   *     completed.
   * @private
   */
  onPrinterDone_(destinationId, request) {
    // Special handling of the first printer request. It does not matter at
    // this point, whether printer was found or not.
    if (request.origin == DestinationOrigin.COOKIES && request.result &&
        request.result['request']['user'] &&
        request.result['request']['users']) {
      const users = request.result['request']['users'];
      this.setUsers_(request);
      // In case the user account is known, but not the primary one,
      // activate it.
      if (request.account != request.result['request']['user'] &&
          this.userSessionIndex_[request.account] > 0 && request.account) {
        this.dispatchUserUpdateEvent_(request.account, users);
        // Repeat the request for the newly activated account.
        this.printer(
            request.result['request']['params']['printerid'], request.origin,
            request.account);
        // Stop processing this request, wait for the new response.
        return;
      }
      this.dispatchUserUpdateEvent_(request.result['request']['user'], users);
    }
    // Process response.
    if (request.xhr.status == 200 && request.result['success']) {
      let activeUser = '';
      if (request.origin == DestinationOrigin.COOKIES) {
        activeUser = request.result['request']['user'];
      }
      const printerJson = request.result['printers'][0];
      let printer;
      try {
        printer =
            parseCloudDestination(printerJson, request.origin, activeUser);
      } catch (err) {
        console.error(
            'Failed to parse cloud print destination: ' +
            JSON.stringify(printerJson));
        return;
      }
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.PRINTER_DONE, {detail: printer}));
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request);
      errorEventDetail.destinationId = destinationId;
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.PRINTER_FAILED,
          {detail: errorEventDetail}));
    }
  }
}

/**
 * Content type header value for a URL encoded HTTP request.
 * @const {string}
 * @private
 */
const URL_ENCODED_CONTENT_TYPE_ = 'application/x-www-form-urlencoded';

/**
 * Multi-part POST request boundary used in communication with Google
 * Cloud Print.
 * @const {string}
 * @private
 */
const MULTIPART_BOUNDARY_ = '----CloudPrintFormBoundaryjc9wuprokl8i';

/**
 * Content type header value for a multipart HTTP request.
 * @const {string}
 * @private
 */
const MULTIPART_CONTENT_TYPE_ =
    'multipart/form-data; boundary=' + MULTIPART_BOUNDARY_;

/**
 * Regex that extracts Chrome's version from the user-agent string.
 * @const {!RegExp}
 * @private
 */
const VERSION_REGEXP_ = /.*Chrome\/([\d\.]+)/i;

export class CloudPrintRequest {
  /**
   * Data structure that holds data for Cloud Print requests.
   * @param {!XMLHttpRequest} xhr Partially prepared http request.
   * @param {string} body Data to send with POST requests.
   * @param {!DestinationOrigin} origin Origin for destination.
   * @param {?string} account Account the request is sent for. Can be
   *     {@code null} or empty string if the request is not cookie bound or
   *     is sent on behalf of the primary user.
   * @param {function(!CloudPrintRequest)} callback Callback to
   *     invoke when request completes.
   */
  constructor(xhr, body, origin, account, callback) {
    /**
     * Partially prepared http request.
     * @type {!XMLHttpRequest}
     */
    this.xhr = xhr;

    /**
     * Data to send with POST requests.
     * @type {string}
     */
    this.body = body;

    /**
     * Origin for destination.
     * @type {!DestinationOrigin}
     */
    this.origin = origin;

    /**
     * User account this request is expected to be executed for.
     * @type {?string}
     */
    this.account = account;

    /**
     * Callback to invoke when request completes.
     * @type {function(!CloudPrintRequest)}
     */
    this.callback = callback;

    /**
     * Result for requests.
     * @type {Object} JSON response.
     */
    this.result = null;
  }
}

class HttpParam {
  /**
   * Data structure that represents an HTTP parameter.
   * @param {string} name Name of the parameter.
   * @param {string} value Value of the parameter.
   */
  constructor(name, value) {
    /**
     * Name of the parameter.
     * @type {string}
     */
    this.name = name;

    /**
     * Name of the value.
     * @type {string}
     */
    this.value = value;
  }
}
