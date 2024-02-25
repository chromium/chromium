// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class FakeObservables<T = any> {
  register(methodName: string): void;
  registerObservableWithArg(methodName: string): void;
  observe(methodName: string, callback: (...args: T[]) => void): void;
  observeWithArg(methodName: string, arg: string, callback: (arg0: T) => void):
      void;
  setObservableData(methodName: string, observations: T[]): void;
  setObservableDataForArg(methodName: string, arg: string, observations: T[]):
      void;
  startTriggerOnInterval(methodName: string, intervalMs: number): void;
  startTriggerOnIntervalWithArg(
      methodName: string, arg: string, intervalMs: number): void;
  stopTriggerOnInterval(methodName: string): void;
  stopTriggerOnIntervalWithArg(methodName: string, arg: string): void;
  stopAllTriggerIntervals(): void;
  trigger(methodName: string): void;
  triggerWithArg(methodName: string, arg: string): void;
}
