// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

// TODO(gavindodd): Currently the addObserver and setObservableData do not
// enforce using the same type for a given method. Revisit when TypeScript is
// supported.

/**
 * @fileoverview
 * Implements a helper class for faking asynchronous observables.
 */

/**
 * Maintains state about an observable and the data it will produce.
 * @template T
 **/
class FakeObservableState {
  constructor() {
    /**
     * The list of functions that will be notified when the observable
     * is triggered.
     * @private {!Array<!function(!T)>}
     **/
    this.observers_ = [];

    /**
     * Array of observations to be supplied by the observer.
     * @private {!Array<T>}
     **/
    this.observations_ = [];

    /**
     * The index of the next observation.
     * @private {number}
     **/
    this.index_ = -1;

    /**
     * Id of the timer if enabled.
     * @private {number}
     */
    this.timerId_ = -1;
  }

  /** @param {!Array<!T>} observations */
  setObservableData(observations) {
    this.observations_ = observations;
    this.index_ = 0;
  }

  /** @param {!function(!T)} callback */
  addObserver(callback) {
    this.observers_.push(callback);
  }

  /**
   * Start firing the observers on a fixed interval. setObservableData() must
   * already have been called.
   * @param {number} intervalMs
   */
  startTriggerOnInterval(intervalMs) {
    assert(this.index_ >= 0);
    if (this.timerId_ != -1) {
      this.stopTriggerOnInterval();
    }

    assert(this.timerId_ == -1);
    this.timerId_ = setInterval(this.trigger.bind(this), intervalMs);
  }

  /**
   * Disables the observer firing automatically on an interval.
   */
  stopTriggerOnInterval() {
    if (this.timerId_ != -1) {
      clearInterval(this.timerId_);
      this.timerId_ = -1;
    }
  }

  /**
   * Causes the observable to trigger and notify all observers of the next
   * observation value.
   */
  trigger() {
    assert(this.observations_.length > 0);
    assert(this.index_ >= 0);
    assert(this.index_ < this.observations_.length);

    // Get the value of this observation and update the index to point to the
    // next one.
    const value = this.observations_[this.index_];
    this.index_ = (this.index_ + 1) % this.observations_.length;

    // Fire all the callbacks that are observing this observable.
    for (const fn of this.observers_) {
      if (Array.isArray(value)) {
        fn.apply(null, value);
      } else {
        fn(value);
      }
    }
  }
}

/**
 * Manages a map of fake observables and the fake data they will produce
 * when triggered.
 * @template T
 */
export class FakeObservables {
  constructor() {
    /** @private {!Map<string, !FakeObservableState>} */
    this.observables_ = new Map();

    /**
     * Set of observables that are capable of taking an additional argument.
     * @private {!Set<string>}
     */
    this.sharedObservables_ = new Set();
  }

  /**
   * Register an observable. Other calls to this class will assert if the
   * observable has not been registered.
   * @param {string} methodName
   */
  register(methodName) {
    this.observables_.set(methodName, new FakeObservableState());
  }

  /**
   * Register an observable that can take a single argument to its observe
   * method. The argument can identify a more specific entity within a group to
   * observe.
   * @param {string} methodName
   */
  registerObservableWithArg(methodName) {
    this.sharedObservables_.add(methodName);
  }

  /**
   * Supply the callback for observing methodName.
   * @param {string} methodName
   * @param {!Function} callback
   */
  observe(methodName, callback) {
    this.getObservable_(methodName).addObserver(callback);
  }

  /**
   * Supply the callback for observing a methodName that belongs to a shared
   * observer group.
   * @param {string} methodName
   * @param {string} arg
   * @param {!function(!T)} callback
   */
  observeWithArg(methodName, arg, callback) {
    this.getObservable_(this.lookupMethodWithArgName_(methodName, arg))
        .addObserver(callback);
  }

  /**
   * Sets the data that will be produced when the observable is triggered.
   * Each observation produces the next value in the array and wraps around
   * when all observations have been produced.
   * If the observation type T is an array it will be treated as a list of
   * parameters to the onObservation method using apply().
   * @param {string} methodName
   * @param {!Array<!T>} observations
   */
  setObservableData(methodName, observations) {
    this.getObservable_(methodName).setObservableData(observations);
  }

  /**
   * Sets the data that will be produced when an observable that takes
   * arg as a parameter is triggered.
   * @param {string} methodName
   * @param {string} arg
   * @param {!Array<!T>} observations
   */
  setObservableDataForArg(methodName, arg, observations) {
    assert(
        this.sharedObservables_.has(methodName),
        `${methodName} not found in sharedObservables_`);
    const methodNameToRegister = this.createMethodWithArgName_(methodName, arg);
    const isMethodRegistered = !!this.observables_.get(methodNameToRegister);
    if (!isMethodRegistered) {
      this.register(methodNameToRegister);
    }
    this.setObservableData(methodNameToRegister, observations);
  }

  /**
   * Start firing the observer on a fixed interval. setObservableData() must
   * already have been called.
   * @param {string} methodName
   * @param {number} intervalMs
   */
  startTriggerOnInterval(methodName, intervalMs) {
    this.getObservable_(methodName).startTriggerOnInterval(intervalMs);
  }

  /**
   * Disables the observer firing automatically on an interval.
   * @param {string} methodName
   */
  stopTriggerOnInterval(methodName) {
    this.getObservable_(methodName).stopTriggerOnInterval();
  }

  /**
   * Start firing the shared observer for |arg| on a fixed interval.
   * setObservableData() must already have been called.
   * @param {string} methodName
   * @param {string} arg
   * @param {number} intervalMs
   */
  startTriggerOnIntervalWithArg(methodName, arg, intervalMs) {
    this.getObservable_(this.lookupMethodWithArgName_(methodName, arg))
        .startTriggerOnInterval(intervalMs);
  }

  /**
   * Disables the shared observer for |arg| firing automatically on an interval.
   * @param {string} methodName
   * @param {string} arg
   */
  stopTriggerOnIntervalWithArg(methodName, arg) {
    this.getObservable_(this.lookupMethodWithArgName_(methodName, arg))
        .stopTriggerOnInterval();
  }

  /**
   * Disables all observers firing automatically on an interval.
   */
  stopAllTriggerIntervals() {
    for (const obs of this.observables_.values()) {
      obs.stopTriggerOnInterval();
    }
  }

  /**
   * Causes the observable to trigger and notify all observers of the next
   * observation value.
   * @param {string} methodName
   */
  trigger(methodName) {
    this.getObservable_(methodName).trigger();
  }

  /**
   * Causes a shared observable to trigger and notify all observers observing
   * |arg| of the next observation value.
   * @param {string} methodName
   * @param {string} arg
   */
  triggerWithArg(methodName, arg) {
    this.getObservable_(this.lookupMethodWithArgName_(methodName, arg))
        .trigger();
  }

  /**
   * Return the Observable for methodName.
   * @param {string} methodName
   * @return {!FakeObservableState}
   * @private
   */
  getObservable_(methodName) {
    const observable = this.observables_.get(methodName);
    assert(!!observable, `Observable '${methodName}' not found.`);
    return observable;
  }

  /**
   * Returns a concatenated form of methodName and arg separated by
   * an underscore.
   * @param {string} methodName
   * @param {string} arg
   * @return {string}
   * @private
   */
  createMethodWithArgName_(methodName, arg) {
    return `${methodName}_${arg}`;
  }

  /**
   * Returns the methodName that was registered for a shared observable.
   * You must register methodName and set data specifically for arg before
   * calling this method.
   * @param {string} methodName
   * @param {string} arg
   * @return {string}
   * @private
   */
  lookupMethodWithArgName_(methodName, arg) {
    const observableName = this.createMethodWithArgName_(methodName, arg);
    assert(
        !!this.observables_.get(observableName),
        `Observable '${observableName}' not found.`);
    return observableName;
  }
}
