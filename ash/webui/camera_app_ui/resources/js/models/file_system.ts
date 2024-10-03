// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '../assert.js';
import {isFileSystemDirectoryHandle} from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {
  DOCUMENT_PREFIX,
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
import {isLocalDev} from './load_time_data.js';


/**
 * Checks if the given |entry|'s name has the video prefix.
 */
export function hasVideoPrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(VIDEO_PREFIX);
}

/**
 * Checks if the given |entry|'s name has the image prefix.
 */
function hasImagePrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(IMAGE_PREFIX);
}

/**
 * Checks if the given |entry|'s name has the document prefix.
 */
function hasDocumentPrefix(entry: FileAccessEntry): boolean {
  return entry.name.startsWith(DOCUMENT_PREFIX);
}

/**
 * Camera directory in the external file system.
 */
let cameraDir: DirectoryAccessEntry|null = null;

/**
 * Temporary directory which is hidden under the camera directory.
 */
let cameraTempDir: DirectoryAccessEntry|null = null;

/**
 * Gets camera directory used by CCA.
 */
export function getCameraDirectory(): DirectoryAccessEntry {
  assert(cameraDir !== null);
  return cameraDir;
}

/**
 * Initializes the temporary directory under camera directory.
 *
 * @return Promise for the directory result.
 */
async function initCameraTempDir(): Promise<DirectoryAccessEntry> {
  assert(cameraDir !== null);
  return getMaybeLazyDirectory(cameraDir, '.Temp');
}

/**
 * Initializes the camera directory in the external file system.
 *
 * @return Promise for the directory result.
 */
async function initCameraDirectory(): Promise<DirectoryAccessEntry> {
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
      const dir = launchParams.files[0];
      assert(isFileSystemDirectoryHandle(dir));

      await idb.set(idb.KEY_CAMERA_DIRECTORY_HANDLE, dir);
      window.sessionStorage.setItem('IsConsumedHandle', 'true');

      handle.signal(dir);
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
  if (isLocalDev()) {
    // TODO(pihsun): Add expert mode option for developer to point the camera
    // folder to a local folder.
    const root = await navigator.storage.getDirectory();
    cameraDir = await getMaybeLazyDirectory(
        new DirectoryAccessEntryImpl(root), 'Camera');
  } else {
    cameraDir = await initCameraDirectory();
  }

  cameraTempDir = await initCameraTempDir();
}

/**
 * Saves photo blob or metadata blob into predefined default location and
 * returns the file.
 *
 * @param blob Data of the photo to be saved.
 * @param name Filename of the photo to be saved.
 */
export async function saveBlob(
    blob: Blob, name: string): Promise<FileAccessEntry> {
  assert(cameraDir !== null);
  const file = await cameraDir.createFile(name);
  assert(file !== null);

  await file.write(blob);
  return file;
}

const PRIVATE_TEMPFILE_NAME = 'video-tmp.mp4';

/**
 * @throws If failed to create video temp file.
 */
export async function createPrivateTempVideoFile(name = PRIVATE_TEMPFILE_NAME):
    Promise<FileAccessEntry> {
  const dir = cameraTempDir;
  assert(dir !== null);

  // Deletes the previous temporary file if there is any.
  await dir.removeEntry(name);

  const file = await dir.createFile(name);
  if (file === null) {
    throw new Error('Failed to create private video temp file.');
  }
  return file;
}

/**
 * Gets the picture entries.
 */
export async function getEntries(): Promise<FileAccessEntry[]> {
  assert(cameraDir !== null);
  const entries = await cameraDir.getFiles();
  return entries.filter((entry) => {
    if (!hasVideoPrefix(entry) && !hasImagePrefix(entry) &&
        !hasDocumentPrefix(entry)) {
      return false;
    }
    return /_(\d{8})_(\d{6})(?: \((\d+)\))?/.exec(entry.name);
  });
}

/**
 * Returns an Object URL for a file `entry`.
 */
export async function getObjectURL(entry: FileAccessEntry): Promise<string> {
  const file = await entry.file();
  return URL.createObjectURL(file);
}
