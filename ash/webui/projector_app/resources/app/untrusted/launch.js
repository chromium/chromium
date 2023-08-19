// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/ash/common/assert.js';

const VIDEO_EXTENSIONS = [
  'webm',
];

const VIDEO_MIME_TYPES = [
  'video/webm',
];

/**
 * Callback to pass the launched file to application.
 * Callback is set when installLaunchHelper is called once
 * when communication is initialized in untrusted_app_comm_factory.js.
 * @type {Function<string, ?File, ?DOMException>}
 */
let sendVideoFile;

/**
 * Installs the handler for launch files, if window.launchQueue is available.
 * @param {!Function<string, ?File, ?DOMException>} callback
 */
export function installLaunchHandler(callback) {
  if (!window.launchQueue) {
    console.error('FileHandling API missing.');
    return;
  }

  sendVideoFile = callback;

  window.launchQueue.setConsumer(wrappedLaunchConsumer);
}

/**
 * Wrapper for the launch consumer to ensure it doesn't return a Promise, nor
 * propagate exceptions. Tests will want to target `launchConsumer` directly so
 * that they can properly await launch results.
 * @param {?LaunchParams} params
 */
function wrappedLaunchConsumer(params) {
  launchConsumer(params).catch(e => {
    console.error(e, '(launch aborted)');
  });
}

/**
 * The launchQueue consumer. This returns a promise to help tests, but the file
 * handling API will ignore it.
 * @param {?LaunchParams} params
 * @return {!Promise<undefined>}
 */
async function launchConsumer(params) {
  if (!params || !params.files || params.files.length !== 2) {
    console.error('Invalid launch (missing files): ', params);
    return;
  }
  // Caution! This first param is a file handler with the file id as its name,
  // not a real file. Don't try to access it on disk. Calling getFile() on it
  // will throw a DOM exception.
  const fileId = assert(params.files[0]).name;
  const fileHandle = assert(params.files[1]);
  try {
    await launchVideoFile(fileId, fileHandle);
  } catch (e) {
    console.error(e, '(launchFile aborted)');
  }
}

/**
 * Sends the provided video file to the.app via the
 * sendVideoFile callback.
 * @param {string} fileId
 * @param {!FileSystemHandle} handle
 */
async function launchVideoFile(fileId, handle) {
  try {
    const file = await getVideoFileFromHandle(handle);
    sendVideoFile(fileId, file, /*error=*/ null);
  } catch (/** @type {!DOMException} */ e) {
    console.error(`${handle.name}: ${e.message}`);
    sendVideoFile(fileId, /*file=*/ null, /*error=*/ e);
  }
}

/**
 * Gets a video file from a handle received via the fileHandling API. Only
 * handles expected to be video files should be passed to this function. Throws
 * a DOMException if opening the file fails - usually because the handle is
 * stale.
 * @param {?FileSystemHandle} fileSystemHandle
 * @return {!Promise<!File>}
 */
async function getVideoFileFromHandle(fileSystemHandle) {
  if (!fileSystemHandle || fileSystemHandle.kind !== 'file') {
    // Invent our own exception for this corner case. It might happen if a file
    // is deleted and replaced with a directory with the same name.
    throw new DOMException('Not a file.', 'NotAFile');
  }
  const handle = /** @type {!FileSystemFileHandle} */ (fileSystemHandle);
  const video = await handle.getFile();  // Note: throws DOMException.
  const extension = video.name.split('.').pop();
  if (!VIDEO_MIME_TYPES.includes(video.type) ||
      !VIDEO_EXTENSIONS.includes(extension)) {
    throw new DOMException('Not a video.', 'NotAVideo');
  }
  return video;
}
