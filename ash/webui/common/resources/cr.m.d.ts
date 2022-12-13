// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: TypeScript UIs should generally use cr.ts directly, and not rely on
// these definitions. These are checked in so that UIs in transition can keep
// relying on cr.m.js.

/* eslint-disable @typescript-eslint/naming-convention */

export function webUIResponse(
    id: string, isSuccess: boolean, response: any): void;

export function sendWithPromise(
    methodName: string, ...varArgs: any[]): Promise<any>;

export function webUIListenerCallback(event: string, ..._varArgs: any[]): void;

export interface WebUIListener {
  eventName: string;
  uid: number;
}

export function addWebUIListener(
    eventName: string, callback: Function): WebUIListener;

export function removeWebUIListener(listener: WebUIListener): boolean;
