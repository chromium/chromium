// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Reply to test messages. Contents depend on the test message sent.
 * @typedef {{
 *     testQueryResult: string,
 *     testQueryResultData: (!Object|undefined)
 * }}
 */
let TestMessageResponseData;

/**
 * Object sent over postMessage to run a command or extract data.
 * @typedef {{
 *     deleteLastFile: (boolean|undefined),
 *     getFileErrors: (boolean|undefined),
 *     navigate: ({direction: string, token: number}|undefined),
 *     openFile: (boolean|undefined),
 *     overwriteLastFile: (string|undefined),
 *     rethrow: (boolean|undefined),
 *     pathToRoot: (!Array<string>|undefined),
 *     property: (string|undefined),
 *     renameLastFile: (string|undefined),
 *     requestFullscreen: (boolean|undefined),
 *     requestSaveFile: (boolean|undefined),
 *     saveAs: (string|undefined),
 *     simple: (string|undefined),
 *     simpleArgs: (Object|undefined),
 *     suppressCrashReports: (boolean|undefined),
 *     testQuery: string,
 * }}
 */
let TestMessageQueryData;

/** @typedef {{testCase: string}} */
let TestMessageRunTestCase;

/**
 * Subset of mediaApp.AbstractFile that can be serialized. The fields
 * `hasDelete` and `hasRename` indicate whether the methods are defined.
 * @typedef {{
 *    blob: !Blob,
 *    name: string,
 *    size: number,
 *    mimeType: string,
 *    fromClipboard: (boolean|undefined),
 *    error: (string|undefined),
 *    token: (number|undefined),
 *    lastModified: number,
 *    hasDelete: boolean,
 *    hasRename: boolean,
 * }}
 */
let FileSnapshot;

/**
 * Return type of `get-last-loaded-files` used to spy on the files sent to the
 * guest app using `loadFiles()`.
 * @typedef {{fileList: ?Array<!FileSnapshot>}}
 */
let LastLoadedFilesResponse;
