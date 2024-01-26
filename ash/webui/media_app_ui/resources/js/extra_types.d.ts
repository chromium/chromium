// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Additional chrome types used by the media app that are not yet
 * available in the TypeScript toolchain.
 */

interface LaunchParams {
  readonly files: readonly FileSystemHandle[];
}

interface LaunchQueue {
  setConsumer(consumer: (params: LaunchParams) => void): void;
}

interface FilePickerAcceptType {
  description: string;
  accept: Record<string, string|string[]>;
}

interface FilePickerOptions {
  types?: FilePickerAcceptType[];
  excludeAcceptAllOption?: boolean;
  id?: string;
  startIn?: string|FileSystemHandle;
}

interface OpenFilePickerOptions extends FilePickerOptions {
  multiple?: boolean;
}

interface SaveFilePickerOptions extends FilePickerOptions {
  suggestedName?: string;
}

type LaunchConsumer = (params: LaunchParams) => void;

interface LaunchQueue {
  setConsumer(consumer: LaunchConsumer): void;
}

interface FileSystemDirectoryHandle {
  values(): AsyncIterable<FileSystemHandle>;
}

interface Window {
  readonly launchQueue: LaunchQueue;

  // Added to window by first_message_received.js which cannot be a JS module.
  readonly firstMessageReceived: Promise<any>;

  // These are cleared in the untrusted context to prevent accidental usage.
  // (They are guaranteed to fail).
  showOpenFilePicker: null|
      ((options: OpenFilePickerOptions) => Promise<FileSystemFileHandle[]>);
  showSaveFilePicker: null|
      ((options: SaveFilePickerOptions) => Promise<FileSystemFileHandle>);
  showDirectoryPicker: null|
      ((options: FilePickerOptions) => Promise<FileSystemDirectoryHandle>);

  // Added to window by launch.js for integration testing.
  advance: (direction: number, currentFileToken?: number) => void;
}


/**
 * Types for
 * https://crsrc.org/c/chrome/common/extensions/api/crash_report_private.idl.
 */
declare namespace chrome.crashReportPrivate {
  interface ErrorInfo {
    message: string;
    url: string;
    product?: string;
    version?: string;
    lineNumber?: number;
    columnNumber?: number;
    debugId?: string;
    stackTrace?: string;
  }
  function reportError(info: ErrorInfo, callback: () => void): void;
}
