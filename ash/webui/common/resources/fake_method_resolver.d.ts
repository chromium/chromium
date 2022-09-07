// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

declare class FakeMethodState<T = any> {
  resolveMethod(): Promise<T>;
  resolveMethodWithDelay(delayMs: number): Promise<T>;
  setResult(result: T): void;
}

export class FakeMethodResolver<T = any> {
  register(methodName: string): void;
  setResult(methodName: string, result: T): void;
  resolveMethod(methodName: string): Promise<T>;
  resolveMethodWithDelay(methodName: string, delayMs: number): Promise<T>;
}
