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
  types: FilePickerAcceptType[];
  excludeAcceptAllOption?: boolean;
  id?: string;
  startIn?: string|FileSystemHandle;
}

interface OpenFilePickerOptions extends FilePickerOptions {
  multiple?: boolean;
}

interface Window {
  readonly launchQueue: LaunchQueue;

  // These are cleared in the untrusted context to prevent accidental usage.
  // (They are guaranteed to fail).
  showOpenFilePicker: null|
      ((options: OpenFilePickerOptions) =>
           Promise<FileSystemFileHandle|FileSystemFileHandle[]>);
  showSaveFilePicker: null|
      ((options: FilePickerOptions) => Promise<FileSystemFileHandle>);
  showDirectoryPicker: null|
      ((options: FilePickerOptions) => Promise<FileSystemDirectoryHandle>);
}
