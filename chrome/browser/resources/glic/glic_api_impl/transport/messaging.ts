// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class ResponseExtras {
  transfers: Transferable[] = [];

  // Add objects to transfer when sending the response over postMessage.
  addTransfer(...transfers: Transferable[]): void {
    this.transfers.push(...transfers);
  }
}

/**
 * Defines a request and optionally a corresponding response messages.
 */
export interface RequestDef {
  // The type of payload sent. Defaults to 'undefined', which means the request
  // has no request payload.
  request?: unknown;
  // The type of response payload. Defaults to 'void', which means the request
  // sends no response payload.
  response?: unknown;
  /**
   * Whether the request can be processed in the background.
   *
   * If true, the request is allowed to be sent and serviced in the
   * background.
   * If false (the default if omitted):
   * For Host requests, `BACKGROUND_RESPONSES` defines how these are handled.
   * For Client requests, it affects usage of `GatedSender`.
   */
  backgroundAllowed?: boolean;
}

// Validates each key is a RequestDef.
export type ValidateRequestMap<T extends {[K in keyof T]: RequestDef}> = T;


type AllValues<T> = T[keyof T];
type ArrayElement<ArrayType extends unknown[]> =
    ArrayType extends Array<infer ElementType>? ElementType : never;

// Do some high level checks that we don't accidentally add a non-cloneable or
// transferable type to our messages. These are not perfect.

// This can be extended for other transferable types when we need them. Using
// 'extends ...' for all possible Transferable types is too permissive.
type TransferableTypes = ArrayBuffer|Blob;
type StructuredClonableBasicType = string|boolean|number|void|undefined|null;
export type CheckStructuredClonable<T> =
    T extends StructuredClonableBasicType ? never : T extends unknown[] ?
    CheckStructuredClonable<ArrayElement<T>>:
    T extends Map<infer K, infer V>?
    (CheckStructuredClonable<K>&CheckStructuredClonable<V>) :
    T extends Function ?
    ['Function not structured cloneable', T] :
    T extends Promise<unknown>? ['Promise not structured cloneable', T] :
                                CheckStructuredClonableObject<T>;
export type CheckStructuredClonableObject<T> = T extends TransferableTypes ?
    never :
    AllValues<{[K in keyof T] -?: CheckStructuredClonable<T[K]>;}>;

// Same as A&B, but replaces properties that are in both with those in B.
export type ReplaceProperties<A, B> = {
  [K in keyof A |
   keyof B]: K extends keyof B ? B[K] : K extends keyof A ? A[K] : never;
};

export type RequestPayload<M, T extends keyof M> =
    M[T] extends {request: infer R} ? R : undefined;
export type ResponsePayload<M, T extends keyof M> =
    M[T] extends {response: infer R} ? R : void;

type Promisify<T> = T extends void ? void : Promise<T>;

export type MessageHandlerInterface<MapType> = {
  [Property in keyof MapType]: (
      payload: RequestPayload<MapType, Property>,
      extras: ResponseExtras,
      ) => Promisify<ResponsePayload<MapType, Property>>;
};

/* eslint-disable-next-line @typescript-eslint/naming-convention */
export function assertNever<_T extends never>() {}
