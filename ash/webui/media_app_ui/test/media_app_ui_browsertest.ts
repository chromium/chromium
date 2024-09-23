// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals, assertEquals, assertGE, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

import {assertFilenamesToBe, assertFilesLoaded, assertFilesToBe, assertMatch, assertSingleFileLaunch, createMockTestDirectory, FakeFileSystemFileHandle, fileToFileHandle, getFileErrors, getLoadedFiles, GuestDriver, launchWithFiles, launchWithFocusFile, launchWithHandles, loadFilesWithoutSendingToGuest, runTestInGuest, sendTestMessage, simulateLosingAccessToDirectory} from './driver.js';
import {FileSnapshot} from './driver_api.js';
import {TEST_ONLY} from './launch.js';
import type {LoadFilesMessage} from './message_types.js';

const {
  Message,
  SortOrder,
  advance,
  currentFiles,
  fileHandleForToken,
  guestMessagePipe,
  launchWithDirectory,
  loadOtherRelatedFiles,
  pickWritableFile,
  tokenGenerator,
  tokenMap,
  error_reporter,
  mediaAppPageHandler,
  setSortOrder,
  setEntryIndex,
  getEntryIndex,
  getGlobalLaunchNumber,
} = TEST_ONLY;
const {captureConsoleErrors, reportCrashError} = error_reporter.TEST_ONLY;

const HOST_ORIGIN = 'chrome://media-app';
const GUEST_ORIGIN = 'chrome-untrusted://media-app';

/**
 * Regex to match against text of a "generic" error. This just checks for
 * something message-like (a string with at least one letter). Note we can't
 * update error messages in the same patch as this test currently. See
 * https://crbug.com/1080473.
 */
const GENERIC_ERROR_MESSAGE_REGEX = '^".*[A-Za-z].*"$';

const driver = new GuestDriver();

/**
 * Runs a CSS selector until it detects the "error" UX being loaded.
 * @return alt= text of the element showing the error.
 */
function waitForErrorUX(): Promise<string> {
  const ERROR_UX_SELECTOR = 'img[alt^="Unable to decode"]';
  return driver.waitForElementInGuest(ERROR_UX_SELECTOR, 'alt');
}

/**
 * Runs a CSS selector that waits for an image to load with the given alt= text
 * and returns its width.
 * @return The value of the width attribute.
 */
function waitForImageAndGetWidth(altText: string): Promise<string> {
  return driver.waitForElementInGuest(`img[alt="${altText}"]`, 'naturalWidth');
}

// Give the test image an unusual size so we can distinguish it form other <img>
// elements that may appear in the guest.
const TEST_IMAGE_WIDTH = 123;
const TEST_IMAGE_HEIGHT = 456;

/**
 * Returns A {width}x{height} transparent encoded image/png.
 */
async function createTestImageFile(
    width = TEST_IMAGE_WIDTH, height = TEST_IMAGE_HEIGHT,
    name = 'test_file.png', lastModified = 0): Promise<File> {
  const canvas = new OffscreenCanvas(width, height);
  canvas.getContext('2d');  // convertToBlob fails without a rendering context.
  const blob = await canvas.convertToBlob();
  return new File([blob], name, {type: 'image/png', lastModified});
}

async function createMultipleImageFiles(filenames: unknown[]): Promise<File[]> {
  const filePromise = (name: unknown) =>
      createTestImageFile(1, 1, `${name}.png`);
  const files = await Promise.all(filenames.map(filePromise));
  return files;
}

function queryIFrame() {
  return document.querySelector('iframe')!;
}

function getTitle() {
  return document.querySelector('title')!;
}

function getIcon() {
  return document.querySelector<HTMLLinkElement>('link[rel=icon]')!;
}

/**
 * Sets up a FakeFileSystemFileHandle to behave like a file which has been
 * deleted or moved to a directory to which we do not have access.
 */
function makeFileNotFound(handle?: FakeFileSystemFileHandle) {
  // Mimic the exception that would be thrown when attempting to call getFile on
  // a file which has been moved or deleted.
  handle!.getFileSync = () => {
    throw new DOMException('File not found', 'NotFoundError');
  };
}

interface TestSuite {
  [testName: string]: () => unknown;
  runTestInGuest: (testName?: string) => unknown;
}

const MediaAppUIBrowserTest: TestSuite = {
  // runTestInGuest takes a compulsory string arg, which isn't compatible with
  // the TestSuite index signature, so cast it here.
  runTestInGuest: runTestInGuest as () => unknown,
};

// Expose an export for tests run through `isolatedTestRunner`.
(window as unknown as {MediaAppUiBrowserTest: {}})['MediaAppUiBrowserTest'] =
    MediaAppUIBrowserTest;

// Tests that chrome://media-app is allowed to frame
// chrome-untrusted://media-app. The URL is set in the html. If that URL can't
// load, test this fails like JS ERROR: "Refused to frame '...' because it
// violates the following Content Security Policy directive: "frame-src
// chrome-untrusted://media-app/". This test also fails if the guest renderer is
// terminated, e.g., due to webui performing bad IPC such as network requests
// (failure detected in content/public/test/no_renderer_crashes_assertion.cc).
MediaAppUIBrowserTest['GuestCanLoad'] = async () => {
  const guest = queryIFrame();
  const app = await driver.waitForElementInGuest('backlight-app', 'tagName');

  assertEquals(document.location.origin, HOST_ORIGIN);
  assertEquals(guest.src, GUEST_ORIGIN + '/app.html');
  assertEquals(app, '"BACKLIGHT-APP"');
};

// Tests that we have localized information in the HTML like title and lang.
MediaAppUIBrowserTest['HasTitleAndLang'] = async () => {
  assertEquals(document.documentElement.lang, 'en');
  assertEquals(document.title, 'Gallery');
};

// Tests that regular launch for an image succeeds.
MediaAppUIBrowserTest['LaunchFile'] = async () => {
  await launchWithFiles([await createTestImageFile()]);
  const result =
      await driver.waitForElementInGuest('img[src^="blob:"]', 'naturalWidth');
  const receivedFiles = await getLoadedFiles();
  const file = receivedFiles[0]!;

  assertEquals(`${TEST_IMAGE_WIDTH}`, result);
  assertEquals(currentFiles.length, 1);
  assertEquals(await getFileErrors(), '');
  assertEquals(receivedFiles.length, 1);
  assertEquals(file.name, 'test_file.png');
  assertEquals(file.hasDelete, true);
  assertEquals(file.hasRename, true);
};

// Tests that console.error()s in the trusted context are sent to the crash
// reporter. This is also useful to ensure when multiple arguments are provided
// to console.error, the error message is built up by appending all arguments to
// the first arguments.
// Note: unhandledrejection & onerror tests throw JS Errors regardless and are
// tested in media_app_integration_browsertest.cc.
MediaAppUIBrowserTest['ReportsErrorsFromTrustedContext'] = async () => {
  const originalConsoleError = console.error;
  // chrome.crashReportPrivate.ErrorInfo.
  interface ErrorInfo {
    message: string;
    stackTrace?: string;
  }
  const reportedErrors: ErrorInfo[] = [];

  /**
   * In tests stub out `chrome.crashReportPrivate.reportError`, check
   *`reportedErrors` to make sure they are "sent" to the crash reporter.
   */
  function suppressConsoleErrorsForErrorTesting() {
    (window as any as {chrome: any}).chrome.crashReportPrivate.reportError =
        function(e: ErrorInfo) {
      // Everything should have a non-empty stack.
      assertEquals(!!e.stackTrace, true);
      reportedErrors.push(e);
    };
    // Set `realConsoleError` in `captureConsoleErrors` to console.log to
    // prevent console.error crashing tests.
    captureConsoleErrors(console.log, reportCrashError);
  }

  suppressConsoleErrorsForErrorTesting();

  assertEquals(0, reportedErrors.length);

  const error = new Error('yikes message');
  error.name = 'yikes error';
  const extraData = {b: 'b'};

  const loop: {loop?: {}} = {};
  loop.loop = loop;
  class MySpecialException {
    aLoop = loop;
  }

  console.error('a');
  console.error(error);
  console.error('b', extraData);
  console.error(extraData, extraData, extraData);
  console.error(error, 'foo', extraData, {e: error});
  console.error(new MySpecialException(), new MySpecialException());
  console.error(1, 2, 3, 4, 5);
  console.error(null, null, null);

  assertEquals(8, reportedErrors.length);
  // Test handles console.error(string).
  assertEquals('Unexpected: "a", (from console)', reportedErrors[0]!.message);
  // Test handles console.error(Error).
  assertEquals(
      'Error: [yikes error] yikes message, (from console)',
      reportedErrors[1]!.message);
  // Test handles console.error(string, Object).
  assertEquals(
      'Unexpected: "b"\n{"b":"b"}, (from console)', reportedErrors[2]!.message);
  // Test handles console.error(Object, Object, Object).
  assertEquals(
      'Object: Unexpected: {"b":"b"}\n{"b":"b"}\n{"b":"b"}, (from console)',
      reportedErrors[3]!.message);
  // Test handles console.error(string, Object, Error, Object).
  assertEquals(
      'Error: [yikes error] yikes message, foo\n{"b":"b"}\n' +
          '{"e":{"name":"yikes error"}}, (from console)',
      reportedErrors[4]!.message);
  // Test arbitrary classes.
  assertEquals(
      'MySpecialException: Unexpected: <object loop?><object loop?>, ' +
          '(from console)',
      reportedErrors[5]!.message);
  // Test non-objects.
  assertEquals(
      'Unexpected: 1\n2\n3\n4\n5, (from console)', reportedErrors[6]!.message);
  assertEquals(
      'Unexpected: null\nnull\nnull, (from console)',
      reportedErrors[7]!.message);

  // Note: This is not needed i.e. tests pass without this but it is good
  // practice to reset it since we stub it out for this test.
  console.error = originalConsoleError;
};

// Tests that we can launch the MediaApp with the selected (first) file,
// interact with it by invoking IPC (deletion) that doesn't re-launch the
// MediaApp i.e. doesn't call `launchWithDirectory`, then the rest of the files
// in the current directory are loaded in.
MediaAppUIBrowserTest['NonLaunchableIpcAfterFastLoad'] = async () => {
  setSortOrder(SortOrder.A_FIRST);
  const files =
      await createMultipleImageFiles(['file1', 'file2', 'file3', 'file4']);
  const directory = await createMockTestDirectory(files);

  // Emulate steps in `launchWithDirectory()` by launching with the first
  // file.
  const focusFile = launchWithFocusFile(directory);

  await assertSingleFileLaunch(directory, files.length);

  // Invoke Deletion IPC that doesn't relaunch the app.
  const messageDelete = {deleteLastFile: true};
  const testResponse = await sendTestMessage(messageDelete);
  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);

  // File removed from `FileSystemDirectoryHandle` internal state.
  assertEquals(3, directory.files.length);
  // Deletion results reloading the app with `currentFiles`, in this case
  // nothing.
  const lastLoadedFiles = await getLoadedFiles();
  assertEquals(0, lastLoadedFiles.length);

  // Load all other files in the `FileSystemDirectoryHandle`.
  await loadOtherRelatedFiles(directory, focusFile.file, focusFile.handle, 0);

  await assertFilesLoaded(
      directory, ['file2.png', 'file3.png', 'file4.png'],
      'fast files: check files after deletion');
};

// Tests that we can launch the MediaApp with the selected (first) file,
// and re-launch it before all files from the first launch are loaded in.
MediaAppUIBrowserTest['ReLaunchableAfterFastLoad'] = async () => {
  setSortOrder(SortOrder.A_FIRST);
  const files =
      await createMultipleImageFiles(['file1', 'file2', 'file3', 'file4']);
  const directory = await createMockTestDirectory(files);

  // Emulate steps in `launchWithDirectory()` by launching with the first
  // file.
  const focusFile = launchWithFocusFile(directory);

  // `globalLaunchNumber` starts at -1, ensure first launch increments it.
  assertEquals(0, getGlobalLaunchNumber());

  await assertSingleFileLaunch(directory, files.length);

  // Mutate the second file.
  directory.files[1]!.name = 'changed.png';
  // Relaunch the app with the second file.
  await launchWithDirectory(directory, directory.files[1]!);

  // Ensure second launch incremented the `globalLaunchNumber`.
  assertEquals(1, getGlobalLaunchNumber());

  // Second launch loads other files into `currentFiles`.
  await assertFilesLoaded(
      directory, ['changed.png', 'file1.png', 'file3.png', 'file4.png'],
      'fast files: check files after relaunching');
  const currentFilesAfterSecondLaunch = [...currentFiles];
  const loadedFilesSecondLaunch = await getLoadedFiles();

  // Try to load with previous launch number simulating the first launch
  // completing after the second launch. Has no effect as it is aborted early
  // due to different launch numbers.
  const previousLaunchNumber = 0;
  await loadOtherRelatedFiles(
      directory, focusFile.file, focusFile.handle, previousLaunchNumber);

  // Ensure `currentFiles is the same as the file state at the end of the second
  // launch before the call to `loadOtherRelatedFiles()`.
  currentFilesAfterSecondLaunch.map(
      (fd, index) => assertEquals(
          fd, currentFiles[index],
          `Equality check for file ${
              JSON.stringify(fd)} in currentFiles filed`));

  // Focus file (file that the directory was launched with) stays index 0.
  const lastLoadedFiles = await getLoadedFiles();
  assertEquals('changed.png', lastLoadedFiles[0]!.name);
  assertEquals(loadedFilesSecondLaunch[0]!.name, lastLoadedFiles[0]!.name);
  // Focus file in the `FileSystemDirectoryHandle` is at index 1.
  assertEquals(directory.files[1]!.name, lastLoadedFiles[0]!.name);
};

// Tests that a regular
//  launch for multiple images succeeds, and the files get
// distinct token mappings.
MediaAppUIBrowserTest['MultipleFilesHaveTokens'] = async () => {
  const directory = await launchWithFiles([
    await createTestImageFile(1, 1, 'file1.png'),
    await createTestImageFile(1, 1, 'file2.png'),
  ]);

  assertEquals(currentFiles.length, 2);
  assertGE(currentFiles[0]!.token, 0);
  assertGE(currentFiles[1]!.token, 0);
  assertNotEquals(currentFiles[0]!.token, currentFiles[1]!.token);
  assertEquals(fileHandleForToken(currentFiles[0]!.token), directory.files[0]);
  assertEquals(fileHandleForToken(currentFiles[1]!.token), directory.files[1]);
};

// Tests that a launch with a single audio file selected in the files app loads
// only that audio file and not the directory.
MediaAppUIBrowserTest['SingleAudioLaunch'] = async () => {
  await launchWithFiles([
    // Zero-byte audio. It won't load, but should still be added to DOM.
    new File([], 'audio1.wav', {type: 'audio/wav'}),
    new File([], 'audio2.wav', {type: 'audio/wav'}),
  ]);

  assertFilenamesToBe('audio1.wav');
};

// Tests that a launch with multiple files selected in the files app loads only
// the files selected.
MediaAppUIBrowserTest['MultipleSelectionLaunch'] = async () => {
  const directoryContents = await createMultipleImageFiles([0, 1, 2, 3]);
  const selectedIndexes = [1, 3];
  await launchWithFiles(directoryContents, selectedIndexes);

  // Expect filenames to be sorted in the default lexicographical order.
  assertEquals(TEST_ONLY.sortOrder, SortOrder.A_FIRST);
  assertFilenamesToBe('1.png,3.png');
};

// Test that each file type has an icon in light mode.
MediaAppUIBrowserTest['NotifyCurrentFileLight'] = async () => {
  const imageFile = new File([], 'image.png', {type: 'image/png'});
  const audioFile = new File([], 'audio.wav', {type: 'audio/wav'});
  const videoFile = new File([], 'video.mp4', {type: 'video/mp4'});
  const pdfFile = new File([], 'form.pdf', {type: 'application/pdf'});
  const unknownFile = new File([], 'foo.xyz', {type: 'unknown/unknown'});

  const TEST_CASES = [
    {file: imageFile, expectedTitle: 'image.png', expectedIconType: 'image'},
    {file: audioFile, expectedTitle: 'audio.wav', expectedIconType: 'audio'},
    {file: videoFile, expectedTitle: 'video.mp4', expectedIconType: 'video'},
    {file: unknownFile, expectedTitle: 'foo.xyz', expectedIconType: 'file'},
    {file: pdfFile, expectedTitle: 'form.pdf', expectedIconType: 'pdf'},
    {file: undefined, expectedTitle: 'Gallery', expectedIconType: 'app'},
  ];
  for (const {file, expectedTitle, expectedIconType} of TEST_CASES) {
    const name = file ? file.name : undefined;
    const type = file ? file.type : undefined;
    await sendTestMessage(
        {simple: 'notifyCurrentFile', simpleArgs: {name, type}});

    assertEquals(getTitle().innerText, expectedTitle);
    assertEquals(getIcon().href.includes(expectedIconType), true);
    assertEquals(getIcon().href.includes('dark'), false);
  }
};

// Test that each file type has a corresponding dark icon.
MediaAppUIBrowserTest['NotifyCurrentFileDark'] = async () => {
  const imageFile = new File([], 'image.png', {type: 'image/png'});
  const audioFile = new File([], 'audio.wav', {type: 'audio/wav'});
  const videoFile = new File([], 'video.mp4', {type: 'video/mp4'});
  const pdfFile = new File([], 'form.pdf', {type: 'application/pdf'});
  const unknownFile = new File([], 'foo.xyz', {type: 'unknown/unknown'});

  const TEST_CASES = [
    {file: imageFile, expectedIconType: 'image'},
    {file: audioFile, expectedIconType: 'audio'},
    {file: videoFile, expectedIconType: 'video'},
    {file: unknownFile, expectedIconType: 'file'},
    {file: pdfFile, expectedIconType: 'pdf'},
  ];
  for (const {file, expectedIconType} of TEST_CASES) {
    const name = file ? file.name : undefined;
    const type = file ? file.type : undefined;
    await sendTestMessage(
        {simple: 'notifyCurrentFile', simpleArgs: {name, type}});

    assertEquals(getIcon().href.includes(expectedIconType), true);
    assertEquals(getIcon().href.includes('dark'), true);
  }
};

// Test that the Gallery app icon does not have a dark variant.
MediaAppUIBrowserTest['NotifyCurrentFileAppIconDark'] = async () => {
  await sendTestMessage({
    simple: 'notifyCurrentFile',
    simpleArgs: {name: undefined, type: undefined},
  });

  assertEquals(getIcon().href.includes('app'), true);
  assertEquals(getIcon().href.includes('dark'), false);
};

// Tests that we show error UX when trying to launch an unopenable file.
MediaAppUIBrowserTest['LaunchUnopenableFile'] = async () => {
  const mockFileHandle =
      new FakeFileSystemFileHandle('not_allowed.png', 'image/png');
  mockFileHandle.getFileSync = () => {
    throw new DOMException(
        'Fake NotAllowedError for LoadUnopenableFile test.', 'NotAllowedError');
  };
  await launchWithHandles([mockFileHandle]);
  const result = await waitForErrorUX();

  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 0);
  assertEquals(await getFileErrors(), 'NotAllowedError');
};

// Tests that directories that are not navigable do not generate crash reports,
// and the focus file still loads.
MediaAppUIBrowserTest['LaunchUnnavigableDirectory'] = async () => {
  const focus = new FakeFileSystemFileHandle('focus.png', 'image/png');
  const mine = new FakeFileSystemFileHandle('mine.png', 'image/png');
  mine.errorToFireOnIterate = new DOMException('boom', 'NotFoundError');
  await launchWithHandles([focus, mine]);

  assertFilenamesToBe('focus.png');
  assertEquals(mine.errorToFireOnIterate, null);  // Consistency check.
};

// Tests that a file that becomes inaccessible after the initial app launch is
// ignored on navigation, and shows an error when navigated to itself.
MediaAppUIBrowserTest['NavigateWithUnopenableSibling'] = async () => {
  setSortOrder(SortOrder.A_FIRST);
  const handles = [
    fileToFileHandle(await createTestImageFile(111 /* width */, 10, '1.png')),
    fileToFileHandle(await createTestImageFile(222 /* width */, 10, '2.png')),
    fileToFileHandle(await createTestImageFile(333 /* width */, 10, '3.png')),
  ];
  await launchWithHandles(handles);
  let result = await waitForImageAndGetWidth('1.png');
  assertEquals(result, '111');
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), ',,');

  // Now that we've launched, make the *last* handle unopenable. This is only
  // interesting if we know the file will be re-opened, so check that first.
  // Note that if the file is non-null, no "reopen" occurs: launch.js does not
  // open files a second time after examining siblings for relevance to the
  // focus file.
  assertEquals(currentFiles[2]!.file, null);
  handles[2]!.getFileSync = () => {
    throw new DOMException(
        'Fake NotAllowedError for NavigateToUnopenableSibling test.',
        'NotAllowedError');
  };
  await advance(1);  // Navigate to the still-openable second file.

  result = await waitForImageAndGetWidth('2.png');
  assertEquals(result, '222');
  assertEquals(currentFiles.length, 3);

  // The error stays on the third, now unopenable. But, since we've advanced, it
  // has now rotated into the second slot. But! Also we don't validate it until
  // it rotates into the first slot, so the error won't be present yet. If we
  // implement pre-loading, this expectation can change to
  // ',NotAllowedError,'.
  assertEquals(await getFileErrors(), ',,');

  // Navigate to the unopenable file and expect a graceful error.
  await advance(1);
  result = await waitForErrorUX();
  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), ',,NotAllowedError');

  // Navigating back to an openable file should still work, and the error should
  // "stick".
  await advance(1);
  result = await waitForImageAndGetWidth('1.png');
  assertEquals(result, '111');
  assertEquals(currentFiles.length, 3);
  assertEquals(await getFileErrors(), ',,NotAllowedError');
};

// Tests a hypothetical scenario where a file may be deleted and replaced with
// an openable directory with the same name while the app is running.
MediaAppUIBrowserTest['FileThatBecomesDirectory'] = async () => {
  await sendTestMessage({suppressCrashReports: true});
  const handles = [
    fileToFileHandle(await createTestImageFile(111 /* width */, 10, '1.png')),
    fileToFileHandle(await createTestImageFile(222 /* width */, 10, '2.png')),
  ];

  await launchWithHandles(handles);
  let result = await waitForImageAndGetWidth('1.png');
  assertEquals(await getFileErrors(), ',');

  (handles[1] as {kind: string}).kind = 'directory';
  handles[1]!.getFileSync = () => {
    throw new Error(
        '(in test) FileThatBecomesDirectory: getFileSync should not be called');
  };

  await advance(1);
  result = await waitForErrorUX();
  assertMatch(result, GENERIC_ERROR_MESSAGE_REGEX);
  assertEquals(currentFiles.length, 2);
  assertEquals(await getFileErrors(), ',NotAFile');
};

// Tests that chrome://media-app can successfully send a request to open the
// feedback dialog and receive a response.
MediaAppUIBrowserTest['CanOpenFeedbackDialog'] = async () => {
  const result = await mediaAppPageHandler.openFeedbackDialog();

  assertEquals(result.errorMessage, null);
};

// Tests that video elements in the guest can be full-screened.
MediaAppUIBrowserTest['CanFullscreenVideo'] = async () => {
  // Remove `overflow: hidden` to work around a spurious DCHECK in Blink
  // layout. See crbug.com/1052791. Oddly, even though the video is in the guest
  // iframe document (which also has these styles on its body), it is necessary
  // and sufficient to remove these styles applied to the main frame.
  document.body.style.overflow = 'unset';

  // Load a zero-byte video. It won't load, but the video element should be
  // added to the DOM (and it can still be fullscreened).
  await launchWithFiles(
      [new File([], 'zero_byte_video.webm', {type: 'video/webm'})]);

  const SELECTOR = 'video';
  const tagName = await driver.waitForElementInGuest(SELECTOR, 'tagName');
  const result = await driver.waitForElementInGuest(
      SELECTOR, undefined, {requestFullscreen: true});

  // A TypeError of 'fullscreen error' results if fullscreen fails.
  assertEquals(result, 'hooray');
  assertEquals(tagName, '"VIDEO"');
};

// Tests that associated subtitles get not just a handle but a valid open File
// upon initial file load.
MediaAppUIBrowserTest['LoadVideoWithSubtitles'] = async () => {
  // Mock the send message call to prevent actual loading. We just want to see
  // what would be sent.
  let secondMessageSent!: Promise<{messageId: string, data: {}}>;
  const messageSent = new Promise<{messageId: string, data: {}}>(resolve => {
    guestMessagePipe.sendMessage = (messageType: string, message: {}) => {
      resolve({messageId: messageType, data: message});
      secondMessageSent = new Promise(resolveAgain => {
        guestMessagePipe.sendMessage = (messageType: string, message: {}) => {
          resolveAgain({messageId: messageType, data: message});
          return Promise.resolve();
        };
      });
      return Promise.resolve();
    };
  });
  await launchWithFiles([
    new File([], 'zero_byte_video.webm', {type: 'video/webm'}),
    new File([], 'zero_byte_video.vtt', {}),
    new File([], 'extra_video.webm', {}),
    new File([], 'unrelated_file.html', {}),
  ]);

  const message = await messageSent;
  assertEquals(message.messageId, Message.LOAD_FILES);

  // Initial launch should have two files, and they should both have valid
  // (non-null) File objects.
  // Note LoadFilesMessage is not type-checked here: the test file can't depend
  // on messge_types.js directly because it's rolled up into launch.js. We
  // *should* be able to re-export LoadFilesMessage, but that confuses closure
  // too much. See b/185734620.
  let data = message.data as LoadFilesMessage;
  assertEquals(data.files.length, 2);
  assertEquals(data.files[0]!.name, 'zero_byte_video.webm');
  assertNotEquals(data.files[0]!.file, null);
  assertEquals(data.files[1]!.name, 'zero_byte_video.vtt');
  assertNotEquals(data.files[1]!.file, null);

  // The extra files message shouldn't include any of the old files. And the new
  // file should have a null ref.
  const secondMessage = await secondMessageSent;
  assertEquals(secondMessage.messageId, Message.LOAD_EXTRA_FILES);

  data = secondMessage.data as LoadFilesMessage;
  assertEquals(data.files.length, 1);
  assertEquals(data.files[0]!.name, 'extra_video.webm');
  assertEquals(data.files[0]!.file, null);
};

// Tests the IPC behind the implementation of ReceivedFile.overwriteOriginal()
// in the untrusted context. Ensures it correctly updates the file handle owned
// by the privileged context.
MediaAppUIBrowserTest['OverwriteOriginalIPC'] = async () => {
  const directory = await launchWithFiles([await createTestImageFile()]);
  const handle = directory.files[0]!;

  // Write should not be called initially.
  assertEquals(handle.lastWritable.writes.length, 0);

  const message = {overwriteLastFile: 'Foo'};
  const testResponse = await sendTestMessage(message);
  const writeResult = await handle.lastWritable.closePromise;

  assertEquals(testResponse.testQueryResult, 'overwriteOriginal resolved');
  assertEquals(
      testResponse.testQueryResultData!['receiverFileName'], 'test_file.png');
  assertEquals(testResponse.testQueryResultData!['receiverErrorName'], '');
  assertEquals(await writeResult.text(), 'Foo');
  assertEquals(handle.lastWritable.writes.length, 1);
  assertDeepEquals(
      handle.lastWritable.writes[0], {position: 0, size: 'Foo'.length});

  // Ensure there's a last modified property on the file after overwriting. The
  // time should be "now", but there isn't a non-flaky way to test that. Just
  // check that it's defined and is strictly positive.
  const loadedFiles = await getLoadedFiles();
  assertEquals(loadedFiles.length, 1);
  assertGE(loadedFiles[0]!.lastModified, 1);
};

MediaAppUIBrowserTest['RejectZeroByteWrites'] = async () => {
  const directory = await launchWithFiles([await createTestImageFile()]);
  const handle = directory.files[0]!;

  const EMPTY_DATA = '';
  const message = {overwriteLastFile: EMPTY_DATA};
  const testResponse = await sendTestMessage(message);

  assertEquals(
      testResponse.testQueryResult,
      'overwriteOriginal failed Error:' +
          ' EmptyWriteError: overwrite-file: saveBlobToFile():' +
          ' Refusing to write zero bytes.');
  assertEquals(handle.lastWritable.writes.length, 0);
};

// Tests that OverwriteOriginal shows a file picker (and writes to that file) if
// the write attempt to the original file fails.
MediaAppUIBrowserTest['OverwriteOriginalPickerFallback'] = async () => {
  const directory = await launchWithFiles([await createTestImageFile()]);

  directory.files[0]!.nextCreateWritableError =
      new DOMException('Fake exception to trigger file picker', 'FakeError');

  const pickedFile = new FakeFileSystemFileHandle('pickme.png');
  window.showSaveFilePicker = () => Promise.resolve(pickedFile);

  const message = {overwriteLastFile: 'Foo'};
  const testResponse = await sendTestMessage(message);
  const writeResult = await pickedFile.lastWritable.closePromise;

  assertEquals(testResponse.testQueryResult, 'overwriteOriginal resolved');
  assertEquals(
      testResponse.testQueryResultData['receiverFileName'], 'pickme.png');
  assertEquals(
      testResponse.testQueryResultData['receiverErrorName'], 'FakeError');
  assertEquals(await writeResult.text(), 'Foo');
  assertEquals(pickedFile.lastWritable.writes.length, 1);
  assertDeepEquals(
      pickedFile.lastWritable.writes[0], {position: 0, size: 'Foo'.length});
};

// Tests that extensions in the `accept` option passed to showSaveFilePicker is
// correctly configured when only a MIME type is provided.
MediaAppUIBrowserTest['FilePickerValidateExtension'] = async () => {
  const JPG_EXTENSIONS =
      ['.jpg', '.jpeg', '.jpe', '.jfif', '.jif', '.jfi', '.pjpeg', '.pjp'];
  function pick(mimeType: string) {
    return new Promise(resolve => {
      window.showSaveFilePicker = options => {
        if (options.types) {
          assertEquals(!!options.excludeAcceptAllOption, true);
          resolve(options.types.map((t: any) => Object.values(t.accept || {})));
        } else {
          assertEquals(!!options.excludeAcceptAllOption, false);
          resolve(null);
        }
        // The handle is unused in the test, but needed to keep types happy.
        return Promise.resolve(null as unknown as FileSystemFileHandle);
      };
      pickWritableFile('foo.foo', mimeType, 0, []);
    });
  }

  assertDeepEquals(await pick('image/jpeg'), [[JPG_EXTENSIONS]]);
  assertDeepEquals(await pick('image/png'), [[['.png']]]);
  assertDeepEquals(await pick('image/webp'), [[['.webp']]]);
  assertDeepEquals(await pick('application/pdf'), [[['.pdf']]]);
  assertDeepEquals(await pick('image/unknown'), null);
  assertDeepEquals(await pick(''), null);
};

// Tests `MessagePipe.sendMessage()` properly propagates errors.
MediaAppUIBrowserTest['CrossContextErrors'] = async () => {
  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = (error: unknown) =>
      console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;

  const directory = await launchWithFiles([await createTestImageFile()]);

  // Note createWritable() throws DOMException, which does not have a stack, but
  // in this test we want to test capture of stacks in the trusted context, so
  // throw an error (made "here", so MediaAppUIBrowserTest is in the stack).
  const error = new Error('Fake NotAllowedError for CrossContextErrors test.');
  error.name = 'NotAllowedError';
  const pickedFile = new FakeFileSystemFileHandle();
  pickedFile.nextCreateWritableError = error;
  window.showSaveFilePicker = () => Promise.resolve(pickedFile);

  directory.files[0]!.nextCreateWritableError =
      new DOMException('Fake exception to trigger file picker', 'FakeError');

  let caughtError!: Error;

  try {
    const message = {overwriteLastFile: 'Foo', rethrow: true};
    await sendTestMessage(message);
  } catch (e: any) {
    caughtError = e;
  }

  assertEquals(caughtError.name, 'NotAllowedError');
  assertEquals(caughtError.message, `test: overwrite-file: ${error.message}`);
};

// Tests the IPC behind the implementation of ReceivedFile.deleteOriginalFile()
// in the untrusted context.
MediaAppUIBrowserTest['DeleteOriginalIPC'] = async () => {
  let directory = await launchWithFiles(
      [await createTestImageFile(1, 1, 'first_file_name.png')]);
  const testHandle = directory.files[0];
  let testResponse;

  // Nothing should be deleted initially.
  assertEquals(null, directory.lastDeleted);

  const messageDelete = {deleteLastFile: true};
  testResponse = await sendTestMessage(messageDelete);

  // Assertion will fail if exceptions from launch.js are thrown, no exceptions
  // indicates the file was successfully deleted.
  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandle, directory.lastDeleted);
  // File removed from `DirectoryHandle` internal state.
  assertEquals(0, directory.files.length);

  // Load another file and replace its handle in the underlying
  // `FileSystemDirectoryHandle`. This gets us into a state where the file on
  // disk has been deleted and a new file with the same name replaces it without
  // updating the `FakeSystemDirectoryHandle`. The new file shouldn't be deleted
  // as it has a different `FileHandle` reference.
  directory = await launchWithFiles(
      [await createTestImageFile(1, 1, 'first_file_name.png')]);
  directory.files[0] = new FakeFileSystemFileHandle('first_file_name.png');

  // Try delete the first file again, should result in file moved.
  const messageDeleteMoved = {deleteLastFile: true};
  testResponse = await sendTestMessage(messageDeleteMoved);

  assertEquals(
      'deleteOriginalFile failed Error: NotFoundError: delete-file: ' +
          'Ignoring delete request: file not found',
      testResponse.testQueryResult);
  // New file not removed from `DirectoryHandle` internal state.
  assertEquals(1, directory.files.length);

  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = (error: unknown) =>
      console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;
  // Test it throws an error by simulating a failed directory change.
  simulateLosingAccessToDirectory();

  const messageDeleteNoOp = {deleteLastFile: true};
  testResponse = await sendTestMessage(messageDeleteNoOp);

  assertEquals(
      'deleteOriginalFile failed Error: Error: delete-file: Delete failed. ' +
          'File without launch directory.',
      testResponse.testQueryResult);
};

// Tests when a file is deleted, the app tries to open the next available file
// and reloads with those files.
MediaAppUIBrowserTest['DeletionOpensNextFile'] = async () => {
  setSortOrder(SortOrder.A_FIRST);
  const testFiles = [
    await createTestImageFile(1, 1, 'test_file_1.png'),
    await createTestImageFile(1, 1, 'test_file_2.png'),
    await createTestImageFile(1, 1, 'test_file_3.png'),
  ];
  const directory = await launchWithFiles(testFiles);
  let testResponse;
  // Shallow copy so mutations to `directory.files` don't effect
  // `testHandles`.
  const testHandles = [...directory.files];

  // Check the app loads all 3 files.
  let lastLoadedFiles = await getLoadedFiles();
  assertEquals(3, lastLoadedFiles.length);
  assertEquals('test_file_1.png', lastLoadedFiles[0]!.name);
  assertEquals('test_file_2.png', lastLoadedFiles[1]!.name);
  assertEquals('test_file_3.png', lastLoadedFiles[2]!.name);

  // Delete the first file.
  const messageDelete = {deleteLastFile: true};
  testResponse = await sendTestMessage(messageDelete);

  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandles[0], directory.lastDeleted);
  assertEquals(directory.files.length, 2);

  // Check the app reloads the file list with the remaining two files.
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(2, lastLoadedFiles.length);
  assertEquals('test_file_2.png', lastLoadedFiles[0]!.name);
  assertEquals('test_file_3.png', lastLoadedFiles[1]!.name);

  // Navigate to the last file (originally the third file) and delete it
  const token = currentFiles[getEntryIndex()]!.token;
  await sendTestMessage({navigate: {direction: 'next', token}});
  testResponse = await sendTestMessage(messageDelete);

  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);
  assertEquals(testHandles[2], directory.lastDeleted);
  assertEquals(directory.files.length, 1);

  // Check the app reloads the file list with the last remaining file
  // (originally the second file).
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(1, lastLoadedFiles.length);
  assertEquals(testFiles[1]!.name, lastLoadedFiles[0]!.name);

  // Delete the last file, should lead to zero state.
  testResponse = await sendTestMessage(messageDelete);
  assertEquals(
      'deleteOriginalFile resolved success', testResponse.testQueryResult);

  // The app should be in zero state with no media loaded.
  lastLoadedFiles = await getLoadedFiles();
  assertEquals(0, lastLoadedFiles.length);
};

// Tests that the app gracefully handles a delete request on a file that's
// been deleted or moved.
MediaAppUIBrowserTest['DeleteMissingFile'] = async () => {
  const directory = await launchWithFiles(
      [await createTestImageFile(1, 1, 'first_file_name.png')]);
  makeFileNotFound(directory.files[0]);

  const messageDelete = {deleteLastFile: true};
  const testResponse = await sendTestMessage(messageDelete);

  assertEquals(
      'deleteOriginalFile failed Error: NotFoundError: delete-file: ' +
          'Ignoring delete request: file not found',
      testResponse.testQueryResult);
};

// Tests that the app gracefully handles a rename request on a file that's
// been deleted or moved.
MediaAppUIBrowserTest['RenameMissingFile'] = async () => {
  const directory =
      await launchWithFiles([await createTestImageFile(1, 1, 'file_name.png')]);
  makeFileNotFound(directory.files[0]);

  const messageRename = {renameLastFile: 'new_file_name'};
  const testResponse = await sendTestMessage(messageRename);

  assertEquals(
      'renameOriginalFile resolved FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY',
      testResponse.testQueryResult);
};

// Tests the IPC behind the AbstractFile.openFile function to open a file from a
// file handle token previously communicated to the untrusted context.
MediaAppUIBrowserTest['OpenAllowedFileIPC'] = async () => {
  await launchWithFiles(
      [await createTestImageFile(), await createTestImageFile()]);
  let testResponse = await sendTestMessage({simple: 'getAllFiles'});
  let clientFiles: FileSnapshot[] = testResponse.testQueryResultData;

  // Second file should be a placeholder with zero size.
  const IMAGE_FILE_SIZE = 1605;
  assertEquals(clientFiles[0]!.size, IMAGE_FILE_SIZE);
  assertEquals(clientFiles[1]!.size, 0);

  testResponse = await sendTestMessage(
      {simple: 'openFileAtIndex', simpleArgs: {index: 1}});
  assertEquals(testResponse.testQueryResult, 'opened and updated');

  testResponse = await sendTestMessage({simple: 'getAllFiles'});
  clientFiles = testResponse.testQueryResultData;

  // Second file should now be opened and have a valid size.
  assertEquals(clientFiles[0]!.size, IMAGE_FILE_SIZE);
  assertEquals(clientFiles[1]!.size, IMAGE_FILE_SIZE);
};

// Tests the IPC behind the loadNext and loadPrev functions on the received file
// list in the untrusted context.
MediaAppUIBrowserTest['NavigateIPC'] = async () => {
  await launchWithFiles(
      [await createTestImageFile(), await createTestImageFile()]);
  const fileOneToken = currentFiles[0]!.token;
  const fileTwoToken = currentFiles[1]!.token;
  assertEquals(getEntryIndex(), 0);

  let result = await sendTestMessage(
      {navigate: {direction: 'next', token: fileOneToken}});
  assertEquals(result.testQueryResult, 'loadNext called');
  assertEquals(getEntryIndex(), 1);

  result = await sendTestMessage(
      {navigate: {direction: 'prev', token: fileTwoToken}});
  assertEquals(result.testQueryResult, 'loadPrev called');
  assertEquals(getEntryIndex(), 0);

  result = await sendTestMessage(
      {navigate: {direction: 'prev', token: fileOneToken}});
  assertEquals(result.testQueryResult, 'loadPrev called');
  assertEquals(getEntryIndex(), 1);
};

// Tests the loadNext and loadPrev functions on the received file list correctly
// navigate when they are working with a out of date file list.
// Regression test for b/163662946
MediaAppUIBrowserTest['NavigateOutOfSync'] = async () => {
  await launchWithFiles(
      [await createTestImageFile(), await createTestImageFile()]);
  const fileOneToken = currentFiles[0]!.token;
  const fileTwoToken = currentFiles[1]!.token;

  // Simulate some operation updating getEntryIndex() without reloading the
  // media app.
  setEntryIndex(1);

  let result = await sendTestMessage(
      {navigate: {direction: 'next', token: fileOneToken}});
  assertEquals(result.testQueryResult, 'loadNext called');
  // The media app is focused on file 0 so the next file is file 1.
  assertEquals(getEntryIndex(), 1);

  setEntryIndex(0);

  result = await sendTestMessage(
      {navigate: {direction: 'prev', token: fileTwoToken}});
  assertEquals(result.testQueryResult, 'loadPrev called');
  assertEquals(getEntryIndex(), 0);

  // The received file list and entry index currently agree that the 0th file is
  // open. Tell loadNext that the 1st file is current to ensure that navigate
  // respects our provided token over any other signal.
  result = await sendTestMessage(
      {navigate: {direction: 'next', token: fileTwoToken}});
  assertEquals(result.testQueryResult, 'loadNext called');
  assertEquals(getEntryIndex(), 0);
};

// Tests the IPC behind the implementation of ReceivedFile.renameOriginalFile()
// in the untrusted context. This test is integration-y making sure we rename
// the focus file and that gets inserted in the right place in `currentFiles`
// preserving navigation order.
MediaAppUIBrowserTest['RenameOriginalIPC'] = async () => {
  const directory = await launchWithFiles([
    await createTestImageFile(1, 1, 'file1.png'),
    await createTestImageFile(1, 1, 'file2.png'),
  ]);

  // Nothing should be deleted initially.
  assertEquals(null, directory.lastDeleted);

  // Navigate to second file "file2.png".
  await advance(1);

  // Test normal rename flow.
  const file2Handle = directory.files[getEntryIndex()]!;
  const file2File = file2Handle.getFileSync();
  const file2Token = currentFiles[getEntryIndex()]!.token;
  let messageRename = {renameLastFile: 'new_file_name.png'};
  let testResponse;

  testResponse = await sendTestMessage(messageRename);

  assertEquals(
      testResponse.testQueryResult, 'renameOriginalFile resolved success');
  // The original file that was renamed got deleted.
  assertEquals(file2Handle, directory.lastDeleted);
  // The new file has the right name in the trusted context.
  assertEquals(directory.files.length, 2);
  assertEquals(directory.files[getEntryIndex()]!.name, 'new_file_name.png');
  assertEquals(currentFiles[getEntryIndex()]!.handle.name, 'new_file_name.png');

  // The file doesn't need to be opened yet. Wait for a navigation.
  assertEquals(currentFiles[getEntryIndex()]!.file, null);

  // The new file has the right name in the untrusted context.
  testResponse = await sendTestMessage({simple: 'getLastFile'});
  const result: FileSnapshot = testResponse.testQueryResultData;
  assertEquals(result.name, 'new_file_name.png');
  // The new file uses the same token as the old file.
  assertEquals(currentFiles[getEntryIndex()]!.token, file2Token);
  // Check the new file written has the correct data.
  const renamedHandle = directory.files[getEntryIndex()]!;
  const renamedFile = await renamedHandle.getFile();
  assertEquals(renamedFile.size, file2File.size);
  assertEquals(await renamedFile.text(), await file2File.text());
  // Check the internal representation (token map & currentFiles) is updated.
  assertEquals(tokenMap.get(file2Token), renamedHandle);
  assertEquals(currentFiles[getEntryIndex()]!.handle, renamedHandle);

  // Check navigation order is preserved.
  assertEquals(getEntryIndex(), 1);
  assertEquals(currentFiles[getEntryIndex()]!.handle.name, 'new_file_name.png');
  assertEquals(currentFiles[0]!.handle.name, 'file1.png');

  // Advancing wraps around back to the first file.
  await advance(1);

  assertEquals(getEntryIndex(), 0);
  assertEquals(currentFiles[getEntryIndex()]!.handle.name, 'file1.png');
  assertEquals(currentFiles[1]!.handle.name, 'new_file_name.png');

  // Test renaming when a file with the new name already exists, tries to rename
  // `file1.png` to `new_file_name.png` which already exists.
  const messageRenameExists = {renameLastFile: 'new_file_name.png'};
  testResponse = await sendTestMessage(messageRenameExists);

  assertEquals(
      testResponse.testQueryResult, 'renameOriginalFile resolved file exists');
  // No change to the existing file.
  assertEquals(directory.files.length, 2);
  assertEquals(directory.files[getEntryIndex()]!.name, 'file1.png');
  assertEquals(directory.files[1]!.name, 'new_file_name.png');

  // Test renaming when something is out of sync with `currentFiles` and has an
  // expired token.
  const expiredToken = tokenGenerator.next().value;
  currentFiles[getEntryIndex()]!.token = expiredToken;

  messageRename = {renameLastFile: 'another_name.png'};

  testResponse = await sendTestMessage(messageRename);

  // Fails silently, nothing changes.
  assertEquals(
      testResponse.testQueryResult,
      'renameOriginalFile resolved FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY');
  assertEquals(currentFiles[getEntryIndex()]!.handle.name, 'file1.png');
  assertEquals(currentFiles.length, 2);
  assertEquals(directory.files.length, 2);

  // Test it throws an error by simulating a failed directory change.
  simulateLosingAccessToDirectory();

  // Prevent the trusted context throwing errors resulting JS errors.
  guestMessagePipe.logClientError = (error: unknown) =>
      console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;

  const messageRenameNoOp = {renameLastFile: 'new_file_name_2.png'};
  testResponse = await sendTestMessage(messageRenameNoOp);

  assertEquals(
      testResponse.testQueryResult,
      'renameOriginalFile failed Error: Error: rename-file: Rename failed. ' +
          'File without launch directory.');
};

// Mock out choose file system entries since it can only be interacted with
// via trusted user gestures.
function mockShowSaveFilePicker() {
  const newFileHandle = new FakeFileSystemFileHandle();
  const chooseEntries = new Promise<FilePickerOptions>(resolve => {
    window.showSaveFilePicker = options => {
      resolve(options);
      return Promise.resolve(newFileHandle);
    };
  });
  return chooseEntries;
}

// Tests the IPC behind the requestSaveFile delegate function.
MediaAppUIBrowserTest['RequestSaveFileIPC'] = async () => {
  let chooseEntries = mockShowSaveFilePicker();
  await launchWithFiles([await createTestImageFile(10, 10)]);

  // Initially test with accept `empty`.
  let result = await sendTestMessage({simple: 'requestSaveFile'});
  let options = await chooseEntries;
  let types = options.types!;
  let lastToken = `${[...tokenMap.keys()].slice(-1)[0]}`;

  // Check the token matches to confirm the ReceivedFile returned represents the
  // new file created on disk.
  assertMatch(result.testQueryResult, lastToken);
  assertEquals(types.length, 1);
  assertEquals(types[0]!.description, 'PNG');
  assertDeepEquals(types[0]!.accept['image/png'], ['.png']);

  chooseEntries = mockShowSaveFilePicker();
  result = await sendTestMessage(
      {simple: 'requestSaveFile', simpleArgs: {accept: ['PDF', 'PNG']}});
  options = await chooseEntries;
  types = options.types!;
  lastToken = `${[...tokenMap.keys()].slice(-1)[0]}`;

  assertMatch(result.testQueryResult, lastToken);
  assertEquals(types.length, 2);
  assertEquals(types[0]!.description, 'PDF');
  assertDeepEquals(types[0]!.accept['application/pdf'], ['.pdf']);
  assertEquals(types[1]!.description, 'PNG');
  assertDeepEquals(types[1]!.accept['image/png'], ['.png']);
};

// Tests the IPC behind the getExportFile method.
MediaAppUIBrowserTest['GetExportFileIPC'] = async () => {
  const chooseEntries = mockShowSaveFilePicker();
  const directory = await launchWithFiles([await createTestImageFile(10, 10)]);

  const message = {
    simple: 'getExportFile',
    simpleArgs: {accept: ['PNG', 'JPG', 'WEBP']},
  };
  const result = await sendTestMessage(message);
  const options = await chooseEntries;
  const types = options.types!;
  const lastToken = `${[...tokenMap.keys()].slice(-1)[0]}`;

  assertMatch(result.testQueryResult, lastToken);
  assertEquals(types.length, 3);

  // Contents and order of the `types` array should correspond.
  assertEquals(types[0]!.description, 'PNG');
  assertEquals(types[1]!.description, 'JPG');
  assertEquals(types[2]!.description, 'WEBP');
  assertDeepEquals(types[0]!.accept['image/png'], ['.png']);
  assertDeepEquals(types[2]!.accept['image/webp'], ['.webp']);

  // jpg has a bunch of extensions.
  assertEquals(types[1]!.accept['image/jpeg']!.length, 8);

  // The startIn option should be set to the opened file.
  assertEquals(options.startIn, directory.files[0]);
};

// Tests the IPC behind the saveAs function on received files.
MediaAppUIBrowserTest['SaveAsIPC'] = async () => {
  // Mock out choose file system entries since it can only be interacted with
  // via trusted user gestures.
  const newFileHandle = new FakeFileSystemFileHandle('new_file.jpg');
  window.showSaveFilePicker = () => Promise.resolve(newFileHandle);

  const directory = await launchWithFiles(
      [await createTestImageFile(10, 10, 'original_file.jpg')]);

  const originalFileToken = currentFiles[0]!.token;
  assertEquals(getEntryIndex(), 0);

  const receivedFilesBefore = await getLoadedFiles();
  const result = await sendTestMessage({saveAs: 'foo'});
  const receivedFilesAfter = await getLoadedFiles();

  // Make sure the receivedFile object has the correct state.
  assertEquals(result.testQueryResult, 'new_file.jpg');
  assertEquals(await result.testQueryResultData['blobText'], 'foo');
  // Confirm the right string was written to the new file.
  const writeResult = await newFileHandle.lastWritable.closePromise;
  assertEquals(await writeResult.text(), 'foo');
  // Make sure we have created a new file descriptor, and that
  // the original file is still available.
  assertEquals(getEntryIndex(), 1);
  assertEquals(currentFiles[0]!.handle, directory.files[0]);
  assertEquals(currentFiles[0]!.handle.name, 'original_file.jpg');
  assertNotEquals(currentFiles[0]!.token, originalFileToken);
  assertEquals(currentFiles[1]!.handle, newFileHandle);
  assertEquals(currentFiles[1]!.handle.name, 'new_file.jpg');
  assertEquals(currentFiles[1]!.token, originalFileToken);
  assertEquals(tokenMap.get(currentFiles[0]!.token), currentFiles[0]!.handle);
  assertEquals(tokenMap.get(currentFiles[1]!.token), currentFiles[1]!.handle);

  // Currently, files obtained from a file picker can not be deleted or renamed.
  // TODO(b/163285659): Try to support delete/rename in this case. For now, we
  // check that the methods go away so that the UI updates to disable buttons.
  assertEquals(receivedFilesBefore[0]!.hasRename, true);
  assertEquals(receivedFilesBefore[0]!.hasDelete, true);
  assertEquals(receivedFilesAfter[0]!.hasRename, false);
  assertEquals(receivedFilesAfter[0]!.hasDelete, false);

  // Ensure there's a last modified property on the file after swapping in the
  // picked file.
  assertGE(receivedFilesAfter[0]!.lastModified, 1);
};

// Tests the error handling behind the saveAs function on received files.
MediaAppUIBrowserTest['SaveAsErrorHandling'] = async () => {
  // Prevent the trusted context from throwing errors which cause the test to
  // fail.
  guestMessagePipe.logClientError = (error: unknown) =>
      console.log(JSON.stringify(error));
  guestMessagePipe.rethrowErrors = false;
  const newFileHandle = new FakeFileSystemFileHandle('new_file.jpg');
  newFileHandle.nextCreateWritableError =
      new DOMException('Fake exception', 'FakeError');
  window.showSaveFilePicker = () => Promise.resolve(newFileHandle);
  const directory = await launchWithFiles(
      [await createTestImageFile(10, 10, 'original_file.jpg')]);
  const originalFileToken = currentFiles[0]!.token;

  const result = await sendTestMessage({saveAs: 'foo'});

  // Make sure we revert back to our original state.
  assertEquals(
      result.testQueryResult,
      'saveAs failed Error: FakeError: save-as: Fake exception');
  assertEquals(result.testQueryResultData!['filename'], 'original_file.jpg');
  assertEquals(getEntryIndex(), 0);
  assertEquals(currentFiles.length, 1);
  assertEquals(currentFiles[0]!.handle, directory.files[0]);
  assertEquals(currentFiles[0]!.handle.name, 'original_file.jpg');
  assertEquals(currentFiles[0]!.token, originalFileToken);
  assertEquals(tokenMap.get(currentFiles[0]!.token), currentFiles[0]!.handle);
};

// Tests the IPC behind the AbstractFileList.openFilesWithFilePicker function to
// relaunch the app with a new selection of files from a file picker.
MediaAppUIBrowserTest['OpenFilesWithFilePickerIPC'] = async () => {
  const pickedFileHandles = [
    new FakeFileSystemFileHandle('picked_file1.jpg'),
    new FakeFileSystemFileHandle('picked_file2.jpg'),
  ];
  let lastPickerOptions!: OpenFilePickerOptions;
  window.showOpenFilePicker = (pickerOptions) => {
    lastPickerOptions = pickerOptions;
    return Promise.resolve(pickedFileHandles);
  };
  const directory = await launchWithFiles(
      [await createTestImageFile(10, 10, 'original_file.jpg')]);

  const simpleArgs: any = {acceptTypeKeys: ['VIDEO', 'IMAGE']};
  async function openFilesWithFilePickerWithSimpleArgs() {
    const response =
        await sendTestMessage({simple: 'openFilesWithFilePicker', simpleArgs});
    assertEquals(response.testQueryResult, 'openFilesWithFilePicker resolved');
    return response;
  }

  let testResponse = await openFilesWithFilePickerWithSimpleArgs();

  // Spot-check the file picker options. It has lots of file extensions in it.
  const {multiple, startIn, excludeAcceptAllOption, types} = lastPickerOptions;
  assertEquals(multiple, true);
  assertEquals(startIn, directory.files[0]);
  assertEquals(excludeAcceptAllOption, true);
  assertEquals(types!.length, 2);
  assertEquals(types![0]!.description, 'Video Files');
  assertEquals(types![1]!.description, 'Image Files');

  testResponse = await sendTestMessage({simple: 'getAllFiles'});
  console.log(JSON.stringify(testResponse));
  const clientFiles: FileSnapshot[] = testResponse.testQueryResultData;

  assertEquals(clientFiles[0]!.name, 'picked_file1.jpg');
  assertEquals(clientFiles[1]!.name, 'picked_file2.jpg');

  // Test to handle invalid tokens (b/209342852). These should leave the
  // `startIn` option unspecified.
  simpleArgs.explicitToken = -1;
  testResponse = await openFilesWithFilePickerWithSimpleArgs();
  assertEquals(lastPickerOptions.startIn, undefined);

  // Ensure the `singleFile` argument is handled when set.
  simpleArgs.singleFile = true;
  await openFilesWithFilePickerWithSimpleArgs();
  assertEquals(lastPickerOptions.multiple, false);

  simpleArgs.singleFile = false;
  await openFilesWithFilePickerWithSimpleArgs();
  assertEquals(lastPickerOptions.multiple, true);

  // Spot-check the ALL_EX_TEXT filter key, which groups all extensions.
  simpleArgs.acceptTypeKeys = ['ALL_EX_TEXT'];
  await openFilesWithFilePickerWithSimpleArgs();
  const extensions = lastPickerOptions.types![0]!.accept['*/*']!;
  assertEquals(lastPickerOptions.types!.length, 1);
  assertEquals(lastPickerOptions.types![0]!.description, 'All');
  assertEquals(extensions.includes('.pdf'), true);
  assertEquals(extensions.includes('.jpeg'), true);
  assertEquals(extensions.includes('.avi'), true);
  assertEquals(extensions.includes('.mp3'), true);
  assertEquals(extensions.includes('.zip'), false);
};

MediaAppUIBrowserTest['RelatedFiles'] = async () => {
  setSortOrder(SortOrder.A_FIRST);
  // These files all have a last modified time of 0 so the order they end up in
  // is their lexicographical order i.e. `jaypeg.jpg, jiff.gif, matroska.mkv,
  // world.webm`. When a file is loaded it becomes the "focus file" and files
  // get rotated around like such that we get `currentFiles = [focus file,
  // ...lexicographically larger files, ...lexicographically smaller files]`.
  const testFiles = [
    {name: 'html', type: 'text/html'},
    {name: 'jaypeg.jpg', type: 'image/jpeg'},
    {name: 'jiff.gif', type: 'image/gif'},
    {name: 'matroska.emkv'},
    {name: 'matroska.mkv'},
    {name: 'matryoshka.MKV'},
    {name: 'noext', type: ''},
    {name: 'other.txt', type: 'text/plain'},
    {name: 'subtitles.vtt'},
    {name: 'text.txt', type: 'text/plain'},
    {name: 'world.webm', type: 'video/webm'},
    {name: 'x.avi'},
    {name: 'y.3gp'},
    {name: 'z.mpg'},
  ];
  const directory = await createMockTestDirectory(testFiles);
  const files = directory.getFilesSync();
  const [html, jpg, gif, _emkv, mkv, MKV, ext, other, vtt, txt] = files;
  const [webm, avi, y3gp, mpg] = files.slice(10);

  await loadFilesWithoutSendingToGuest(directory, mkv!);
  assertFilesToBe([mkv, MKV, vtt, webm, avi, y3gp, mpg, jpg, gif], 'mkv');

  await loadFilesWithoutSendingToGuest(directory, jpg!);
  assertFilesToBe([jpg, gif, mkv, MKV, vtt, webm, avi, y3gp, mpg], 'jpg');

  await loadFilesWithoutSendingToGuest(directory, gif!);
  assertFilesToBe([gif, mkv, MKV, vtt, webm, avi, y3gp, mpg, jpg], 'gif');

  await loadFilesWithoutSendingToGuest(directory, webm!);
  assertFilesToBe([webm, avi, y3gp, mpg, jpg, gif, mkv, MKV, vtt], 'webm');

  await loadFilesWithoutSendingToGuest(directory, txt!);
  assertFilesToBe([txt, other], 'txt');

  await loadFilesWithoutSendingToGuest(directory, html!);
  assertFilesToBe([html], 'html');

  await loadFilesWithoutSendingToGuest(directory, ext!);
  assertFilesToBe([ext], 'ext');
};

MediaAppUIBrowserTest['SortedFilesByTime'] = async () => {
  setSortOrder(SortOrder.NEWEST_FIRST);
  // We want the more recent (i.e. higher timestamp) files first. In the case of
  // equal timestamp, it should sort lexicographically by filename.
  const filesInModifiedOrder = await Promise.all([
    createTestImageFile(1, 1, '6.png', 6),
    createTestImageFile(1, 1, '5.png', 5),
    createTestImageFile(1, 1, '4.png', 4),
    createTestImageFile(1, 1, '2a.png', 2),
    createTestImageFile(1, 1, '2b.png', 2),
    createTestImageFile(1, 1, '1.png', 1),
    createTestImageFile(1, 1, '0.png', 0),
  ]);
  const files = [...filesInModifiedOrder];
  // Mix up files so that we can check they get sorted correctly.
  [files[4], files[2], files[3]] = [files[2]!, files[3]!, files[4]!];

  await launchWithFiles(files);

  assertFilesToBe(filesInModifiedOrder);
};

MediaAppUIBrowserTest['SortedFilesByName'] = async () => {
  // A_FIRST should be the default.
  assertEquals(TEST_ONLY.sortOrder, SortOrder.A_FIRST);
  // Establish some sample files that match the naming style from the Camera app
  // in m86, except one file with lowercase prefix is included, to verify that
  // the collation ignores case (to match the Files app). Note we want
  // "pressing right" to go to the next taken photo/video, which means
  // lexicographic ordering.
  const filesInLexicographicOrder = await Promise.all([
    createTestImageFile(1, 1, 'IMG_20200921_104750.jpg', 5),  // Oldest.
    createTestImageFile(1, 1, 'IMG_20200921_104816.jpg', 7),  // Modified.
    createTestImageFile(1, 1, 'img_20200921_104910.jpg', 6),  // Newest on day.
    createTestImageFile(1, 1, 'IMG_20200922_104816.jpg', 9),  // Later date.
    createTestImageFile(1, 1, 'VID_20200921_104848.jpg', 8),  // Video from day.
  ]);
  const files = [...filesInLexicographicOrder];
  // Mix up files so that we can check they get sorted correctly.
  [files[4], files[2], files[3]] = [files[2]!, files[3]!, files[4]!];

  await launchWithFiles(files);

  assertFilesToBe(filesInLexicographicOrder);
};

// Tests that getFile is not called on all files in a directory on launch with
// default sort order. This is to avoid a series of slow file system api calls
// due to b/172529567.
MediaAppUIBrowserTest['GetFileNotCalledOnAllFiles'] = async () => {
  const handles = [
    fileToFileHandle(await createTestImageFile(1, 1, '1.png')),
    fileToFileHandle(await createTestImageFile(1, 1, '2.png')),
    fileToFileHandle(await createTestImageFile(1, 1, '3.png')),
    fileToFileHandle(await createTestImageFile(1, 1, '4.png')),
  ];
  const getFileCalls: string[] = [];
  for (const handle of handles) {
    handle.getFileSync = () => {
      getFileCalls.push(handle.name);
      return undefined as unknown as File;  // unused.
    };
  }

  await launchWithHandles(handles);

  // Expect only the current file to have been opened. Note the current file is
  // opened twice since the file is force refreshed before being sent over to
  // the guest in addition to the original open.
  assertEquals(getFileCalls.length, 2);
  assertEquals(getFileCalls[0], '1.png');
  assertEquals(getFileCalls[1], '1.png');
};

// Tests that the guest gets focus automatically on start up.
MediaAppUIBrowserTest['GuestHasFocus'] = async () => {
  const guest = queryIFrame();

  // By the time this tests runs the iframe should already have been loaded.
  assertEquals(document.activeElement, guest);
};

// Check the body element's background color when it is light mode.
MediaAppUIBrowserTest['BodyHasCorrectBackgroundColorInLightMode'] = () => {
  const actualBackgroundColor = getComputedStyle(document.body).backgroundColor;
  assertEquals(actualBackgroundColor, 'rgb(255, 255, 255)');  // White.
};
