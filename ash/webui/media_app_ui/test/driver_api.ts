// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Reply to test messages. Contents depend on the test message sent.
 */
export interface TestMessageResponseData {
  testQueryResult: string;
  testQueryResultData?: any;
}

/**
 * Object sent over postMessage to run a command or extract data.
 */
export interface TestMessageQueryData {
  deleteLastFile?: boolean;
  getFileErrors?: boolean;
  navigate?: {direction?: string, token?: number};
  openFile?: boolean;
  overwriteLastFile?: string;
  rethrow?: boolean;
  pathToRoot?: string[];
  property?: string;
  renameLastFile?: string;
  requestFullscreen?: boolean;
  requestSaveFile?: boolean;
  saveAs?: string;
  simple?: string;
  simpleArgs?: any;
  suppressCrashReports?: boolean;
  testQuery: string;
}

export interface TestMessageRunTestCase {
  testCase: string;
}

/**
 * Subset of mediaApp.AbstractFile that can be serialized. The fields
 * `hasDelete` and `hasRename` indicate whether the methods are defined.
 */
export interface FileSnapshot {
  blob: Blob;
  name: string;
  size: number;
  mimeType: string;
  fromClipboard?: boolean;
  error?: string;
  token?: number;
  lastModified: number;
  hasDelete: boolean;
  hasRename: boolean;
}

/**
 * Return type of `get-last-loaded-files` used to spy on the files sent to the
 * guest app using `loadFiles()`.
 */
export interface LastLoadedFilesResponse {
  fileList: FileSnapshot[];
}
