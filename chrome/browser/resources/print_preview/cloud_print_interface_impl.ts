// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType, CloudPrintInterfacePrinterFailedDetail, CloudPrintInterfaceSearchDoneDetail} from './cloud_print_interface.js';
import {CloudDestinationInfo, parseCloudDestination} from './data/cloud_parsers.js';
import {CloudOrigins, Destination, DestinationOrigin} from './data/destination.js';
import {PrinterType} from './data/destination_match.js';
import {MetricsContext, PrintPreviewInitializationEvents} from './metrics.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {NativeLayerCrosImpl} from './native_layer_cros.js';

// </if>


export class CloudPrintInterfaceImpl implements CloudPrintInterface {
  /**
   * The base URL of the Google Cloud Print API.
   */
  private baseUrl_: string = '';

  /**
   * Whether Print Preview is in App Kiosk mode; use only printers available
   * for the device and disable cookie destinations.
   */
  private isInAppKioskMode_: boolean = false;

  /**
   * The UI locale, used to get printer information in the correct locale
   * from Google Cloud Print.
   */
  private uiLocale_: string = '';

  /**
   * Currently logged in users (identified by email) mapped to the Google
   * session index.
   */
  private userSessionIndex_: {[account: string]: number} = {};

  /**
   * Stores last received XSRF tokens for each user account. Sent as
   * a parameter with every request.
   */
  private xsrfTokens_: {[account: string]: string} = {};

  /**
   * Outstanding cloud destination search requests.
   */
  private outstandingCloudSearchRequests_: CloudPrintRequest[] = [];

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * Promise that will be resolved when the access token for
   * DestinationOrigin.DEVICE is available. Null if there is no request
   * currently pending.
   */
  private accessTokenRequestPromise_: Promise<string>|null = null;
  // </if>

  private eventTarget_: EventTarget = new EventTarget();

  configure(baseUrl: string, isInAppKioskMode: boolean, uiLocale: string) {
    this.baseUrl_ = baseUrl;
    this.isInAppKioskMode_ = isInAppKioskMode;
    this.uiLocale_ = uiLocale;
  }

  isConfigured(): boolean {
    return this.baseUrl_ !== '';
  }

  areCookieDestinationsDisabled(): boolean {
    return this.isInAppKioskMode_;
  }

  getEventTarget(): EventTarget {
    return this.eventTarget_;
  }

  isCloudDestinationSearchInProgress(): boolean {
    return this.outstandingCloudSearchRequests_.length > 0;
  }

  search(opt_account?: string|null, opt_origin?: DestinationOrigin) {
    const account = opt_account || '';
    let origins = opt_origin ? [opt_origin] : CloudOrigins;
    if (this.isInAppKioskMode_) {
      origins = origins.filter(function(origin) {
        return origin !== DestinationOrigin.COOKIES;
      });
    }
    this.abortSearchRequests_(origins);
    if (opt_account) {
      // No need to send two search requests if we don't know the account. The
      // server only sends back the XSRF token once so the other request will
      // fail.
      this.search_(true, account, origins);
    }
    this.search_(false, account, origins);
  }

  /**
   * Sends Google Cloud Print search API requests.
   * @param isRecent Whether to search for only recently used printers.
   * @param account Account the search is sent for. It matters for
   *     COOKIES origin only, and can be empty (sent on behalf of the primary
   *     user in this case).
   * @param origins Origins to search printers for.
   */
  private search_(
      isRecent: boolean, account: string, origins: DestinationOrigin[]) {
    const params = [
      new HttpParam('connection_status', 'ALL'),
      new HttpParam('client', 'chrome'), new HttpParam('use_cdd', 'true')
    ];
    if (isRecent) {
      params.push(new HttpParam('q', '^recent'));
    }
    origins.forEach((origin: DestinationOrigin) => {
      const cpRequest = this.buildRequest_(
          'GET', 'search', params, origin, account,
          (request: CloudPrintRequest) =>
              this.onSearchDone_(isRecent, request));
      this.outstandingCloudSearchRequests_.push(cpRequest);
      this.sendOrQueueRequest_(cpRequest);

      MetricsContext.getPrinters(PrinterType.CLOUD_PRINTER)
          .record(PrintPreviewInitializationEvents.FUNCTION_INITIATED);
    });
  }

  submit(
      destination: Destination, printTicket: string, documentTitle: string,
      data: string) {
    const result = VERSION_REGEXP_.exec(navigator.userAgent);
    let chromeVersion = 'unknown';
    if (result && result.length === 2) {
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
        (request: CloudPrintRequest) => this.onSubmitDone_(request));
    this.sendOrQueueRequest_(cpRequest);
  }

  printer(printerId: string, origin: DestinationOrigin, account: string) {
    const params = [
      new HttpParam('printerid', printerId), new HttpParam('use_cdd', 'true'),
      new HttpParam('printer_connection_status', 'true')
    ];
    this.sendOrQueueRequest_(this.buildRequest_(
        'GET', 'printer', params, origin, account || '',
        (request: CloudPrintRequest) =>
            this.onPrinterDone_(printerId, request)));
  }

  /**
   * Builds request to the Google Cloud Print API.
   * @param method HTTP method of the request.
   * @param action Google Cloud Print action to perform.
   * @param params HTTP parameters to include in the request.
   * @param origin Origin for destination.
   * @param account Account the request is sent for. Can be
   *     {@code null} or empty string if the request is not cookie bound or
   *     is sent on behalf of the primary user.
   * @param callback Callback to invoke when request completes.
   * @return Partially prepared request.
   */
  private buildRequest_(
      method: string, action: string, params: HttpParam[],
      origin: DestinationOrigin, account: string|null,
      callback: (request: CloudPrintRequest) => void): CloudPrintRequest {
    const url = new URL(this.baseUrl_ + '/' + action);
    const searchParams = url.searchParams;
    if (origin === DestinationOrigin.COOKIES) {
      const xsrfToken = this.xsrfTokens_[account!];
      if (!xsrfToken) {
        searchParams.append('xsrf', '');
        // TODO(rltoscano): Should throw an error if not a read-only action or
        // issue an xsrf token request.
      } else {
        searchParams.append('xsrf', xsrfToken);
      }
      if (account) {
        const index = this.userSessionIndex_[account!] || 0;
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
      if (method === 'GET') {
        params.forEach(param => {
          searchParams.append(param.name, encodeURIComponent(param.value));
        });
      } else if (method === 'POST') {
        body = params.reduce(function(partialBody, param) {
          return partialBody + 'Content-Disposition: form-data; name=\"' +
              param.name + '\"\r\n\r\n' + param.value + '\r\n--' +
              MULTIPART_BOUNDARY_ + '\r\n';
        }, '--' + MULTIPART_BOUNDARY_ + '\r\n');
      }
    }

    const headers: {[header: string]: string} = {};
    headers['X-CloudPrint-Proxy'] = 'ChromePrintPreview';
    if (method === 'GET') {
      headers['Content-Type'] = URL_ENCODED_CONTENT_TYPE_;
    } else if (method === 'POST') {
      headers['Content-Type'] = MULTIPART_CONTENT_TYPE_;
    }

    const xhr = new XMLHttpRequest();
    xhr.open(method, url.toString(), true);
    xhr.withCredentials = (origin === DestinationOrigin.COOKIES);
    for (const header in headers) {
      xhr.setRequestHeader(header, headers[header]);
    }

    return new CloudPrintRequest(xhr, body, origin, account, callback);
  }

  /**
   * Sends a request to the Google Cloud Print API or queues if it needs to
   *     wait OAuth2 access token.
   * @param request Request to send or queue.
   */
  private sendOrQueueRequest_(request: CloudPrintRequest) {
    if (request.origin === DestinationOrigin.COOKIES) {
      this.sendRequest_(request);
      return;
    }

    // <if expr="chromeos_ash or chromeos_lacros">
    assert(request.origin === DestinationOrigin.DEVICE);
    if (this.accessTokenRequestPromise_ === null) {
      this.accessTokenRequestPromise_ =
          NativeLayerCrosImpl.getInstance().getAccessToken();
    }

    this.accessTokenRequestPromise_.then(
        (token: string) => this.onAccessTokenReady_(request, token));
    // </if>
  }

  /**
   * Sends a request to the Google Cloud Print API.
   * @param request Request to send.
   */
  private sendRequest_(request: CloudPrintRequest) {
    request.xhr.onreadystatechange = () => this.onReadyStateChange_(request);
    request.xhr.onerror = () => {
      console.warn('Error with request to Cloud Print');
    };
    try {
      request.xhr.send(request.body);
    } catch (error) {
      console.warn('Error with request to Cloud Print: ' + request.body);
      // Do nothing because otherwise JS crash reporting system will go crazy.
    }
  }

  /**
   * Creates an object containing information about the error based on the
   * request.
   * @param request Request that has been completed.
   * @return Information about the error.
   */
  private createErrorEventDetail_(request: CloudPrintRequest):
      CloudPrintInterfaceErrorEventDetail {
    const status200 = request.xhr.status === 200;
    return {
      status: request.xhr.status,
      errorCode: status200 ? request.result!['errorCode']! : 0,
      message: status200 ? request.result!['message']! : '',
      origin: request.origin,
      account: request.account,
    };
  }

  /**
   * Fires an event with information about the new active user and logged in
   * users.
   * @param activeUser The active user account.
   * @param users The currently logged in users. Omitted if the list of users
   *     has not changed.
   */
  private dispatchUserUpdateEvent_(activeUser: string, users?: string[]) {
    this.eventTarget_.dispatchEvent(new CustomEvent(
        CloudPrintInterfaceEventType.UPDATE_USERS,
        {detail: {activeUser: activeUser, users: users}}));
  }

  /**
   * Updates user info and session index from the {@code request} response.
   * @param request Request to extract user info from.
   */
  private setUsers_(request: CloudPrintRequest) {
    if (request.origin === DestinationOrigin.COOKIES) {
      const users = request.result!['request']['users'] || [];
      this.setUsers(users);
    }
  }

  setUsers(users: string[]) {
    this.userSessionIndex_ = {};
    for (let i = 0; i < users.length; i++) {
      this.userSessionIndex_[users[i]] = i;
    }
  }

  /**
   * Terminates search requests for requested {@code origins}.
   * @param origins Origins to terminate search requests for.
   */
  private abortSearchRequests_(origins: DestinationOrigin[]) {
    this.outstandingCloudSearchRequests_ =
        this.outstandingCloudSearchRequests_.filter(function(request) {
          if (origins.indexOf(request.origin) >= 0) {
            request.xhr.abort();
            return false;
          }
          return true;
        });
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * Called when a native layer receives access token. Assumes that the
   * destination type for this token is DestinationOrigin.DEVICE.
   * @param request The pending request that requires the access token.
   * @param accessToken The access token obtained.
   */
  private onAccessTokenReady_(request: CloudPrintRequest, accessToken: string) {
    assert(request.origin === DestinationOrigin.DEVICE);
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
   * @param request Request that was changed.
   */
  private onReadyStateChange_(request: CloudPrintRequest) {
    if (request.xhr.readyState === 4) {
      if (request.xhr.status === 200) {
        request.result = JSON.parse(request.xhr.responseText);
        if (request.origin === DestinationOrigin.COOKIES &&
            request.result!['success']) {
          this.xsrfTokens_[request.result!['request']!['user']!] =
              request.result!['xsrf_token']!;
        }
      }
      request.callback(request);
    }
  }

  /**
   * Called when the search request completes.
   * @param isRecent Whether the search request was for recent destinations.
   * @param request Request that has been completed.
   */
  private onSearchDone_(isRecent: boolean, request: CloudPrintRequest) {
    let lastRequestForThisOrigin = true;
    this.outstandingCloudSearchRequests_ =
        this.outstandingCloudSearchRequests_.filter(function(item) {
          if (item !== request && item.origin === request.origin) {
            lastRequestForThisOrigin = false;
          }
          return item !== request;
        });
    let activeUser = '';
    if (request.origin === DestinationOrigin.COOKIES) {
      activeUser = request.result! && request.result!['request']! &&
          request.result!['request']!['user']!;
    }
    if (request.xhr.status === 200 && request.result!['success']) {
      // Extract printers.
      const printerListJson = request.result!['printers']! || [];
      const printerList: Destination[] = [];
      printerListJson.forEach(function(printerJson) {
        try {
          printerList.push(
              parseCloudDestination(printerJson, request.origin, activeUser));
        } catch (err) {
          console.warn('Unable to parse cloud print destination: ' + err);
        }
      });
      // Extract and store users.
      this.setUsers_(request);
      this.dispatchUserUpdateEvent_(
          activeUser, request.result!['request']['users']);
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

      MetricsContext.getPrinters(PrinterType.CLOUD_PRINTER)
          .record(PrintPreviewInitializationEvents.FUNCTION_SUCCESSFUL);
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request) as
          CloudPrintInterfaceSearchDoneDetail;
      errorEventDetail.user = activeUser;
      errorEventDetail.searchDone = lastRequestForThisOrigin;
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SEARCH_FAILED,
          {detail: errorEventDetail}));

      MetricsContext.getPrinters(PrinterType.CLOUD_PRINTER)
          .record(PrintPreviewInitializationEvents.FUNCTION_FAILED);
    }
  }

  /**
   * Called when the submit request completes.
   * @param request Request that has been completed.
   */
  private onSubmitDone_(request: CloudPrintRequest) {
    if (request.xhr.status === 200 && request.result!['success']) {
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SUBMIT_DONE,
          {detail: request.result!['job']!['id']}));
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request) as
          CloudPrintInterfacePrinterFailedDetail;
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.SUBMIT_FAILED,
          {detail: errorEventDetail}));
    }
  }

  /**
   * Called when the printer request completes.
   * @param destinationId ID of the destination that was looked up.
   * @param request Request that has been completed.
   */
  private onPrinterDone_(destinationId: string, request: CloudPrintRequest) {
    // Special handling of the first printer request. It does not matter at
    // this point, whether printer was found or not.
    if (request.origin === DestinationOrigin.COOKIES && request.result &&
        request.result!['request']['user'] &&
        request.result!['request']['users']) {
      const users = request.result!['request']['users'];
      this.setUsers_(request);
      // In case the user account is known, but not the primary one,
      // activate it.
      if (request.account !== request.result!['request']['user'] &&
          request.account && this.userSessionIndex_[request.account!] > 0) {
        this.dispatchUserUpdateEvent_(request.account, users);
        // Repeat the request for the newly activated account.
        this.printer(
            request.result!['request']['params']['printerid'], request.origin,
            request.account);
        // Stop processing this request, wait for the new response.
        return;
      }
      this.dispatchUserUpdateEvent_(request.result!['request']['user'], users);
    }
    // Process response.
    if (request.xhr.status === 200 && request.result!['success']) {
      let activeUser = '';
      if (request.origin === DestinationOrigin.COOKIES) {
        activeUser = request.result!['request']['user'];
      }
      const printerJson = request.result!['printers']![0];
      let printer;
      try {
        printer =
            parseCloudDestination(printerJson, request.origin, activeUser);
      } catch (err) {
        console.warn(
            'Failed to parse cloud print destination: ' +
            JSON.stringify(printerJson));
        return;
      }
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.PRINTER_DONE, {detail: printer}));
    } else {
      const errorEventDetail = this.createErrorEventDetail_(request) as
          CloudPrintInterfacePrinterFailedDetail;
      errorEventDetail.destinationId = destinationId;
      this.eventTarget_.dispatchEvent(new CustomEvent(
          CloudPrintInterfaceEventType.PRINTER_FAILED,
          {detail: errorEventDetail}));
    }
  }

  static getInstance(): CloudPrintInterface {
    return instance || (instance = new CloudPrintInterfaceImpl());
  }

  static setInstance(obj: CloudPrintInterface) {
    instance = obj;
  }
}

let instance: CloudPrintInterface|null = null;

/**
 * Content type header value for a URL encoded HTTP request.
 */
const URL_ENCODED_CONTENT_TYPE_: string = 'application/x-www-form-urlencoded';

/**
 * Multi-part POST request boundary used in communication with Google
 * Cloud Print.
 */
const MULTIPART_BOUNDARY_: string = '----CloudPrintFormBoundaryjc9wuprokl8i';

/**
 * Content type header value for a multipart HTTP request.
 */
const MULTIPART_CONTENT_TYPE_: string =
    'multipart/form-data; boundary=' + MULTIPART_BOUNDARY_;

/**
 * Regex that extracts Chrome's version from the user-agent string.
 */
const VERSION_REGEXP_: RegExp = /.*Chrome\/([\d\.]+)/i;

type CloudPrintRequestInfo = {
  user: string,
  users?: string[], params: {[param: string]: string},
};

type CloudPrintResult = {
  errorCode?: number,
  message?: string, success: boolean, request: CloudPrintRequestInfo,
  job?: {id: string},
  user?: string,
  users?: string[],
  printers?: CloudDestinationInfo[],
  xsrf_token?: string,
};

export class CloudPrintRequest {
  /**
   * Partially prepared http request.
   */
  xhr: XMLHttpRequest;

  /**
   * Data to send with POST requests. Null for GET requests.
   */
  body: string|null;

  /**
   * Origin for destination.
   */
  origin: DestinationOrigin;

  /**
   * User account this request is expected to be executed for.
   */
  account: string|null;

  /**
   * Callback to invoke when request completes.
   */
  callback: (request: CloudPrintRequest) => void;

  /**
   * JSON response for requests.
   */
  result: CloudPrintResult|null = null;

  /**
   * Data structure that holds data for Cloud Print requests.
   * @param xhr Partially prepared http request.
   * @param body Data to send with POST requests.
   * @param origin Origin for destination.
   * @param account Account the request is sent for. Can be
   *     {@code null} or empty string if the request is not cookie bound or
   *     is sent on behalf of the primary user.
   * @param callback Callback to invoke when request completes.
   */
  constructor(
      xhr: XMLHttpRequest, body: string|null, origin: DestinationOrigin,
      account: string|null, callback: (request: CloudPrintRequest) => void) {
    this.xhr = xhr;
    this.body = body;
    this.origin = origin;
    this.account = account;
    this.callback = callback;
  }
}

class HttpParam {
  /**
   * Name of the parameter.
   */
  name: string;

  /**
   * Name of the value.
   */
  value: string;

  /**
   * Data structure that represents an HTTP parameter.
   * @param name Name of the parameter.
   * @param value Value of the parameter.
   */
  constructor(name: string, value: string) {
    this.name = name;
    this.value = value;
  }
}
