// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {                          //
             AdditionalContext,        //
             AdditionalContextPart,    //
             AnnotatedPageData,        //
             ErrorReasonTypes,         //
             ErrorWithReason,          //
             FocusedTabDataHasFocus,   //
             FocusedTabDataHasNoFocus, //
             InvokeOptions,            //
             PageMetadata,             //
             PdfDocumentData,          //
             ResumeActorTaskResult,    //
             TabContextResult,         //
             TabData,                  //
} from '../glic_api/glic_api.js';

import type {InterfaceDef, InterfaceDefMethods} from './transport/messaging.js';
import {defInterface} from './transport/messaging.js';
import type {ErrorCodec, TransferableException} from './transport/post_message_transport.js';

/*
This file defines messages sent over postMessage in-between the Glic WebUI
and the Glic web client.

Most requests closely match signatures of API methods. Where possible, name
messages by concatenating the interface name with the method name. This helps
readability, and ensures that each name is unique.
*/

export const WebClientHostDef = defInterface({
  name: 'WebClientHost',
  methods: [],
});


export type WebClientHost = typeof WebClientHostDef;

// Types of requests to the GlicWebClient.
export const WebClientDef = defInterface({
  name: 'WebClient',
  methods: [],
});

export type WebClient = typeof WebClientDef;

export type WebClientRequestTypes = InterfaceDefMethods<WebClient>;

export type HostRequestTypes = InterfaceDefMethods<WebClientHost>;

// Provides metrics histogram information for a host request type.
export interface HostRequestHistogramInfo {
  // The name of the host request type, used as histogram suffix.
  name: string;
  // The histogram enum value for this host request type.
  id: number;
}

export function getHostRequestHistogramInfo(
    requestType: string,
    interfaceDef: InterfaceDef|undefined): HostRequestHistogramInfo|undefined {
  if (!interfaceDef) {
    return undefined;
  }
  const method = interfaceDef.methodMap?.get(requestType);
  if (!method || !method.histogram) {
    return undefined;
  }
  const name = (method.histogram as {name?: string}).name ??
      (requestType.charAt(0).toUpperCase() + requestType.slice(1));
  return {
    name,
    id: method.histogram.id,
  };
}

//
// Types used in messages that are not exposed directly to the API.
//
// Some types cannot be directly transported over postMessage. The pattern we
// use here is to define a new type with a 'Private' suffix, which replaces the
// property types that cannot be structured cloned, with types that can.
//
// Note that it's a good idea to replace properties with new properties that
// have the same name, but different type. This ensures that we don't
// accidentally leave the private data on the returned object.
//

// TabData format for postMessage transport.
export declare interface TabDataPrivate extends Omit<TabData, 'favicon'> {
  favicon?: RgbaImage;
}

// A bitmap, used to store data from a BitmapN32 without conversion.
export declare interface RgbaImage {
  dataRGBA: ArrayBuffer;
  width: number;
  height: number;
  alphaType: ImageAlphaType;
  colorType: ImageColorType;
}

export enum ImageAlphaType {
  // RGB values are unmodified.
  UNPREMUL = 0,
  // RGB values have been premultiplied by alpha.
  PREMUL = 1,
}

// Chromium currently only uses a single color type for BitmapN32.
export enum ImageColorType {
  BGRA = 0,
  RGBA = 1,
}


// FocusedTabData data for postMessage transport.
export declare interface FocusedTabDataPrivate {
  hasFocus?: Omit<FocusedTabDataHasFocus, 'tabData'>&{tabData: TabDataPrivate};
  hasNoFocus?: Omit<FocusedTabDataHasNoFocus, 'tabFocusCandidateData'>&
      {tabFocusCandidateData?: TabDataPrivate};
}

// TabContextResult data for postMessage transport.
export declare interface TabContextResultPrivate extends Omit<
    TabContextResult,
    'tabData'|'screenshotInfo'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  screenshotInfo?: ArrayBuffer;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

// ResumeActorTaskResult data for postMessage transport.
export declare interface ResumeActorTaskResultPrivate extends Omit<
    ResumeActorTaskResult,
    'tabData'|'screenshotInfo'|'pdfDocumentData'|'annotatedPageData'> {
  tabData: TabDataPrivate;
  screenshotInfo?: ArrayBuffer;
  pdfDocumentData?: PdfDocumentDataPrivate;
  annotatedPageData?: AnnotatedPageDataPrivate;
}

export declare interface PdfDocumentDataPrivate extends
    Omit<PdfDocumentData, 'pdfData'> {
  pdfData?: ArrayBuffer;
}

export declare interface AnnotatedPageDataPrivate extends
    Omit<AnnotatedPageData, 'annotatedPageContent'> {
  annotatedPageContent?: ArrayBuffer;
  metadata?: PageMetadata;
}

export declare interface AdditionalContextPartPrivate extends
    Omit<AdditionalContextPart, 'annotatedPageData'|'pdf'|'data'|'tabContext'> {
  annotatedPageData?: AnnotatedPageDataPrivate;
  pdf?: PdfDocumentDataPrivate;
  data?: {mimeType: string, data: ArrayBuffer};
  filename?: string;
  tabContext?: TabContextResultPrivate;
}

export declare interface AdditionalContextPrivate extends
    Omit<AdditionalContext, 'parts'> {
  parts: AdditionalContextPartPrivate[];
}

export declare interface InvokeOptionsPrivate extends
    Omit<InvokeOptions, 'context'> {
  context?: AdditionalContextPrivate;
}

export declare interface ImageInfoPrivate {
  caption?: string;
  sourceOrigin?: string;
  url: string;
  mimeType?: string;
}

export declare interface ImageBytesResultPrivate {
  bytes: ArrayBuffer;
  imageInfo: ImageInfoPrivate;
}

export class ErrorWithReasonImpl<T extends keyof ErrorReasonTypes> extends Error
    implements ErrorWithReason<T> {
  constructor(
      public reasonType: T,
      public reason: ErrorReasonTypes[T],
      message?: string,
  ) {
    super(message ?? `${reasonType} Error: ${reason}`);
  }
}

/** Any ErrorWithReason<T>.reason type. */
export type AnyErrorReasonType = ErrorReasonTypes[keyof ErrorReasonTypes];
/** Any ErrorWithReason type. */
export type AnyErrorWithReasonType = ErrorWithReason<keyof ErrorReasonTypes>;
/** Sent in ResponseMessage to reconstruct the ErrorWithReason. */
export interface ErrorWithReasonDetails {
  reason: AnyErrorReasonType;
  reasonType: keyof ErrorReasonTypes;
}

// Exception information that can be passed across postMessage.
export interface GlicException extends TransferableException {
  // This may be set to indicate that the exception is a ErrorWithReason
  // exception.
  exceptionReason?: ErrorWithReasonDetails;
}

// Constructs an exception from a TransferableException.
export function exceptionFromTransferable(e: GlicException): Error|
    AnyErrorWithReasonType {
  // Error types are serializable, but they do not serialize all members.
  // If exceptionReason is provided, we use it to reconstruct a
  // ErrorWithReason by just setting additional fields after
  // serialization.
  if (e.exceptionReason !== undefined) {
    const withReason = e.exception as AnyErrorWithReasonType;
    withReason.reason = e.exceptionReason.reason;
    withReason.reasonType = e.exceptionReason.reasonType;
  }

  return e.exception;
}

// Transform an Error into a TransferableException.
export function newTransferableException(e: Error): GlicException {
  let exceptionReason = undefined;
  const maybeWithReason = e as Partial<AnyErrorWithReasonType>;
  if (maybeWithReason.reasonType !== undefined &&
      maybeWithReason.reason !== undefined) {
    exceptionReason = {
      reason: maybeWithReason.reason,
      reasonType: maybeWithReason.reasonType,
    };
  }
  return {exception: e, exceptionReason};
}

export const ERROR_CODEC: ErrorCodec = {
  serialize: newTransferableException,
  deserialize: exceptionFromTransferable,
};
