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

export interface HistogramInfo {
  // The unique name of the method used in histogram variations.
  // The method name is used, with an uppercase first letter, if this is not
  // provided.
  name?: string;
  // The ID of the histogram in histograms.xml.
  id: number;
}

/**
 * Defines a request and optionally a corresponding response messages.
 */
export interface RequestDef {
  name: string;
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
  // Provides information about the histogram to use for this method.
  // Undefined if no histogram should be recorded.
  histogram?: HistogramInfo;
}

export interface InterfaceDef {
  name: string;
  methods: readonly RequestDef[];
  methodMap?: Map<string, RequestDef>;
}

export type InterfaceDefMethods<I extends InterfaceDef> = {
  [M in I['methods'][number] as M['name']]: M;
};

// Defines a message type. Ensures the message is structured cloneable.
// Currently returns undefined, as no information about the message is retained
// at runtime.
export function defMessage<
    T extends(CheckStructuredClonable<T> extends never ? unknown : never)>():
    T {
  return undefined as unknown as T;
}

export function defInterface<const T extends InterfaceDef>(def: T): T {
  const tidyRequest = (m: RequestDef) => {
    const id = m.histogram?.id;
    if (id === undefined) {
      return m;
    }
    let name = m.histogram!.name;
    if (name === undefined) {
      name = m.name.charAt(0).toUpperCase() + m.name.slice(1);
    }
    return {...m, histogram: {name, id}};
  };
  return {
    ...def,
    methodMap: new Map(def.methods.map(m => [m.name, tidyRequest(m)])),
  } as T;
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

export type MessageHandlerInterface<I extends InterfaceDef> = {
  [Property in keyof InterfaceDefMethods<I>]: (
      payload: RequestPayload<InterfaceDefMethods<I>, Property>,
      extras: ResponseExtras,
      ) => Promisify<ResponsePayload<InterfaceDefMethods<I>, Property>>;
};

/* eslint-disable-next-line @typescript-eslint/naming-convention */
export function assertNever<_T extends never>() {}
