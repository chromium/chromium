// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FakeMethodResolver} from 'chrome://resources/ash/common/fake_method_resolver.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

import {PowerRoutineResult, RoutineResult, RoutineResultInfo, RoutineRunnerInterface, RoutineType, StandardRoutineResult, SystemRoutineControllerInterface} from './system_routine_controller.mojom-webui.js';

/**
 * @fileoverview
 * Implements a fake version of the SystemRoutineController mojo interface.
 */

export class FakeSystemRoutineController implements
    SystemRoutineControllerInterface {
  private methods_: FakeMethodResolver = new FakeMethodResolver();
  private routineResults_: Map<RoutineType, RoutineResult> = new Map();
  /**
   * Controls the delay resolving routines. By default this is 0 and routines
   * resolve immediately, but still asynchronously.
   */
  private delayTimeMilliseconds_: number = 0;
  /**
   * Holds the resolver for the next routine waiting to be resolved. This
   * will be null if no routines are in progress.
   */
  private resolver_: PromiseResolver<any>|null = null;
  // Holds the remote that is called on completion.
  private remote_: RoutineRunnerInterface|null = null;
  // Holds the type of the routine currently running.
  private routineType_: RoutineType|null = null;

  constructor() {
    this.registerMethods();
  }

  getAllRoutines(): RoutineType[] {
    return [
      RoutineType.kBatteryCharge,
      RoutineType.kBatteryDischarge,
      RoutineType.kCpuStress,
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
      RoutineType.kCpuPrime,
      RoutineType.kMemory,
      RoutineType.kCaptivePortal,
      RoutineType.kDnsLatency,
      RoutineType.kDnsResolution,
      RoutineType.kDnsResolverPresent,
      RoutineType.kGatewayCanBePinged,
      RoutineType.kHasSecureWiFiConnection,
      RoutineType.kHttpFirewall,
      RoutineType.kHttpsFirewall,
      RoutineType.kHttpsLatency,
      RoutineType.kLanConnectivity,
      RoutineType.kSignalStrength,
      RoutineType.kArcHttp,
      RoutineType.kArcPing,
      RoutineType.kArcDnsResolution,
    ];
  }

  // Implements SystemRoutineController.GetSupportedRoutines
  getSupportedRoutines(): Promise<{routines: RoutineType[]}> {
    return this.methods_.resolveMethod('getSupportedRoutines');
  }

  // Sets the value that will be returned when calling getSupportedRoutines().
  setFakeSupportedRoutines(routines: RoutineType[]): void {
    this.methods_.setResult('getSupportedRoutines', {routines: routines});
  }

  // Implements SystemRoutineController.RunRoutine.
  runRoutine(routineType: RoutineType, remoteRunner: RoutineRunnerInterface):
      void {
    this.resolver_ = new PromiseResolver();
    this.remote_ = remoteRunner;
    this.routineType_ = routineType;

    // If there is a positive or zero delay then setup a timer, otherwise
    // the routine will wait until resolveRoutineForTesting() is called.
    if (this.delayTimeMilliseconds_ >= 0) {
      setTimeout(() => {
        this.fireRemoteWithResult_();
      }, this.delayTimeMilliseconds_);
    }
  }

  // Setup method resolvers.
  registerMethods(): void {
    this.methods_.register('getSupportedRoutines');
  }

  setFakeStandardRoutineResult(
      routineType: RoutineType, routineResult: StandardRoutineResult): void {
    this.routineResults_.set(
        routineType, ({simpleResult: routineResult} as RoutineResult));
  }

  setFakePowerRoutineResult(
      routineType: RoutineType, routineResult: PowerRoutineResult): void {
    this.routineResults_.set(
        routineType, ({powerResult: routineResult} as RoutineResult));
  }

  /**
   * Sets how long each routine waits before resolving. Setting to a value
   * >=0 will use a timer to delay resolution automatically. Setting to -1
   * will allow a caller to manually determine when each routine resolves.
   *
   * If set to -1, the caller can call resolveRoutineForTesting() and
   * isRoutineInProgressForTesting() to manually control in unit tests.
   */
  setDelayTimeInMillisecondsForTesting(delayMilliseconds: number): void {
    assert(delayMilliseconds >= -1);
    this.delayTimeMilliseconds_ = delayMilliseconds;
  }

  // Returns the pending run routine promise.
  getRunRoutinePromiseForTesting(): Promise<void> {
    assert(this.resolver_ != null);
    return this.resolver_.promise;
  }

  /**
   * Resolves a routine that is in progress. The delay time must be set to
   * -1 or this method will assert. It will also assert if there is no
   * routine running. Use isRoutineInProgressForTesting() to determine if
   * a routing is currently in progress.
   */
  resolveRoutineForTesting(): Promise<void> {
    assert(this.delayTimeMilliseconds_ == -1);
    assert(this.resolver_ != null);
    const promise = this.resolver_.promise;

    this.fireRemoteWithResult_();
    return promise;
  }

  /**
   * Returns true if a routine is in progress. The delay time must be -1
   * otherwise this function will assert.
   */
  isRoutineInProgressForTesting(): boolean {
    assert(this.delayTimeMilliseconds_ == -1);
    return this.resolver_ != null;
  }

  // Returns the expected result for a running routine.
  private getResultInfo_(): RoutineResultInfo {
    assert(this.routineType_ != null);
    let result = this.routineResults_.get(this.routineType_);
    if (result == undefined) {
      result = {simpleResult: StandardRoutineResult.kExecutionError} as
          RoutineResult;
    }

    const resultInfo: RoutineResultInfo = {
      type: this.routineType_,
      result: result,
    };

    return resultInfo;
  }

  // Fires the remote callback with the expected result.
  private fireRemoteWithResult_(): void {
    this.remote_!.onRoutineResult(this.getResultInfo_());
    this.resolver_!.resolve(null);
    this.resolver_ = null;
    this.remote_ = null;
    this.routineType_ = null;
  }
}
