// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pihsun): Remove this once we fully specify all the types.
/* eslint-disable @typescript-eslint/no-explicit-any */

// ESLint doesn't like "declare class" without jsdoc.
/* eslint-disable require-jsdoc */

// File System Access API: This is currently a Chrome only API, and the spec is
// still in working draft stage.
// https://wicg.github.io/file-system-access/

type FileSystemWriteChunkType = Blob|BufferSource|string;

interface FileSystemWritableFileStream extends WritableStream {
  seek(position: number): Promise<void>;
  truncate(size: number): Promise<void>;
  write(data: FileSystemWriteChunkType): Promise<void>;
}

interface FileSystemCreateWritableOptions {
  keepExistingData?: boolean;
}

interface FileSystemFileHandle {
  createWritable(options?: FileSystemCreateWritableOptions):
      Promise<FileSystemWritableFileStream>;
}

interface FileSystemDirectoryHandle {
  values(): IterableIterator<FileSystemHandle>;
}

interface StorageManager {
  getDirectory(): Promise<FileSystemDirectoryHandle>;
}

// Chrome WebUI specific helper.
// https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/resources/js/load_time_data.js

interface Window {
  loadTimeData: {
    getBoolean(id: string): boolean,
    getString(id: string): string,
    getStringF(id: string, ...args: Array<number|string>): string,
  };
}

// v8 specific stack information.
interface CallSite {
  getFileName(): string|undefined;
  getFunctionName(): string|undefined;
  getLineNumber(): number|undefined;
  getColumnNumber(): number|undefined;
}

// v8 specific stack trace customizing, see https://v8.dev/docs/stack-trace-api.
interface ErrorConstructor {
  prepareStackTrace(error: Error, structuredStackTrace: CallSite[]): void;
}

// Chrome private API for crash report.
declare namespace chrome.crashReportPrivate {
  export interface ErrorInfo {
    message: string;
    url: string;
    columnNumber?: number;
    debugId?: string;
    lineNumber?: number;
    product?: string;
    stackTrace?: string;
    version?: string;
  }
  export const reportError: (info: ErrorInfo, callback: () => void) => void;
}

// Idle Detection API: This is currently a Chrome only API gated behind Origin
// Trial, and the spec is still in working draft stage.
// https://wicg.github.io/idle-detection/
declare class IdleDetector extends EventTarget {
  screenState: 'locked'|'unlocked';

  start: () => Promise<void>;
}

// CSS Properties and Values API Level 1: The spec is still in working draft
// stage.
// https://www.w3.org/TR/css-properties-values-api-1/
interface PropertyDefinition {
  name: string;
  inherits: boolean;
  syntax?: string;
  initialValue?: string;
}

declare namespace CSS {
  function registerProperty(definition: PropertyDefinition): void;
}

// File handling API: This is currently a Chrome only API.
// https://github.com/WICG/file-handling/blob/main/explainer.md
interface Window {
  readonly launchQueue: LaunchQueue;
}

interface LaunchQueue {
  setConsumer(consumer: LaunchConsumer): void;
}

type LaunchConsumer = (params: LaunchParams) => void;

interface LaunchParams {
  readonly files: readonly FileSystemHandle[];
}

// HTMLVideoElement.requestVideoFrameCallback, this is currently available in
// Chrome and the spec is still in draft stage.
// https://wicg.github.io/video-rvfc/
interface VideoFrameMetadata {
  expectedDisplayTime: DOMHighResTimeStamp;
  height: number;
  mediaTime: number;
  presentationTime: DOMHighResTimeStamp;
  presentedFrames: number;
  width: number;
  captureTime?: DOMHighResTimeStamp;
  processingDuration?: number;
  receiveTime?: DOMHighResTimeStamp;
  rtpTimestamp?: number;
}

interface HTMLVideoElement {
  requestVideoFrameCallback(callback: VideoFrameRequestCallback): number;
  cancelVideoFrameCallback(handle: number): undefined;
}

// Barcode Detection API, this is currently only supported in Chrome on
// ChromeOS, Android or macOS.
// https://wicg.github.io/shape-detection-api/
declare class BarcodeDetector {
  static getSupportedFormats(): Promise<BarcodeFormat[]>;
  constructor(barcodeDetectorOptions?: BarcodeDetectorOptions);
  detect(image: ImageBitmapSource): Promise<DetectedBarcode[]>;
}

interface BarcodeDetectorOptions {
  formats?: BarcodeFormat[];
}

interface DetectedBarcode {
  boundingBox: DOMRectReadOnly;
  rawValue: string;
  format: BarcodeFormat;
  cornerPoints: readonly Point2D[];
}

type BarcodeFormat =
    'aztec'|'codabar'|'code_39'|'code_93'|'code_128'|'data_matrix'|'ean_8'|
    'ean_13'|'itf'|'pdf417'|'qr_code'|'unknown'|'upc_a'|'upc_e';

// Web Workers API interface. This is included in lib.webworker.d.ts and
// available if we enable lib: ["webworker"] in tsconfig.json, but it conflicts
// with the "dom" lib that we also need. For simplicity we're providing a
// simplified interface for the only thing we've used in the
// SharedWorkerGlobalScope to satisfy TypeScript.
//
// TODO(pihsun): Consider splitting the webworker part into another
// tsconfig.json, see https://github.com/microsoft/TypeScript/issues/20595
interface SharedWorkerGlobalScope {
  onconnect?: ((this: SharedWorkerGlobalScope, ev: MessageEvent) => any)|null;
}
