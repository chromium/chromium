// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {PromiseResolver} from './promise_resolver.js';

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
    /** @private {T} */
    this.result_ = undefined;
  }

  /**
   * Resolve the method with the supplied result.
   * @return {!Promise}
   */
  resolveMethod() {
    const resolver = new PromiseResolver();
    resolver.resolve(this.result_);
    return resolver.promise;
  }

  /**
   * Resolve the method with the supplied result after delayMs.
   * @param {number} delayMs
   * @return {!Promise}
   */
   resolveMethodWithDelay(delayMs) {
     const resolver = new PromiseResolver();
     setTimeout(() => {
       resolver.resolve(this.result_);
     }, delayMs);
     return resolver.promise;
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
   * Causes the promise for methodName to resolve after delayMs.
   * @param {string} methodName
   * @param {number} delayMs
   * @return {!Promise}
   */
  resolveMethodWithDelay(methodName, delayMs) {
    return this.getState_(methodName).resolveMethodWithDelay(delayMs);
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
