// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';
import {Destination, DestinationOrigin} from './data/destination.js';
import {Invitation} from './data/invitation.js';

/**
 * Event types dispatched by the cloudprint interface.
 * @enum {string}
 */
export const CloudPrintInterfaceEventType = {
  INVITES_DONE: 'cloudprint.CloudPrintInterface.INVITES_DONE',
  INVITES_FAILED: 'cloudprint.CloudPrintInterface.INVITES_FAILED',
  PRINTER_DONE: 'cloudprint.CloudPrintInterface.PRINTER_DONE',
  PRINTER_FAILED: 'cloudprint.CloudPrintInterface.PRINTER_FAILED',
  PROCESS_INVITE_DONE: 'cloudprint.CloudPrintInterface.PROCESS_INVITE_DONE',
  SEARCH_DONE: 'cloudprint.CloudPrintInterface.SEARCH_DONE',
  SEARCH_FAILED: 'cloudprint.CloudPrintInterface.SEARCH_FAILED',
  SUBMIT_DONE: 'cloudprint.CloudPrintInterface.SUBMIT_DONE',
  SUBMIT_FAILED: 'cloudprint.CloudPrintInterface.SUBMIT_FAILED',
  UPDATE_USERS: 'cloudprint.CloudPrintInterface.UPDATE_USERS',
};

/**
 * @typedef {{
 *   status: number,
 *   errorCode: number,
 *   message: string,
 *   origin: !DestinationOrigin,
 * }}
 */
export let CloudPrintInterfaceErrorEventDetail;

/**
 * @typedef {{
 *   user: string,
 *   origin: !DestinationOrigin,
 *   printers: (!Array<!Destination>|undefined),
 *   searchDone: boolean,
 * }}
 */
export let CloudPrintInterfaceSearchDoneDetail;

/**
 * @typedef {{
 *   destinationId: string,
 *   origin: !DestinationOrigin,
 * }}
 */
export let CloudPrintInterfacePrinterFailedDetail;

/**
 * @typedef {{
 *   invitations: !Array<!Invitation>,
 *   user: string,
 * }}
 */
export let CloudPrintInterfaceInvitesDoneDetail;

/**
 * @typedef {{
 *   invitation: !Invitation,
 *   printer: ?Destination,
 *   accept: boolean,
 *   user: string,
 * }}
 */
export let CloudPrintInterfaceProcessInviteDetail;

/** @interface */
export class CloudPrintInterface {
  /** @return {boolean} Whether cookie destinations are disabled. */
  areCookieDestinationsDisabled() {}

  /**
   * @return {boolean} Whether a search for cloud destinations is in progress.
   */
  isCloudDestinationSearchInProgress() {}

  /** @return {!EventTarget} The event target for this interface. */
  getEventTarget() {}

  /**
   * Sends Google Cloud Print search API request.
   * @param {?string=} opt_account Account the search is sent for. When
   *      null or omitted, the search is done on behalf of the primary user.
   * @param {DestinationOrigin=} opt_origin When specified,
   *     searches destinations for {@code opt_origin} only, otherwise starts
   *     searches for all origins.
   */
  search(opt_account, opt_origin) {}

  /**
   * Sets the currently active users.
   * @param {!Array<string>} users
   */
  setUsers(users) {}

  /**
   * Sends Google Cloud Print printer sharing invitations API requests.
   * @param {string} account Account the request is sent for.
   */
  invites(account) {}

  /**
   * Accepts or rejects printer sharing invitation.
   * @param {!Invitation} invitation Invitation to process.
   * @param {boolean} accept Whether to accept this invitation.
   */
  processInvite(invitation, accept) {}

  /**
   * Sends a Google Cloud Print submit API request.
   * @param {!Destination} destination Cloud destination to
   *     print to.
   * @param {string} printTicket The print ticket to print.
   * @param {string} documentTitle Title of the document.
   * @param {string} data Base64 encoded data of the document.
   */
  submit(destination, printTicket, documentTitle, data) {}

  /**
   * Sends a Google Cloud Print printer API request.
   * @param {string} printerId ID of the printer to lookup.
   * @param {!DestinationOrigin} origin Origin of the printer.
   * @param {string=} account Account this printer is registered for. When
   *     provided for COOKIES {@code origin}, and users sessions are still not
   *     known, will be checked against the response (both success and failure
   *     to get printer) and, if the active user account is not the one
   *     requested, {@code account} is activated and printer request reissued.
   */
  printer(printerId, origin, account) {}
}
