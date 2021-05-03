// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';

/**
 * @fileoverview
 * Implements a helper class for managing a fake API with async
 * methods.
 */

/**
 * Maintains a resolver and the return value of the fake method.
 * @template T
 */
class FakeMethodState {
  constructor() {
    /** @private {PromiseResolver} */
    this.resolver_ = new PromiseResolver();

    /** @private {T} */
    this.result_ = undefined;
  }

  /**
   * Resolve the method with the supplied result.
   * @return {!Promise}
   */
  resolveMethod() {
    this.resolver_.resolve(this.result_);
    return this.resolver_.promise;
  }

  /**
   * Set the result for this method.
   * @param {T} result
   */
  setResult(result) {
    this.result_ = result;
  }
}

/**
 * Manages a map of fake async methods, their resolvers and the fake
 * return value they will produce.
 * @template T
 **/
export class FakeMethodResolver {
  constructor() {
    /** @private {!Map<string, !FakeMethodState>} */
    this.methodMap_ = new Map();
  }

  /**
   * Registers methodName with the resolver. Calling other methods with a
   * methodName that has not been registered will assert.
   * @param {string} methodName
   */
  register(methodName) {
    this.methodMap_.set(methodName, new FakeMethodState());
  }

  /**
   * Set the value that methodName will produce when it resolves.
   * @param {string} methodName
   * @param {T} result
   */
  setResult(methodName, result) {
    this.getState_(methodName).setResult(result);
  }

  /**
   * Causes the promise for methodName to resolve.
   * @param {string} methodName
   * @return {!Promise}
   */
  resolveMethod(methodName) {
    return this.getState_(methodName).resolveMethod();
  }

  /**
   * Return the FakeMethodState for methodName.
   * @param {string} methodName
   * @return {!FakeMethodState}
   * @private
   */
  getState_(methodName) {
    const state = this.methodMap_.get(methodName);
    assert(!!state, `Method '${methodName}' not found.`);
    return state;
  }
}
