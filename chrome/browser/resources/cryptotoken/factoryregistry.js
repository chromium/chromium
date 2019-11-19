// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class providing common dependencies for the extension's
 * top half.
 */
'use strict';

/**
 * @param {!AppIdCheckerFactory} appIdCheckerFactory An appId checker factory.
 * @param {!ApprovedOrigins} approvedOrigins An origin approval implementation.
 * @param {!CountdownFactory} countdownFactory A countdown timer factory.
 * @param {!OriginChecker} originChecker An origin checker.
 * @param {!RequestHelper} requestHelper A request helper.
 * @param {!SystemTimer} sysTimer A system timer implementation.
 * @param {!TextFetcher} textFetcher A text fetcher.
 * @constructor
 */
function FactoryRegistry(
    appIdCheckerFactory, approvedOrigins, countdownFactory, originChecker,
    sysTimer) {
  /** @private {!AppIdCheckerFactory} */
  this.appIdCheckerFactory_ = appIdCheckerFactory;
  /** @private {!ApprovedOrigins} */
  this.approvedOrigins_ = approvedOrigins;
  /** @private {!CountdownFactory} */
  this.countdownFactory_ = countdownFactory;
  /** @private {!OriginChecker} */
  this.originChecker_ = originChecker;
  /** @private {!SystemTimer} */
  this.sysTimer_ = sysTimer;
}

/** @return {!AppIdCheckerFactory} An appId checker factory. */
FactoryRegistry.prototype.getAppIdCheckerFactory = function() {
  return this.appIdCheckerFactory_;
};

/** @return {!ApprovedOrigins} An origin approval implementation. */
FactoryRegistry.prototype.getApprovedOrigins = function() {
  return this.approvedOrigins_;
};

/** @return {!CountdownFactory} A countdown factory. */
FactoryRegistry.prototype.getCountdownFactory = function() {
  return this.countdownFactory_;
};

/** @return {!OriginChecker} An origin checker. */
FactoryRegistry.prototype.getOriginChecker = function() {
  return this.originChecker_;
};

/** @return {!SystemTimer} A system timer implementation. */
FactoryRegistry.prototype.getSystemTimer = function() {
  return this.sysTimer_;
};
