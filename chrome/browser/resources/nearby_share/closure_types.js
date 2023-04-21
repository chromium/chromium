// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Externs for nearby share files. Only needed for the nearby share tests which
 * are still using Closure.
 * @externs
 */


class NearbyConfirmationPageElement extends HTMLElement {
  constructor() {
    this.confirmationManager;
    this.payloadPreview;
    this.shareTarget;
    this.transferUpdateListener;
  }

  /**
   * @return {!{
   *    confirmationToken: string,
   *    transferStatus: Object,
   *    errorTitle: string,
   *    errorDescription: string
   * }}
   */
  getTransferInfoForTesting() {}

  /**
   * @param {string} path
   * @param {*} value
   */
  set(path, value) {}
}

/**
 * @constructor
 * @extends {HTMLElement}
 */
class NearbyDiscoveryPageElement extends HTMLElement {
  constructor() {
    this.selectedShareTarget;
  }

  /** @return {!Array<Object>} */
  getShareTargetsForTesting() {}

  /** @param {Object} shareTarget */
  selectShareTargetForTesting(shareTarget) {}
}

/**
 * @constructor
 * @extends {HTMLElement}
 */
function NearbyShareAppElement() {}


class LoadTimeData {
  overrideValues() {}
}

/** @type {!LoadTimeData} */
// eslint-disable-next-line no-var
var loadTimeData;
