// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// <reference path="media_app.d.ts" />

/**
 * @fileoverview
 * Temporary types for receiver.js until it is converted to TypeScript. Seeded
 * from generated .d.ts and updated to be compatible with media_app.d.ts.
 */

export class ReceivedFileList {
  constructor(filesMessage: LoadFilesMessage);
  currentFileIndex: number;
  length: number;
  files: ReceivedFile[];
  observers: Array<(arg0: any) => void>;
  item(index: any): ReceivedFile;
  loadNext(currentFileToken: any): Promise<void>;
  loadPrev(currentFileToken: any): Promise<void>;
  addObserver(observer: any): void;
  openFilesWithFilePicker(
      acceptTypeKeys: string[], startInFolder: AbstractFile,
      isSingleFile: boolean|null): Promise<undefined>;
  filterInPlace(filter: (arg0: AbstractFile) => boolean): void;
  addFiles(files: ReceivedFile[]): void;
}
export namespace TEST_ONLY {
  export {RenameResult};
  export {DELEGATE};
  export {parentMessagePipe};
  export {loadFiles};
  export function setLoadFiles(spy: any): void;
}
declare class ReceivedFile implements AbstractFile {
  constructor(file: FileContext);
  blob: Blob;
  name: string;
  size: number;
  mimeType: string;
  token: number;
  error: string;
  fromClipboard: boolean;
  deleteOriginalFile: () => Promise<void>;
  renameOriginalFile: (newName: any) => Promise<number>;
  isArcWritable(): Promise<boolean>;
  isBrowserWritable(): Promise<boolean>;
  editInPhotos(): Promise<void>;
  overwriteOriginal(blob: Blob): Promise<void>;
  deleteOriginalFileImpl(): Promise<number>;
  renameOriginalFileImpl(newName: string): Promise<number>;
  saveAs(blob: Blob, pickedFileToken: number): Promise<undefined>;
  getExportFile(accept: string[]): Promise<AbstractFile>;
  openFile(): Promise<File>;
  private updateFile;
}
import {LoadFilesMessage, RenameResult, FileContext} from './message_types.js';
declare const DELEGATE: ClientApiDelegate;
declare const parentMessagePipe: any;
declare function loadFiles(fileList: ReceivedFileList): Promise<undefined>;
export {};
