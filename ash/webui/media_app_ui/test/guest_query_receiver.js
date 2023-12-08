// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note we can only import from 'receiver.js': other modules are rolled-up into
// it, and already loaded.
import {TEST_ONLY} from './receiver.js';

/**
 * @typedef {{
 *     name: string,
 *     message: string,
 *     stack: string,
 * }}
 */
let GenericErrorResponse;

const {
  RenameResult,
  DELEGATE,
  assertCast,
  parentMessagePipe,
  loadFiles,
  setLoadFiles,
} = TEST_ONLY;

/**
 * The last file list loaded into the guest, updated via a spy on loadFiles().
 * TODO(b/185734620): This should be type {ReceivedFileList} but closure fails
 * to resolve it properly. See b/185734620 for details.
 */
let lastLoadedFileList = null;

/**
 * Test cases registered by GUEST_TEST.
 * @type {!Map<string, function(): !Promise<undefined>>}
 */
const guestTestCases = new Map();

/**
 * Returns the last file list passed to the guest context over the message pipe.
 * A file list can be "received" whether or not the app is loaded. If the app is
 * loaded, `loadFiles` can `await` the load, and forward that promise over the
 * message pipe back to the launchConsumer, which unit tests can `await`. But if
 * the app is not loaded (and the test cares) it must await on the effect of the
 * load as well. This is because `loadFiles` returns immediately after setting
 * the received file list on `window.customLaunchData.files`. Support either.
 * This only affects tests: `launchConsumer` cannot be awaited in the real app.
 */
function assertLastReceivedFileList() {
  if (lastLoadedFileList) {
    return lastLoadedFileList;
  }
  if (window.customLaunchData.files.length === 0) {
    throw new Error('No file list received.');
  }
  console.log('Note: app not loaded. Returning customLaunchData.files.');
  return window.customLaunchData.files;
}

/**
 * @return {!Array<!mediaApp.AbstractFile>}
 */
function assertLastReceivedFileArray() {
  const fileList = assertLastReceivedFileList();
  return Array.from({length: fileList.length}, (v, k) => fileList.item(k));
}

/**
 * @return {!mediaApp.AbstractFile}
 */
function currentFile() {
  const fileList = assertLastReceivedFileList();
  return assertCast(fileList.item(fileList.currentFileIndex));
}

/**
 * Flatten out primitives from the file object for transfer over message pipe.
 * @param {!mediaApp.AbstractFile} file
 * @return {!FileSnapshot}
 */
function flattenFile(file) {
  const hasDelete = !!file.deleteOriginalFile;
  const hasRename = !!file.renameOriginalFile;
  const lastModified = /** @type{!File} */ (file.blob).lastModified;
  const {blob, name, size, mimeType, fromClipboard, error, token} = file;
  return {
    blob,
    name,
    size,
    mimeType,
    fromClipboard,
    error,
    token,
    lastModified,
    hasDelete,
    hasRename,
  };
}

/**
 * Handlers for simple tests run in the guest that return a string result.
 * @type{!Object<string, function(!TestMessageQueryData, !Object):
 * Promise<string>>}
 */
const SIMPLE_TEST_QUERIES = {
  requestSaveFile: async (data, resultData) => {
    // Call requestSaveFile on the delegate.
    const existingFile = assertLastReceivedFileList().item(0);
    if (!existingFile) {
      return 'requestSaveFile failed, no file loaded';
    }
    const pickedFile = await DELEGATE.requestSaveFile(
        existingFile.name, existingFile.mimeType,
        data.simpleArgs ? data.simpleArgs.accept : []);
    return assertCast(pickedFile.token).toString();
  },
  getExportFile: async (data, resultData) => {
    const existingFile = assertLastReceivedFileList().item(0);
    if (!existingFile) {
      return 'getExportFile failed, no file loaded';
    }
    const pickedFile = await existingFile.getExportFile(data.simpleArgs.accept);
    return pickedFile.token.toString();
  },
  getLastFile: async (data, resultData) => {
    Object.assign(resultData, flattenFile(currentFile()));
    return resultData.name;
  },
  getAllFiles: async (data, resultData) => {
    Object.assign(resultData, assertLastReceivedFileArray().map(flattenFile));
    return `${resultData.length}`;
  },
  notifyCurrentFile: (data, resultData) => {
    DELEGATE.notifyCurrentFile(data.simpleArgs.name, data.simpleArgs.type);
  },
  openFileAtIndex: async (data, resultData) => {
    const handle = assertLastReceivedFileList().item(data.simpleArgs.index);
    const domFile = await handle.openFile();
    handle.updateFile(domFile, domFile.name);
    return 'opened and updated';
  },
  openFilesWithFilePicker: async (data, resultData) => {
    /**
     * @typedef {{
     *   acceptTypeKeys: !Array<string>,
     *   explicitToken: (number|undefined),
     *   singleFile: ?boolean,
     * }}
     */
    let Args;
    const args = /** @type {Args} */ (data.simpleArgs);
    let existingFile = assertLastReceivedFileList().item(0) || null;
    if (args.explicitToken) {
      existingFile = {token: args.explicitToken};
    }
    await assertLastReceivedFileList().openFilesWithFilePicker(
        args.acceptTypeKeys, existingFile, args.singleFile);
    return 'openFilesWithFilePicker resolved';
  },
};

/**
 * Acts on received TestMessageQueryData.
 * @param {!TestMessageQueryData} data
 * @return {!Promise<!TestMessageResponseData>}
 */
async function runTestQuery(data) {
  let result = 'no result';
  let extraResultData = {};
  if (data.testQuery) {
    const element = await waitForNode(data.testQuery, data.pathToRoot || []);
    result = element.tagName;

    if (data.property) {
      result = JSON.stringify(element[data.property]);
    } else if (data.requestFullscreen) {
      try {
        await element.requestFullscreen();
        result = 'hooray';
      } catch (/** @type {!TypeError} */ typeError) {
        result = typeError.message;
      }
    }
  } else if (data.simple !== undefined && data.simple in SIMPLE_TEST_QUERIES) {
    result = await SIMPLE_TEST_QUERIES[data.simple](data, extraResultData);
  } else if (data.navigate !== undefined) {
    // Simulate a user navigating to the next/prev file.
    if (data.navigate.direction === 'next') {
      await assertLastReceivedFileList().loadNext(data.navigate.token);
      result = 'loadNext called';
    } else if (data.navigate.direction === 'prev') {
      await assertLastReceivedFileList().loadPrev(data.navigate.token);
      result = 'loadPrev called';
    } else {
      result = 'nothing called';
    }
  } else if (data.overwriteLastFile !== undefined) {
    // Simulate a user overwriting the currently open file.
    const testBlob = new Blob([data.overwriteLastFile]);
    const file = currentFile();
    try {
      await assertCast(file.overwriteOriginal).call(file, testBlob);
      result = 'overwriteOriginal resolved';
    } catch (/** @type{!Error} */ error) {
      result = `overwriteOriginal failed Error: ${error}`;
      if (data.rethrow) {
        throw error;
      }
    }
    extraResultData = {
      receiverFileName: file.name,
      receiverErrorName: file.error,
    };
  } else if (data.deleteLastFile) {
    // Simulate a user deleting the currently open file.
    try {
      await assertCast(currentFile().deleteOriginalFile).call(currentFile());
      result = 'deleteOriginalFile resolved success';
    } catch (/** @type{!Error} */ error) {
      result = `deleteOriginalFile failed Error: ${error}`;
    }
  } else if (data.renameLastFile) {
    // Simulate a user renaming the currently open file.
    try {
      const renameResult = await assertCast(currentFile().renameOriginalFile)
                               .call(currentFile(), data.renameLastFile);
      if (renameResult === RenameResult.FILE_EXISTS) {
        result = 'renameOriginalFile resolved file exists';
      } else if (
          renameResult ===
          RenameResult.FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY) {
        result = 'renameOriginalFile resolved ' +
            'FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY';
      } else {
        result = 'renameOriginalFile resolved success';
      }
    } catch (/** @type{!Error} */ error) {
      result = `renameOriginalFile failed Error: ${error}`;
    }
  } else if (data.saveAs) {
    // Call save as on the first item in the last received file list, simulating
    // a user clicking save as in the file.
    const existingFile = assertLastReceivedFileList().item(0);
    if (!existingFile) {
      result = 'saveAs failed, no file loaded';
    } else {
      const file = currentFile();
      try {
        const token = (await DELEGATE.requestSaveFile(
                           existingFile.name, existingFile.mimeType, []))
                          .token;
        const testBlob = new Blob([data.saveAs]);
        await assertCast(file.saveAs).call(file, testBlob, assertCast(token));
        result = file.name;
        extraResultData = {blobText: await file.blob.text()};
      } catch (/** @type{!Error} */ error) {
        result = `saveAs failed Error: ${error}`;
        extraResultData = {filename: file.name};
      }
    }
  } else if (data.getFileErrors) {
    result = assertLastReceivedFileList().files.map(file => file.error).join();
  } else if (data.suppressCrashReports) {
    // TODO(b/172981864): Remove this once we stop triggering crash reports for
    // NotAFile errors.

    // Remove the implementation of reportError so test code
    // can safely check that the right errors are being thrown without
    // triggering a crash.
    if (chrome) {
      chrome.crashReportPrivate.reportError = () => {};
    }
  }

  return {testQueryResult: result, testQueryResultData: extraResultData};
}

/**
 * Acts on TestMessageRunTestCase.
 * @param {!TestMessageRunTestCase} data
 * @return {!Promise<!TestMessageResponseData>}
 */
async function runTestCase(data) {
  const testCase = guestTestCases.get(data.testCase);
  if (!testCase) {
    throw new Error(`Unknown test case: '${data.testCase}'`);
  }
  await testCase();  // Propate exceptions to the MessagePipe handler.
  return {testQueryResult: 'success'};
}

/**
 * Registers a test that runs in the guest context. To indicate failure, the
 * test throws an exception (e.g. via assertEquals).
 * @param {string} testName
 * @param {function(): !Promise<undefined>} testCase
 */
export function GUEST_TEST(testName, testCase) {
  guestTestCases.set(testName, testCase);
}

/**
 * Tells the test driver the guest test message handlers are installed. This
 * requires the test handler that receives the signal to be set up. The order
 * that this occurs can not be guaranteed. So this function retries until the
 * signal is handled, which requires the 'test-handlers-ready' handler to be
 * registered in driver.js.
 */
async function signalTestHandlersReady() {
  const EXPECTED_ERROR =
      /No handler registered for message type 'test-handlers-ready'/;
  let attempts = 10;
  while (--attempts >= 0) {
    try {
      // Try to limit log output from message pipe errors.
      await new Promise(resolve => setTimeout(resolve, 100));
      await parentMessagePipe.sendMessage('test-handlers-ready', {});
      return;
    } catch (/** @type {!GenericErrorResponse} */ e) {
      if (!EXPECTED_ERROR.test(e.message)) {
        console.error('Unexpected error in signalTestHandlersReady', e);
        return;
      }
    }
  }
  console.error('signalTestHandlersReady failed to signal.');
}

/** Installs the MessagePipe handlers for receiving test queries. */
function installTestHandlers() {
  parentMessagePipe.registerHandler('test', (data) => {
    return runTestQuery(/** @type {!TestMessageQueryData} */ (data));
  });
  // Turn off error rethrowing for tests so the test runner doesn't mark
  // our error handling tests as failed.
  parentMessagePipe.rethrowErrors = false;

  parentMessagePipe.registerHandler('run-test-case', (data) => {
    return runTestCase(/** @type{!TestMessageRunTestCase} */ (data));
  });

  parentMessagePipe.registerHandler('get-last-loaded-files', () => {
    //  Note: the `ReceivedFileList` has methods stripped since it gets sent
    //  over a pipe so just send the underlying files.
    return /** @type {!LastLoadedFilesResponse} */ (
        {fileList: assertLastReceivedFileList().files.map(flattenFile)});
  });

  // Log errors, rather than send them to console.error. This allows the error
  // handling tests to work correctly, and is also required for
  // signalTestHandlersReady() to operate without failing tests.
  parentMessagePipe.logClientError = error =>
      console.log(JSON.stringify(error));

  // Install spies.
  const realLoadFiles = loadFiles;
  /**
   * @param {*} fileList
   * @return {!Promise<undefined>}
   */
  async function watchLoadFiles(fileList) {
    lastLoadedFileList = fileList;
    return realLoadFiles(fileList);
  }
  setLoadFiles(watchLoadFiles);
  signalTestHandlersReady();
}

// Ensure content and all scripts have loaded before installing test handlers.
if (document.readyState !== 'complete') {
  window.addEventListener('load', installTestHandlers);
} else {
  installTestHandlers();
}
