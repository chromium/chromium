// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {VideoType} from '../type.js';
import {WaitableEvent} from '../waitable_event.js';

import {
  DOCUMENT_PREFIX,
  Filenamer,
  IMAGE_PREFIX,
  VIDEO_PREFIX,
} from './file_namer.js';
import {
  DirectoryAccessEntry,
  DirectoryAccessEntryImpl,
  FileAccessEntry,
} from './file_system_access_entry.js';
import * as idb from './idb.js';
import {getMaybeLazyDirectory} from './lazy_directory_entry.js';


/**
 * Checks if the entry's name has the video prefix.
 *
 * @param entry File entry.
 * @return Has the video prefix or not.
 */
export function hasVideoPrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(VIDEO_PREFIX);
}

/**
 * Checks if the entry's name has the image prefix.
 *
 * @param entry File entry.
 * @return Has the image prefix or not.
 */
function hasImagePrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(IMAGE_PREFIX);
}

/**
 * Checks if the entry's name has the document prefix.
 *
 * @param entry File entry.
 * @return Has the document prefix or not.
 */
function hasDocumentPrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(DOCUMENT_PREFIX);
}

/**
 * Temporary directory in the internal file system.
 */
let internalTempDir: DirectoryAccessEntry|null = null;

/**
 * Camera directory in the external file system.
 */
let cameraDir: DirectoryAccessEntry|null = null;

/**
 * Gets camera directory used by CCA.
 */
export function getCameraDirectory(): DirectoryAccessEntry|null {
  return cameraDir;
}

/**
 * Initializes the temporary directory in the internal file system.
 *
 * @return Promise for the directory result.
 */
async function initInternalTempDir(): Promise<DirectoryAccessEntry> {
  return new DirectoryAccessEntryImpl(await navigator.storage.getDirectory());
}

/**
 * Initializes the camera directory in the external file system.
 *
 * @return Promise for the directory result.
 */
async function initCameraDirectory(): Promise<DirectoryAccessEntry|null> {
  const handle = new WaitableEvent<FileSystemDirectoryHandle>();

  // We use the sessionStorage to decide if we should use the handle in the
  // database or the handle from the launch queue so that we can use the new
  // handle if the handle changes in the future.
  const isConsumedHandle = window.sessionStorage.getItem('IsConsumedHandle');
  if (isConsumedHandle !== null) {
    const storedHandle = await idb.get<FileSystemDirectoryHandle>(
        idb.KEY_CAMERA_DIRECTORY_HANDLE);
    assert(storedHandle !== null);
    handle.signal(storedHandle);
  } else {
    const launchQueue = window.launchQueue;
    assert(launchQueue !== undefined);
    launchQueue.setConsumer(async (launchParams) => {
      assert(launchParams.files.length > 0);
      const dir: FileSystemHandle = launchParams.files[0];
      assert(dir.kind === 'directory');

      await idb.set(idb.KEY_CAMERA_DIRECTORY_HANDLE, dir);
      window.sessionStorage.setItem('IsConsumedHandle', 'true');

      handle.signal(dir as FileSystemDirectoryHandle);
    });
  }
  const dir = await handle.wait();
  const myFilesDir = new DirectoryAccessEntryImpl(dir);
  return getMaybeLazyDirectory(myFilesDir, 'Camera');
}

/**
 * Initializes file systems. This function should be called only once in the
 * beginning of the app.
 */
export async function initialize(): Promise<void> {
  internalTempDir = await initInternalTempDir();
  assert(internalTempDir !== null);

  cameraDir = await initCameraDirectory();
  assert(cameraDir !== null);
}

/**
 * Saves photo blob or metadata blob into predefined default location.
 *
 * @param blob Data of the photo to be saved.
 * @param name Filename of the photo to be saved.
 * @return Promise for the result.
 */
export async function saveBlob(
    blob: Blob, name: string): Promise<FileAccessEntry> {
  assert(cameraDir !== null);
  const file = await cameraDir.createFile(name);
  assert(file !== null);

  await file.write(blob);
  return file;
}

/**
 * Creates a file for saving video recording result.
 *
 * @return Newly created video file.
 * @throws If failed to create video file.
 */
export async function createVideoFile(videoType: VideoType):
    Promise<FileAccessEntry> {
  assert(cameraDir !== null);
  const name = new Filenamer().newVideoName(videoType);
  const file = await cameraDir.createFile(name);
  if (file === null) {
    throw new Error('Failed to create video temp file.');
  }
  return file;
}

const PRIVATE_TEMPFILE_NAME = 'video-intent.mp4';

/**
 * @return Newly created temporary file.
 * @throws If failed to create video temp file.
 */
export async function createPrivateTempVideoFile(): Promise<FileAccessEntry> {
  // TODO(inker): Handles running out of space case.
  const dir = internalTempDir;
  assert(dir !== null);
  const file = await dir.createFile(PRIVATE_TEMPFILE_NAME);
  if (file === null) {
    throw new Error('Failed to create private video temp file.');
  }
  return file;
}

/**
 * Gets the picture entries.
 *
 * @return Promise for the picture entries.
 */
export async function getEntries(): Promise<FileAccessEntry[]> {
  assert(cameraDir !== null);
  const entries = await cameraDir.getFiles();
  return entries.filter((entry) => {
    if (!hasVideoPrefix(entry) && !hasImagePrefix(entry) &&
        !hasDocumentPrefix(entry)) {
      return false;
    }
    return entry.name.match(/_(\d{8})_(\d{6})(?: \((\d+)\))?/);
  });
}

/**
 * Returns an URL for a picture given by the file |entry|.
 *
 * @param entry The file entry of the picture.
 * @return Promise for the result.
 */
export async function pictureURL(entry: FileAccessEntry): Promise<string> {
  const file = await entry.file();
  return URL.createObjectURL(file);
}
