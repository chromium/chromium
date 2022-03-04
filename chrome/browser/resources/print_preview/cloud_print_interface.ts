// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationOrigin} from './data/destination.js';

/**
 * Event types dispatched by the cloudprint interface.
 */
export enum CloudPrintInterfaceEventType {
  INVITES_DONE = 'cloudprint.CloudPrintInterface.INVITES_DONE',
  INVITES_FAILED = 'cloudprint.CloudPrintInterface.INVITES_FAILED',
  PRINTER_DONE = 'cloudprint.CloudPrintInterface.PRINTER_DONE',
  PRINTER_FAILED = 'cloudprint.CloudPrintInterface.PRINTER_FAILED',
  PROCESS_INVITE_DONE = 'cloudprint.CloudPrintInterface.PROCESS_INVITE_DONE',
  SEARCH_DONE = 'cloudprint.CloudPrintInterface.SEARCH_DONE',
  SEARCH_FAILED = 'cloudprint.CloudPrintInterface.SEARCH_FAILED',
  SUBMIT_DONE = 'cloudprint.CloudPrintInterface.SUBMIT_DONE',
  SUBMIT_FAILED = 'cloudprint.CloudPrintInterface.SUBMIT_FAILED',
  UPDATE_USERS = 'cloudprint.CloudPrintInterface.UPDATE_USERS',
}

export type CloudPrintInterfaceErrorEventDetail = {
  status: number,
  errorCode: number,
  message: string,
  origin: DestinationOrigin,
  account: (string|null),
};

export type CloudPrintInterfaceSearchDoneDetail = {
  user: string,
  printers?: Destination[], searchDone: boolean,
}&CloudPrintInterfaceErrorEventDetail;

export type CloudPrintInterfacePrinterFailedDetail = {
  destinationId: string,
}&CloudPrintInterfaceErrorEventDetail;

export interface CloudPrintInterface {
  /** @return Whether cookie destinations are disabled. */
  areCookieDestinationsDisabled(): boolean;

  /**
   * @param baseUrl Base part of the Google Cloud Print service URL
   *     with no trailing slash. For example,
   *     'https://www.google.com/cloudprint'.
   */
  configure(baseUrl: string, isInAppKioskMode: boolean, uiLocale: string): void;

  /** @return Whether the interface has been configured. */
  isConfigured(): boolean;

  /**
   * @return Whether a search for cloud destinations is in progress.
   */
  isCloudDestinationSearchInProgress(): boolean;

  /** @return The event target for this interface. */
  getEventTarget(): EventTarget;

  /**
   * Sends Google Cloud Print search API request.
   * @param opt_account Account the search is sent for. When null or omitted,
   *     the search is done on behalf of the primary user.
   * @param opt_origin When specified, searches destinations for |opt_origin|
   *     only, otherwise starts searches for all origins.
   */
  search(opt_account?: (string|null), opt_origin?: DestinationOrigin): void;

  /**
   * Sets the currently active users.
   */
  setUsers(users: string[]): void;

  /**
   * Sends a Google Cloud Print submit API request.
   * @param destination Cloud destination to print to.
   * @param printTicket The print ticket to print.
   * @param documentTitle Title of the document.
   * @param data Base64 encoded data of the document.
   */
  submit(
      destination: Destination, printTicket: string, documentTitle: string,
      data: string): void;

  /**
   * Sends a Google Cloud Print printer API request.
   * @param printerId ID of the printer to lookup.
   * @param origin Origin of the printer.
   * @param account Account this printer is registered for. When
   *     provided for COOKIES {@code origin}, and users sessions are still not
   *     known, will be checked against the response (both success and failure
   *     to get printer) and, if the active user account is not the one
   *     requested, {@code account} is activated and printer request reissued.
   */
  printer(printerId: string, origin: DestinationOrigin, account?: string): void;
}
