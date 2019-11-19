// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterface} from './cloud_print_interface.js';

/** @implements {CloudPrintInterface} */
export class CloudPrintInterfaceNative {
  constructor() {}

  /** @override */
  areCookieDestinationsDisabled() {}

  /** @override */
  isCloudDestinationSearchInProgress() {}

  /** @override */
  getEventTarget() {}

  /** @override */
  search(opt_account, opt_origin) {}

  /** @override */
  setUsers(users) {}

  /** @override */
  invites(account) {}

  /** @override */
  processInvite(invitation, accept) {}

  /** @override */
  submit(destination, printTicket, documentTitle, data) {}

  /** @override */
  printer(printerId, origin, account) {}
}
