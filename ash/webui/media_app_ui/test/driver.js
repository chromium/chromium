// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TEST_ONLY} from './launch.js';

const {
  guestMessagePipe,
  launchConsumer,
  processOtherFilesInDirectory,
  currentFiles,
  sendFilesToGuest,
  setCurrentDirectory,
  incrementLaunchNumber,
  setCurrentDirectoryHandle,
} = TEST_ONLY;

// See message_pipe.js.
function assertCast(condition) {
  if (!condition) {
    throw new Error('Failed assertion');
  }
  return condition;
}

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
 * @type {!Promise<undefined>}
 */
const testMessageHandlersReady = new Promise(resolve => {
  guestMessagePipe.registerHandler('test-handlers-ready', resolve);
});

/** Host-side of web-driver like controller for sandboxed guest frames. */
export class GuestDriver {
  /**
   * Sends a query to the guest that repeatedly runs a query selector until
   * it returns an element.
   *
   * @param {string} query the querySelector to run in the guest.
   * @param {string=} opt_property a property to request on the found element.
   * @param {!Object=} opt_commands test commands to execute on the element.
   * @return Promise<string> JSON.stringify()'d value of the property, or
   *   tagName if unspecified.
   */
  async waitForElementInGuest(query, opt_property, opt_commands = {}) {
    /** @type {!TestMessageQueryData} */
    const message = {testQuery: query, property: opt_property};
    await testMessageHandlersReady;
    const result = /** @type {!TestMessageResponseData} */ (
        await guestMessagePipe.sendMessage(
            'test', {...message, ...opt_commands}));
    return result.testQueryResult;
  }
}

/**
 * Runs the given `testCase` in the guest context.
 * @param {string} testCase
 */
export async function runTestInGuest(testCase) {
  /** @type {!TestMessageRunTestCase} */
  const message = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}

/**
 * @param {!Object=} data
 * @return {!Promise<!TestMessageResponseData>}
 */
export async function sendTestMessage(data = undefined) {
  await testMessageHandlersReady;
  return /** @type {!Promise<!TestMessageResponseData>} */ (
      guestMessagePipe.sendMessage('test', data));
}

/**
 * Gets a concatenated list of errors on the currently loaded files. Note the
 * currently open file is always at index 0.
 * @return {!Promise<string>}
 */
export async function getFileErrors() {
  await testMessageHandlersReady;
  const message = {getFileErrors: true};
  const response = /** @type {!TestMessageResponseData} */ (
      await guestMessagePipe.sendMessage('test', message));
  return response.testQueryResult;
}

export class FakeWritableFileSink {
  constructor(/** !Blob= */ data = new Blob()) {
    this.data = data;

    /** @type {!Array<{position: number, size: (number|undefined)}>} */
    this.writes = [];

    /** @type {function(!Blob)} */
    this.resolveClose;

    this.closePromise = new Promise((/** function(!Blob) */ resolve) => {
      this.resolveClose = resolve;
    });
  }
  /** @param {?BufferSource|!Blob|string|!WriteParams} data */
  async write(data) {
    const position = 0;  // Assume no seeks.
    if (!data) {
      this.writes.push({position, size: 0});
      return;
    }
    const dataSize = data.size === undefined ? data.length : data.size;
    this.writes.push({position, size: /** @type {number} */ (dataSize)});
    this.data = new Blob([
      this.data.slice(0, position),
      data,
      this.data.slice(position + dataSize),
    ]);
  }
  /** @param {number} size */
  async truncate(size) {
    this.data = this.data.slice(0, size);
  }
  /** Resolves the close promise. */
  async close() {
    this.resolveClose(this.data);
  }
  /** @param {number} offset */
  async seek(offset) {
    throw new Error('seek() not implemented.');
  }
}

/** @implements FileSystemHandle  */
export class FakeFileSystemHandle {
  /**
   * @param {string=} name
   */
  constructor(name = 'fake_file.png') {
    this.kind = 'file';
    this.name = name;
  }
  /** @override */
  async isSameEntry(other) {
    return this === other;
  }
  /** @override */
  async queryPermission(descriptor) {}
  /** @override */
  async requestPermission(descriptor) {}
}

/** @implements FileSystemFileHandle  */
export class FakeFileSystemFileHandle extends FakeFileSystemHandle {
  /**
   * @param {string=} name
   * @param {string=} type
   * @param {number=} lastModified
   * @param {!Blob=} blob
   */
  constructor(
      name = 'fake_file.png', type = '', lastModified = 0, blob = new Blob()) {
    super(name);
    this.lastWritable = new FakeWritableFileSink(blob);

    /** @type {string} */
    this.type = type;

    /** @type {number} */
    this.lastModified = lastModified;

    /** @type {!DOMException|!Error|undefined} */
    this.nextCreateWritableError;

    /**
     * Used simulate an error thrown from directory traversal.
     * @type {?DOMException|undefined}
     */
    this.errorToFireOnIterate;
  }
  /** @override */
  async createWritable(options) {
    if (this.nextCreateWritableError) {
      throw this.nextCreateWritableError;
    }
    const sink = this.lastWritable;
    const stream = new WritableStream(sink);

    // The FileSystemWritableFileStream supports both streams and direct writes.
    // Splice on the direct writing capabilities by delegating to the sink.
    const writable = /** @type {!FileSystemWritableFileStream} */ (stream);
    writable.write = (data) => sink.write(data);
    writable.truncate = (size) => sink.truncate(size);
    writable.close = () => sink.close();
    return writable;
  }
  /** @override */
  async getFile() {
    return this.getFileSync();
  }

  /** @return {!File} */
  getFileSync() {
    return new File(
        [this.lastWritable.data], this.name,
        {type: this.type, lastModified: this.lastModified});
  }
}

/** @implements FileSystemDirectoryHandle  */
export class FakeFileSystemDirectoryHandle extends FakeFileSystemHandle {
  /**
   * @param {string=} name
   */
  constructor(name = 'fake-dir') {
    super(name);
    this.kind = 'directory';
    /**
     * Internal state mocking file handles in a directory handle.
     * @type {!Array<!FakeFileSystemFileHandle>}
     */
    this.files = [];
    /**
     * Used to spy on the last deleted file.
     * @type {?FakeFileSystemFileHandle}
     */
    this.lastDeleted = null;
  }
  /**
   * Use to populate `FileSystemFileHandle`s for tests.
   * @param {!FakeFileSystemFileHandle} fileHandle
   */
  addFileHandleForTest(fileHandle) {
    this.files.push(fileHandle);
  }
  /**
   * Helper to get all entries as File.
   * @return {!Array<!File>}
   */
  getFilesSync() {
    return this.files.map(f => f.getFileSync());
  }
  /** @override */
  async getFileHandle(name, options) {
    const fileHandle = this.files.find(f => f.name === name);
    if (!fileHandle && options && options.create === true) {
      // Simulate creating a new file, assume it is an image. This is needed for
      // renaming files to ensure it has the right mime type, the real
      // implementation copies the mime type from the binary.
      const newFileHandle = new FakeFileSystemFileHandle(name, 'image/png');
      this.files.push(newFileHandle);
      return Promise.resolve(newFileHandle);
    }
    return fileHandle ? Promise.resolve(fileHandle) :
                        Promise.reject((createNamedError(
                            'NotFoundError', `File ${name} not found`)));
  }
  /** @override */
  getDirectoryHandle(name, options) {}
  /**
   * @override
   * @return {!AsyncIterable<!Array<string|!FileSystemHandle>>}
   * @suppress {reportUnknownTypes} suppress [JSC_UNKNOWN_EXPR_TYPE] for `yield
   * [file.name, file]`.
   */
  async * entries() {
    for (const file of this.files) {
      yield [file.name, file];
    }
  }
  /**
   * @override
   * @return {!AsyncIterable<string>}
   * @suppress {reportUnknownTypes} suppress [JSC_UNKNOWN_EXPR_TYPE] for `yield
   * file.name`.
   */
  async * keys() {
    for (const file of this.files) {
      yield file.name;
    }
  }
  /**
   * @override
   * @return {!AsyncIterable<!FileSystemHandle>}
   * @suppress {reportUnknownTypes} suppress [JSC_UNKNOWN_EXPR_TYPE] for `yield
   * file`.
   */
  async * values() {
    for (const file of this.files) {
      if (file.errorToFireOnIterate) {
        const error = file.errorToFireOnIterate;
        file.errorToFireOnIterate = null;
        throw error;
      }
      yield file;
    }
  }
  /** @override */
  async removeEntry(name, options) {
    // Remove file handle from internal state.
    const fileHandleIndex = this.files.findIndex(f => f.name === name);
    // Store the file removed for spying in tests.
    this.lastDeleted = this.files.splice(fileHandleIndex, 1)[0];
  }

  /** @override */
  resolve() {
    return Promise.resolve(null);
  }
}

/**
 * Structure to define a test file.
 * @typedef{{
 *   name: (string|undefined),
 *   type: (string|undefined),
 *   lastModified: (number|undefined),
 *   arrayBuffer: (function(): (!Promise<!ArrayBuffer>)|undefined)
 * }}
 */
export let FileDesc;

/**
 * Creates a mock directory with the provided files in it.
 * @param {!Array<!FileDesc>=} files
 * @return {!Promise<!FakeFileSystemDirectoryHandle>}
 */
export async function createMockTestDirectory(files = [{}]) {
  const directory = new FakeFileSystemDirectoryHandle();
  for (const /** !FileDesc */ file of files) {
    const fileBlob = file.arrayBuffer !== undefined ?
        new Blob([await file.arrayBuffer()]) :
        new Blob();
    directory.addFileHandleForTest(new FakeFileSystemFileHandle(
        file.name, file.type, file.lastModified, fileBlob));
  }
  return directory;
}

/**
 * Creates a mock LaunchParams object from the provided `files`.
 * @param {!Array<!FileSystemHandle>} files
 * @return {!LaunchParams}
 */
export function handlesToLaunchParams(files) {
  return /** @type{!LaunchParams} */ ({files});
}

/**
 * Helper to "launch" with the given `directoryContents`. Populates a fake
 * directory containing those handles, then launches the app. The focus file is
 * either the first file in `multiSelectionFiles`, or the first directory entry.
 * @param {!Array<!FakeFileSystemFileHandle>} directoryContents
 * @param {!Array<!FakeFileSystemFileHandle>=} multiSelectionFiles If provided,
 *     holds additional files selected in the files app at launch time.
 * @return {!Promise<!FakeFileSystemDirectoryHandle>}
 */
export async function launchWithHandles(
    directoryContents, multiSelectionFiles = []) {
  await testMessageHandlersReady;

  /** @type {?FakeFileSystemFileHandle} */
  let focusFile = multiSelectionFiles[0];
  if (!focusFile) {
    focusFile = directoryContents[0];
  }
  multiSelectionFiles = multiSelectionFiles.slice(1);
  const directory = new FakeFileSystemDirectoryHandle();
  for (const handle of directoryContents) {
    directory.addFileHandleForTest(handle);
  }
  const files = [directory, focusFile, ...multiSelectionFiles];
  await launchConsumer(handlesToLaunchParams(files));
  return directory;
}

/**
 * Wraps a file in a FakeFileSystemFileHandle.
 * @param {!File} file
 * @return {!FakeFileSystemFileHandle}
 */
export function fileToFileHandle(file) {
  return new FakeFileSystemFileHandle(
      file.name, file.type, file.lastModified, file);
}

/**
 * Helper to invoke launchWithHandles after wrapping `files` in fake handles.
 * @param {!Array<!File>} files
 * @param {!Array<number>=} selectedIndexes
 * @return {!Promise<!FakeFileSystemDirectoryHandle>}
 */
export async function launchWithFiles(files, selectedIndexes = []) {
  const fileHandles = files.map(fileToFileHandle);
  const selection =
      selectedIndexes.map((/** @type {number} */ i) => fileHandles[i]);
  return launchWithHandles(fileHandles, selection);
}

/**
 * Creates an `Error` with the name field set.
 * @param {string} name
 * @param {string} msg
 * @return {!Error}
 */
export function createNamedError(name, msg) {
  const error = new Error(msg);
  error.name = name;
  return error;
}

/**
 * @param {!FileSystemDirectoryHandle} directory
 * @param {!File} file
 */
export async function loadFilesWithoutSendingToGuest(directory, file) {
  const handle = await directory.getFileHandle(file.name);
  const launchNumber = incrementLaunchNumber();
  setCurrentDirectory(directory, {file, handle});
  await processOtherFilesInDirectory(directory, file, launchNumber);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of files as `expectedFiles`.
 * @param {!Array<!File>} expectedFiles
 * @param {string=} testCase
 */
export function assertFilesToBe(expectedFiles, testCase = undefined) {
  assertFilenamesToBe(expectedFiles.map(f => f.name).join(), testCase);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of filenames as `expectedFilenames`.
 * @param {string} expectedFilenames
 * @param {string=} testCase
 */
export function assertFilenamesToBe(expectedFilenames, testCase = undefined) {
  // Use filenames as an approximation of file uniqueness.
  const currentFilenames = currentFiles.map(d => d.handle.name).join();
  chai.assert.equal(
      currentFilenames, expectedFilenames,
      `Expected '${expectedFilenames}' but got '${currentFilenames}'` +
          (testCase ? ` for ${testCase}` : ''));
}

/**
 * Wraps `chai.assert.match` allowing tests to use `assertMatch`.
 * @param {string} string the string to match
 * @param {string} regex an escaped regex compatible string
 * @param {string=} opt_message logged if the assertion fails
 */
export function assertMatch(string, regex, opt_message = undefined) {
  chai.assert.match(string, new RegExp(regex), opt_message);
}

/**
 * Returns the files loaded in the most recent call to `loadFiles()`.
 * @return {!Promise<?Array<!FileSnapshot>>}
 */
export async function getLoadedFiles() {
  const response = /** @type {!LastLoadedFilesResponse} */ (
      await guestMessagePipe.sendMessage('get-last-loaded-files'));
  if (response.fileList) {
    return response.fileList;
  }
  return null;
}

/**
 * Puts the app into valid but "unexpected" state for it to be in after handling
 * a launch. Currently this restores part of the app state to what it would be
 * on a launch from the icon (i.e. no launch files).
 */
export function simulateLosingAccessToDirectory() {
  setCurrentDirectoryHandle(null);
}

/**
 * @param {!FakeFileSystemDirectoryHandle} directory
 * @return {{handle: !FakeFileSystemFileHandle, file: !File}}
 */
export function launchWithFocusFile(directory) {
  const focusFile = {
    /** @type {!FakeFileSystemFileHandle} */
    handle: directory.files[0],
    file: directory.files[0].getFileSync(),
  };
  incrementLaunchNumber();
  setCurrentDirectory(directory, focusFile);
  return focusFile;
}

/**
 * @param {!FakeFileSystemDirectoryHandle} directory
 * @param {number} totalFiles
 */
export async function assertSingleFileLaunch(directory, totalFiles) {
  chai.assert.equal(1, currentFiles.length);

  await sendFilesToGuest();

  const loadedFiles = assertCast(await getLoadedFiles());
  // The untrusted context only loads the first file.
  chai.assert.equal(1, loadedFiles.length);
  // All files are in the `FileSystemDirectoryHandle`.
  chai.assert.equal(totalFiles, directory.files.length);
}

/**
 * Check files loaded in the trusted context `currentFiles` against the working
 * directory and the untrusted context.
 * @param {!FakeFileSystemDirectoryHandle} directory
 * @param {!Array<string>} fileNames
 * @param {string=} testCase
 */
export async function assertFilesLoaded(
    directory, fileNames, testCase = undefined) {
  chai.assert.equal(fileNames.length, directory.files.length);
  chai.assert.equal(fileNames.length, currentFiles.length);

  const loadedFiles = /** @type {!Array<!File>} */ (await getLoadedFiles());
  chai.assert.equal(fileNames.length, loadedFiles.length);

  // Check `currentFiles` in the trusted context matches up with files sent
  // to guest.
  assertFilenamesToBe(fileNames.join(), testCase);
  assertFilesToBe(loadedFiles, testCase);
}
