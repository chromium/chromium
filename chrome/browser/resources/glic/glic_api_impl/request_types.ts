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
