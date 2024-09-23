// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {FileSnapshot, LastLoadedFilesResponse, TestMessageQueryData, TestMessageResponseData, TestMessageRunTestCase} from './driver_api.js';
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
function assertCast<A>(condition: A): NonNullable<A> {
  if (!condition) {
    throw new Error('Failed assertion');
  }
  return condition;
}

/**
 * Promise that signals the guest is ready to receive test messages (in addition
 * to messages handled by receiver.js).
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
   * @param query the querySelector to run in the guest.
   * @param property a property to request on the found element.
   * @param commands test commands to execute on the element.
   * @return JSON.stringify()'d value of the property or tagName if unspecified.
   */
  async waitForElementInGuest(
      query: string, property?: string, commands: Object = {}) {
    const message: TestMessageQueryData = {testQuery: query, property};
    await testMessageHandlersReady;
    const result: TestMessageResponseData =
        await guestMessagePipe.sendMessage('test', {...message, ...commands});
    return result.testQueryResult;
  }
}

/**
 * Runs the given `testCase` in the guest context.
 */
export async function runTestInGuest(testCase: string) {
  const message: TestMessageRunTestCase = {testCase};
  await testMessageHandlersReady;
  await guestMessagePipe.sendMessage('run-test-case', message);
}

export async function sendTestMessage(data?: Object):
    Promise<TestMessageResponseData> {
  await testMessageHandlersReady;
  return guestMessagePipe.sendMessage('test', data);
}

/**
 * Gets a concatenated list of errors on the currently loaded files. Note the
 * currently open file is always at index 0.
 */
export async function getFileErrors(): Promise<string> {
  await testMessageHandlersReady;
  const message = {getFileErrors: true};
  const response: TestMessageResponseData =
      await guestMessagePipe.sendMessage('test', message);
  return response.testQueryResult;
}

export class FakeWritableFileSink {
  writes: Array<{position: number, size?: number}> = [];
  resolveClose!: (blob: Blob) => void;
  closePromise = new Promise<Blob>(resolve => {
    this.resolveClose = resolve;
  });

  constructor(public data = new Blob()) {}

  async write(dataParam: BufferSource|Blob|string|WriteParams) {
    const position = 0;  // Assume no seeks.
    if (!dataParam) {
      this.writes.push({position, size: 0});
      return;
    }
    interface HasLengthOrSize {
      size?: number;
      length: number;
    }
    const data = dataParam as BlobPart & HasLengthOrSize;
    const dataSize = data.size === undefined ? data.length : data.size;
    this.writes.push({position, size: dataSize});
    this.data = new Blob([
      this.data.slice(0, position),
      data,
      this.data.slice(position + dataSize),
    ]);
  }
  async truncate(size: number) {
    this.data = this.data.slice(0, size);
  }
  /** Resolves the close promise. */
  async close() {
    this.resolveClose(this.data);
  }
  async seek(_offset: number) {
    throw new Error('seek() not implemented.');
  }
}

export class FakeFileSystemHandle implements FileSystemHandle {
  kind: FileSystemHandleKind = 'file';
  constructor(public name: string = 'fake_file.png') {}
  async isSameEntry(other: FileSystemHandle): Promise<boolean> {
    return this === other;
  }
}

export class FakeFileSystemFileHandle extends FakeFileSystemHandle implements
    FileSystemFileHandle {
  override kind: 'file' = 'file';
  lastWritable: FakeWritableFileSink;
  nextCreateWritableError?: DOMException|Error;

  /** Used simulate an error thrown from directory traversal. */
  errorToFireOnIterate?: null|DOMException;

  constructor(
      name = 'fake_file.png', public type: string = '',
      public lastModified: number = 0, blob: Blob = new Blob()) {
    super(name);
    this.lastWritable = new FakeWritableFileSink(blob);
  }
  async createWritable(_options: FileSystemCreateWritableOptions) {
    if (this.nextCreateWritableError) {
      throw this.nextCreateWritableError;
    }
    const sink = this.lastWritable;
    const stream = new WritableStream(sink);

    // The FileSystemWritableFileStream supports both streams and direct writes.
    // Splice on the direct writing capabilities by delegating to the sink.
    const writable = stream as FileSystemWritableFileStream;
    writable.write = (data: BufferSource|Blob|string|WriteParams) =>
        sink.write(data);
    writable.truncate = (size: number) => sink.truncate(size);
    writable.close = () => sink.close();
    return writable;
  }
  async getFile() {
    return this.getFileSync();
  }

  getFileSync() {
    return new File(
        [this.lastWritable.data], this.name,
        {type: this.type, lastModified: this.lastModified});
  }
}

export class FakeFileSystemDirectoryHandle extends FakeFileSystemHandle
    implements FileSystemDirectoryHandle {
  override kind: 'directory' = 'directory';

  /** Internal state mocking file handles in a directory handle. */
  files: FakeFileSystemFileHandle[] = [];

  /** Used to spy on the last deleted file. */
  lastDeleted?: null|FakeFileSystemFileHandle = null;

  constructor(name = 'fake-dir') {
    super(name);
  }

  /**
   * Use to populate `FileSystemFileHandle`s for tests.
   */
  addFileHandleForTest(fileHandle: FakeFileSystemFileHandle) {
    this.files.push(fileHandle);
  }
  /**
   * Helper to get all entries as File.
   */
  getFilesSync(): File[] {
    return this.files.map(f => f.getFileSync());
  }

  async getFileHandle(name: string, options?: FileSystemGetFileOptions) {
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
  async getDirectoryHandle(
      _name: string, _options?: FileSystemGetDirectoryOptions):
      Promise<FakeFileSystemDirectoryHandle> {
    throw new Error('Not implemented');
  }

  async * entries(): AsyncIterableIterator<[string, FileSystemHandle]> {
    for (const file of this.files) {
      yield [file.name, file];
    }
  }
  async * keys(): AsyncIterableIterator<string> {
    for (const file of this.files) {
      yield file.name;
    }
  }
  async * values(): AsyncIterableIterator<FileSystemHandle> {
    for (const file of this.files) {
      if (file.errorToFireOnIterate) {
        const error = file.errorToFireOnIterate;
        file.errorToFireOnIterate = null;
        throw error;
      }
      yield file;
    }
  }
  async *
      [Symbol.asyncIterator]():
          AsyncIterableIterator<[string, FileSystemHandle]> {
    for (const file of this.files) {
      if (file.errorToFireOnIterate) {
        const error = file.errorToFireOnIterate;
        file.errorToFireOnIterate = null;
        throw error;
      }
      yield [file.name, file];
    }
  }
  async removeEntry(name: string, _options: FileSystemRemoveOptions) {
    // Remove file handle from internal state.
    const fileHandleIndex = this.files.findIndex(f => f.name === name);
    // Store the file removed for spying in tests.
    this.lastDeleted = this.files.splice(fileHandleIndex, 1)[0];
  }

  resolve() {
    return Promise.resolve(null);
  }
}

/**
 * Structure to define a test file.
 */
export interface FileDesc {
  name?: string;
  type?: string;
  lastModified?: number;
  arrayBuffer?: () => Promise<ArrayBuffer>;
}

/**
 * Creates a mock directory with the provided files in it.
 */
export async function createMockTestDirectory(files: FileDesc[] = [{}]):
    Promise<FakeFileSystemDirectoryHandle> {
  const directory = new FakeFileSystemDirectoryHandle();
  for (const file of files) {
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
 */
export function handlesToLaunchParams(files: FileSystemHandle[]): LaunchParams {
  return {files};
}

/**
 * Helper to "launch" with the given `directoryContents`. Populates a fake
 * directory containing those handles, then launches the app. The focus file is
 * either the first file in `multiSelectionFiles`, or the first directory entry.
 * @param multiSelectionFiles If provided,
 *     holds additional files selected in the files app at launch time.
 */
export async function launchWithHandles(
    directoryContents: FakeFileSystemFileHandle[],
    multiSelectionFiles: FakeFileSystemFileHandle[] =
        []): Promise<FakeFileSystemDirectoryHandle> {
  await testMessageHandlersReady;

  let focusFile = multiSelectionFiles[0];
  if (!focusFile) {
    focusFile = directoryContents[0]!;
  }
  multiSelectionFiles = multiSelectionFiles.slice(1);
  const directory = new FakeFileSystemDirectoryHandle();
  for (const handle of directoryContents) {
    directory.addFileHandleForTest(handle);
  }
  const files: FileSystemHandle[] =
      [directory, focusFile, ...multiSelectionFiles];
  await launchConsumer(handlesToLaunchParams(files));
  return directory;
}

/**
 * Wraps a file in a FakeFileSystemFileHandle.
 */
export function fileToFileHandle(file: File): FakeFileSystemFileHandle {
  return new FakeFileSystemFileHandle(
      file.name, file.type, file.lastModified, file);
}

/**
 * Helper to invoke launchWithHandles after wrapping `files` in fake handles.
 */
export async function launchWithFiles(
    files: File[],
    selectedIndexes: number[] = []): Promise<FakeFileSystemDirectoryHandle> {
  const fileHandles = files.map(fileToFileHandle);
  const selection = selectedIndexes.map(i => fileHandles[i]!);
  return launchWithHandles(fileHandles, selection);
}

/**
 * Creates an `Error` with the name field set.
 */
export function createNamedError(name: string, msg: string): Error {
  const error = new Error(msg);
  error.name = name;
  return error;
}

export async function loadFilesWithoutSendingToGuest(
    directory: FileSystemDirectoryHandle, file: File) {
  const handle = await directory.getFileHandle(file.name);
  const launchNumber = incrementLaunchNumber();
  setCurrentDirectory(directory, {file, handle});
  await processOtherFilesInDirectory(directory, file, launchNumber);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of files as `expectedFiles`.
 */
export function assertFilesToBe(
    expectedFiles: Array<undefined|{name: string}>, testCase?: string) {
  assertFilenamesToBe(expectedFiles.map(f => f!.name).join(), testCase);
}

/**
 * Checks that the `currentFiles` array maintained by launch.js has the same
 * sequence of filenames as `expectedFilenames`.
 */
export function assertFilenamesToBe(
    expectedFilenames: string, testCase?: string) {
  // Use filenames as an approximation of file uniqueness.
  const currentFilenames = currentFiles.map(d => d.handle.name).join();
  assertEquals(
      expectedFilenames, currentFilenames,
      `Expected '${expectedFilenames}' but got '${currentFilenames}'` +
          (testCase ? ` for ${testCase}` : ''));
}

/**
 * Wraps `chai.assert.match` allowing tests to use `assertMatch`.
 * @param string the string to match
 * @param regex an escaped regex compatible string
 * @param message logged if the assertion fails
 */
export function assertMatch(string: string, regex: string, message?: string) {
  chai.assert.match(string, new RegExp(regex), message);
}

/**
 * Returns the files loaded in the most recent call to `loadFiles()`.
 */
export async function getLoadedFiles(): Promise<FileSnapshot[]> {
  const response: LastLoadedFilesResponse =
      await guestMessagePipe.sendMessage('get-last-loaded-files');
  if (response.fileList) {
    return response.fileList;
  }
  // No callers currently want this to return null.
  throw new Error('No last loaded files');
}

/**
 * Puts the app into valid but "unexpected" state for it to be in after handling
 * a launch. Currently this restores part of the app state to what it would be
 * on a launch from the icon (i.e. no launch files).
 */
export function simulateLosingAccessToDirectory() {
  setCurrentDirectoryHandle(null);
}

export function launchWithFocusFile(directory: FakeFileSystemDirectoryHandle):
    {handle: FakeFileSystemFileHandle, file: File} {
  const firstFile = assertCast(directory.files[0]);
  const focusFile = {
    handle: firstFile,
    file: firstFile.getFileSync(),
  };
  incrementLaunchNumber();
  setCurrentDirectory(directory, focusFile);
  return focusFile;
}

export async function assertSingleFileLaunch(
    directory: FakeFileSystemDirectoryHandle, totalFiles: number) {
  assertEquals(1, currentFiles.length);

  await sendFilesToGuest();

  const loadedFiles = await getLoadedFiles();
  // The untrusted context only loads the first file.
  assertEquals(1, loadedFiles.length);
  // All files are in the `FileSystemDirectoryHandle`.
  assertEquals(totalFiles, directory.files.length);
}

/**
 * Check files loaded in the trusted context `currentFiles` against the working
 * directory and the untrusted context.
 */
export async function assertFilesLoaded(
    directory: FakeFileSystemDirectoryHandle, fileNames: string[],
    testCase?: string) {
  assertEquals(fileNames.length, directory.files.length);
  assertEquals(fileNames.length, currentFiles.length);

  const loadedFiles = await getLoadedFiles();
  assertEquals(fileNames.length, loadedFiles.length);

  // Check `currentFiles` in the trusted context matches up with files sent
  // to guest.
  assertFilenamesToBe(fileNames.join(), testCase);
  assertFilesToBe(loadedFiles, testCase);
}
