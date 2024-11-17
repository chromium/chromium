// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(pihsun): Remove this once we fully specify all the types.
/* eslint-disable @typescript-eslint/no-explicit-any */

// File System Access API: This is currently a Chrome only API, and the spec is
// still in working draft stage.
// https://wicg.github.io/file-system-access/

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

  // move() is only implemented in Chrome so it's not in upstream type
  // definitions. Ref:
  // https://chromestatus.com/feature/5640802622504960
  move(dir: FileSystemDirectoryHandle, name: string): Promise<void>;
}

interface FileSystemDirectoryHandle {
  values(): IterableIterator<FileSystemHandle>;
}

interface StorageManager {
  getDirectory(): Promise<FileSystemDirectoryHandle>;
}

// v8 specific stack information.
interface CallSite {
  getFileName(): string|undefined;
  getFunctionName(): string|undefined;
  getLineNumber(): number|undefined;
  getColumnNumber(): number|undefined;
}

// Compute Pressure API, see
// https://developer.chrome.com/docs/web-platform/compute-pressure
interface PressureObseverOptions {
  sampleInterval?: number;
}

interface PressureRecord {
  readonly source: string;
  readonly state: 'critical'|'fair'|'nominal'|'serious';
  readonly time: number;
}

type PressureObserverCallback = (records: PressureRecord[]) => void;

declare class PressureObserver {
  constructor(
      callback: PressureObserverCallback, options: PressureObseverOptions);
  observe(source: string): void;
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

// This is a builtin name.
// eslint-disable-next-line @typescript-eslint/naming-convention
interface HTMLVideoElement {
  requestVideoFrameCallback(callback: VideoFrameRequestCallback): number;
  cancelVideoFrameCallback(handle: number): undefined;
}

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

// Measure Memory API interface. This is currently only supported in
// Chromium-based browsers. https://wicg.github.io/performance-measure-memory/
interface MemoryAttributionContainer {
  id: string;
  src: string;
}

interface MemoryAttribution {
  // Container is absent if the memory attribution is for the same-origin
  // top-level realm.
  container?: MemoryAttributionContainer;
  scope: string;
  url: string;
}

interface MemoryBreakdownEntry {
  attribution: MemoryAttribution[];
  bytes: number;
  types: string[];
}

interface MemoryMeasurement {
  breakdown: MemoryBreakdownEntry[];
  bytes: number;
}

// This interface is only exposed to cross-origin-isolated Window,
// ServiceWorker, and SharedWorker.
// https://wicg.github.io/performance-measure-memory/#processing-model
interface Performance {
  measureUserAgentSpecificMemory(): Promise<MemoryMeasurement>;
}

/*
 * This is the return value for LitElement render function.
 *
 * Since the render function can return multiple different renderable types [1],
 * the type gets really complex if we explicitly list all possible types.
 * LitElement own typing use `unknown` for render return type, and upstream
 * discussion [2] also suggests using `unknown`, so we just alias the type to
 * `unknown` and don't further restrict what types can be returned by render.
 *
 * Since directly writing `unknown` as return type of the render function is
 * a bit confusing to readers, we expose a type alias here makes the code more
 * readable.
 *
 * Also see
 * https://chromium-review.googlesource.com/c/chromium/src/+/4318288/comment/c7a4600e_6ce078bc/
 *
 * [1]: https://lit.dev/docs/components/rendering/#renderable-values
 * [2]: https://github.com/lit/lit/discussions/2359
 */
type RenderResult = unknown;
