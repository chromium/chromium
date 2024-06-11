// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import './unguessable_token.mojom-lite.js';
import './file_system_access_transfer_token.mojom-lite.js';
import './url.mojom-lite.js';

import {assertCast, MessagePipe} from '//system_apps/message_pipe.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import * as error_reporter from './error_reporter.js';
import {DeleteFileMessage, EditInPhotosMessage, FileContext, IsFileArcWritableMessage, IsFileBrowserWritableMessage, LoadFilesMessage, Message, NavigateMessage, NotifyCurrentFileMessage, OpenAllowedFileMessage, OpenAllowedFileResponse, OpenFilesWithPickerMessage, OverwriteFileMessage, OverwriteViaFilePickerResponse, RenameFileMessage, RenameResult, RequestSaveFileMessage, RequestSaveFileResponse, SaveAsMessage, SaveAsResponse} from './message_types.js';
import {mediaAppPageHandler} from './mojo_api_bootstrap.js';

// TODO(b/319570394): Replace these.
declare const blink: any;
declare const Mojo: any;

const DEFAULT_APP_ICON = 'app';
const EMPTY_WRITE_ERROR_NAME = 'EmptyWriteError';

// Open file picker configurations. Should be kept in sync with launch handler
// configurations in media_web_app_info.cc.
const AUDIO_EXTENSIONS =
    ['.flac', '.m4a', '.mp3', '.oga', '.ogg', '.opus', '.wav', '.weba', '.m4a'];
const IMAGE_EXTENSIONS = [
  '.jpg',  '.png', '.webp', '.gif', '.avif', '.bmp',   '.ico', '.svg',
  '.jpeg', '.jpe', '.jfif', '.jif', '.jfi',  '.pjpeg', '.pjp', '.arw',
  '.cr2',  '.dng', '.nef',  '.nrw', '.orf',  '.raf',   '.rw2', '.svgz',
];
const VIDEO_EXTENSIONS = [
  '.3gp',
  '.avi',
  '.m4v',
  '.mkv',
  '.mov',
  '.mp4',
  '.mpeg',
  '.mpeg4',
  '.mpg',
  '.mpg4',
  '.ogv',
  '.ogx',
  '.ogm',
  '.webm',
];
const PDF_EXTENSIONS = ['.pdf'];
const OPEN_ACCEPT_ARGS: {[index: string]: FilePickerAcceptType} = {
  'AUDIO': {
    description: loadTimeData.getString('fileFilterAudio'),
    accept: {'audio/*': AUDIO_EXTENSIONS},
  },
  'IMAGE': {
    description: loadTimeData.getString('fileFilterImage'),
    accept: {'image/*': IMAGE_EXTENSIONS},
  },
  'VIDEO': {
    description: loadTimeData.getString('fileFilterVideo'),
    accept: {'video/*': VIDEO_EXTENSIONS},
  },
  'PDF': {description: 'PDF', accept: {'application/pdf': PDF_EXTENSIONS}},
  // All supported file types, excluding text files (see b/183150750).
  'ALL_EX_TEXT': {
    description: 'All',
    accept: {
      '*/*': [
        ...AUDIO_EXTENSIONS,
        ...IMAGE_EXTENSIONS,
        ...VIDEO_EXTENSIONS,
        ...PDF_EXTENSIONS,
      ],
    },
  },
};

/**
 * Sort order for files in the navigation ring.
 */
enum SortOrder {
  /**
   * Lexicographic (with natural number ordering): advancing goes "down" the
   * alphabet.
   */
  A_FIRST = 1,
  /**
   * Reverse lexicographic (with natural number ordering): advancing goes "up"
   * the alphabet.
   */
  Z_FIRST = 2,
  /** By modified time: pressing "right" goes to older files. */
  NEWEST_FIRST = 3,
}

/**
 * Wrapper around a file handle that allows the privileged context to arbitrate
 * read and write access as well as file navigation. `token` uniquely identifies
 * the file, `file` temporarily holds the object passed over postMessage, and
 * `handle` allows it to be reopened upon navigation. If an error occurred on
 * the last attempt to open `handle`, `lastError` holds the error name.
 */
interface FileDescriptor {
  token: number;
  file: File|null;
  handle: FileSystemFileHandle;
  lastError?: string;
  inCurrentDirectory?: boolean;
}

/**
 * Array of entries available in the current directory.
 */
const currentFiles: FileDescriptor[] = [];

/**
 * A variable for storing the name of the app, taken from the <title>. We store
 * it here since we mutate the title to show filename, but may want to restore
 * it in some circumstances i.e. returning to zero state.
 */
let appTitle: string|undefined;

/**
 * The current sort order.
 * TODO(crbug.com/40384768): Match the file manager order when launched that way.
 * Note currently this is reassigned in tests.
 */
// eslint-disable-next-line prefer-const
let sortOrder = SortOrder.A_FIRST;

/**
 * Index into `currentFiles` of the current file.
 */
let entryIndex = -1;

/**
 * Keeps track of the current launch (i.e. call to `launchWithDirectory`) .
 * Since file loading can be deferred i.e. we can load the first focused file
 * and start using the app then load other files in `loadOtherRelatedFiles()` we
 * need to make sure `loadOtherRelatedFiles` gets aborted if it is out of date
 * i.e. in interleaved launches.
 */
let globalLaunchNumber = -1;

/**
 * Reference to the directory handle that contains the first file in the most
 * recent launch event.
 */
let currentDirectoryHandle: FileSystemDirectoryHandle|null = null;

/**
 * Map of file tokens. Persists across new launch requests from the file
 * manager when chrome://media-app has not been closed.
 */
const tokenMap = new Map<number, FileSystemFileHandle>();

/**
 * A pipe through which we can send messages to the guest frame.
 * Use an undefined `target` to find the <iframe> automatically.
 * Do not rethrow errors, since handlers installed here are expected to
 * throw exceptions that are handled on the other side of the pipe. And
 * nothing `awaits` async callHandlerForMessageType_(), so they will always
 * be reported as `unhandledrejection` and trigger a crash report.
 */
const guestMessagePipe =
    new MessagePipe('chrome-untrusted://media-app', undefined, false);

// Register a handler for the "IFRAME_READY" message which does nothing. This
// prevents MessagePipe emitting an error that there is no handler for it. The
// message is handled by logic in first_message_received.js, which installs the
// event listener before the <iframe> is added to the DOM.
guestMessagePipe.registerHandler(Message.IFRAME_READY, () => {});

/**
 * The type of icon to show for this app's window.
 */
let appIconType = DEFAULT_APP_ICON;

/**
 * Sets the app icon depending on the icon type and color theme.
 * @param mediaQueryList Determines whether or not the icon should be in dark
 *     mode.
 */
function updateAppIcon(mediaQueryList: MediaQueryList|
                       (Event & {matches: boolean})) {
  // The default app icon does not have a separate dark variant.
  const isDark =
      mediaQueryList.matches && appIconType !== DEFAULT_APP_ICON ? '_dark' : '';

  const icon = document.querySelector<HTMLLinkElement>('link[rel=icon]');
  icon!.href = `system_assets/${appIconType}_icon${isDark}.svg`;
}

const darkMediaQuery = window.matchMedia('(prefers-color-scheme: dark)');

guestMessagePipe.registerHandler(Message.NOTIFY_CURRENT_FILE, (message) => {
  const notifyMsg: NotifyCurrentFileMessage = message;

  const title = document.querySelector('title')!;
  appTitle = appTitle || title.text;
  title.text = notifyMsg.name || appTitle;

  appIconType = notifyMsg.type ? notifyMsg.type.split('/')[0]! : 'file';
  if (title.text === appTitle) {
    appIconType = DEFAULT_APP_ICON;
  } else if (notifyMsg.type === 'application/pdf') {
    appIconType = 'pdf';
  } else if (!['audio', 'image', 'video', 'file'].includes(appIconType)) {
    appIconType = 'file';
  }
  updateAppIcon(darkMediaQuery);
});

darkMediaQuery.addEventListener('change', updateAppIcon);

guestMessagePipe.registerHandler(Message.OPEN_FEEDBACK_DIALOG, () => {
  let response = mediaAppPageHandler.openFeedbackDialog();
  if (response === null) {
    response = {errorMessage: 'Null response received'};
  }
  return response;
});

guestMessagePipe.registerHandler(Message.TOGGLE_BROWSER_FULLSCREEN_MODE, () => {
  mediaAppPageHandler.toggleBrowserFullscreenMode();
});

guestMessagePipe.registerHandler(
    Message.OPEN_IN_SANDBOXED_VIEWER, (message) => {
      window.open(
          `./viewpdfhost.html?${new URLSearchParams(message)}`, '_blank',
          'popup=1');
    });

guestMessagePipe.registerHandler(Message.RELOAD_MAIN_FRAME, () => {
  window.location.reload();
});

guestMessagePipe.registerHandler(Message.MAYBE_TRIGGER_PDF_HATS, () => {
  mediaAppPageHandler.maybeTriggerPdfHats();
});

guestMessagePipe.registerHandler(Message.EDIT_IN_PHOTOS, (message) => {
  const editInPhotosMsg: EditInPhotosMessage = message;
  const fileHandle = fileHandleForToken(editInPhotosMsg.token);

  const transferToken = new blink.mojom.FileSystemAccessTransferTokenRemote(
      Mojo.getFileSystemAccessTransferToken(fileHandle));

  return mediaAppPageHandler.editInPhotos(
      transferToken, editInPhotosMsg.mimeType);
});

guestMessagePipe.registerHandler(Message.IS_FILE_ARC_WRITABLE, (message) => {
  const writableMsg: IsFileArcWritableMessage = message;
  const fileHandle = fileHandleForToken(writableMsg.token);

  const transferToken = new blink.mojom.FileSystemAccessTransferTokenRemote(
      Mojo.getFileSystemAccessTransferToken(fileHandle));

  return mediaAppPageHandler.isFileArcWritable(transferToken);
});

guestMessagePipe.registerHandler(
    Message.IS_FILE_BROWSER_WRITABLE, (message) => {
      const writableMsg: IsFileBrowserWritableMessage = message;
      const fileHandle = fileHandleForToken(writableMsg.token);

      const transferToken = new blink.mojom.FileSystemAccessTransferTokenRemote(
          Mojo.getFileSystemAccessTransferToken(fileHandle));

      return mediaAppPageHandler.isFileBrowserWritable(transferToken);
    });

guestMessagePipe.registerHandler(
    Message.OVERWRITE_FILE,
    async(message): Promise<void|OverwriteViaFilePickerResponse> => {
      const overwrite: OverwriteFileMessage = message;
      const originalHandle = fileHandleForToken(overwrite.token);
      try {
        await saveBlobToFile(originalHandle, overwrite.blob);
      } catch (e: any) {
        if (e.name === EMPTY_WRITE_ERROR_NAME) {
          throw e;
        }
        // TODO(b/160843424): Collect UMA.
        console.warn('Showing a picker due to', e);
        return pickFileForFailedOverwrite(
            originalHandle.name, e.name, overwrite);
      }
    });

guestMessagePipe.registerHandler(
    Message.SUBMIT_FORM,
    async (message) => {
      mediaAppPageHandler.submitForm(
          {url: message.url}, message.payload, message.header);
    },
);

/**
 * Shows a file picker and redirects a failed OverwriteFileMessage to the chosen
 * file. Updates app state and rebinds file tokens if the write is successful.
 */
async function pickFileForFailedOverwrite(
    fileName: string, errorName: string,
    overwrite: OverwriteFileMessage): Promise<OverwriteViaFilePickerResponse> {
  const fileHandle = await pickWritableFile(
      fileName, overwrite.blob.type, overwrite.token, []);
  await saveBlobToFile(fileHandle, overwrite.blob);

  // Success. Replace the old handle.
  tokenMap.set(overwrite.token, fileHandle);
  const entry = currentFiles.find(i => i.token === overwrite.token);
  if (entry) {
    entry.handle = fileHandle;
  }
  return {renamedTo: fileHandle.name, errorName};
}

guestMessagePipe.registerHandler(Message.DELETE_FILE, async (message) => {
  const deleteMsg: DeleteFileMessage = message;
  const {handle, directory} =
      assertFileAndDirectoryMutable(deleteMsg.token, 'Delete');

  if (!(await isHandleInCurrentDirectory(handle))) {
    // removeEntry() silently "succeeds" in this case, but that gives poor UX.
    console.warn(`"${handle.name}" not found in the last opened folder.`);
    const error = new Error('Ignoring delete request: file not found');
    error.name = 'NotFoundError';
    throw error;
  }

  await directory.removeEntry(handle.name);

  // Remove the file that was deleted.
  currentFiles.splice(entryIndex, 1);

  // Attempts to load the file to the right which is at now at
  // `currentFiles[entryIndex]`, where `entryIndex` was previously the index of
  // the deleted file.
  await advance(0);
});

/** Handler to rename the currently focused file. */
guestMessagePipe.registerHandler(Message.RENAME_FILE, async (message) => {
  const renameMsg: RenameFileMessage = message;
  const {handle, directory} =
      assertFileAndDirectoryMutable(renameMsg.token, 'Rename');

  if (await filenameExistsInCurrentDirectory(renameMsg.newFilename)) {
    return {renameResult: RenameResult.FILE_EXISTS};
  }

  const originalFile = await maybeGetFileFromFileHandle(handle);
  let originalFileIndex =
      currentFiles.findIndex(fd => fd.token === renameMsg.token);

  if (!originalFile || originalFileIndex < 0) {
    return {renameResult: RenameResult.FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY};
  }

  const renamedFileHandle =
      await directory.getFileHandle(renameMsg.newFilename, {create: true});
  // Copy file data over to the new file.
  const writer = await renamedFileHandle.createWritable();
  const sink: WritableStream<any> = writer;
  const source: {stream: () => ReadableStream} = originalFile;
  await source.stream().pipeTo(sink);

  // Remove the old file since the new file has all the data & the new name.
  // Note even though removing an entry that doesn't exist is considered
  // success, we first check `handle` is the same as the handle for the file
  // with that filename in the `currentDirectoryHandle`.
  if (await isHandleInCurrentDirectory(handle)) {
    await directory.removeEntry(originalFile.name);
  }

  // Replace the old file in our internal representation. There is no harm using
  // the old file's token since the old file is removed.
  tokenMap.set(renameMsg.token, renamedFileHandle);
  // Remove the entry for `originalFile` in current files, replace it with a
  // FileDescriptor for the renamed file.

  // Ensure the file is still in `currentFiles` after all the above `awaits`. If
  // missing it means either new files have loaded (or tried to), see
  // b/164985809.
  originalFileIndex =
      currentFiles.findIndex(fd => fd.token === renameMsg.token);

  if (originalFileIndex < 0) {
    // Can't navigate to the renamed file so don't add it to `currentFiles`.
    return {renameResult: RenameResult.SUCCESS};
  }

  currentFiles.splice(originalFileIndex, 1, {
    token: renameMsg.token,
    file: null,
    handle: renamedFileHandle,
    inCurrentDirectory: true,
  });

  return {renameResult: RenameResult.SUCCESS};
});

guestMessagePipe.registerHandler(Message.NAVIGATE, async (message) => {
  const navigate: NavigateMessage = message;

  await advance(navigate.direction, navigate.currentFileToken);
});

guestMessagePipe.registerHandler(Message.REQUEST_SAVE_FILE, async (message) => {
  const {suggestedName, mimeType, startInToken, accept} =
      message as RequestSaveFileMessage;
  const handle =
      await pickWritableFile(suggestedName, mimeType, startInToken, accept);

  const response: RequestSaveFileResponse = {
    pickedFileContext: {
      token: generateToken(handle),
      file: assertCast(await handle.getFile()),
      name: handle.name,
      error: '',
      canDelete: false,
      canRename: false,
    },
  };
  return response;
});

guestMessagePipe.registerHandler(Message.SAVE_AS, async (message) => {
  const {blob, oldFileToken, pickedFileToken} = message as SaveAsMessage;
  const oldFileDescriptor = currentFiles.find(fd => fd.token === oldFileToken);
  const pickedHandle = assertCast(tokenMap.get(pickedFileToken));
  const pickedFileDescriptor: FileDescriptor = {
    // We silently take over the old file's file descriptor by taking its token,
    // note we can be passed an undefined token if the file we are saving was
    // dragged into the media app.
    token: oldFileToken || tokenGenerator.next().value,
    file: null,
    handle: pickedHandle,
  };
  const oldFileIndex = currentFiles.findIndex(fd => fd.token === oldFileToken);
  tokenMap.set(pickedFileDescriptor.token, pickedHandle);
  // Give the old file a new token, if we couldn't find the old file we assume
  // its been deleted (or pasted/dragged into the media app) and skip this
  // step.
  if (oldFileDescriptor) {
    oldFileDescriptor.token = generateToken(oldFileDescriptor.handle);
  }
  try {
    // Note `pickedFileHandle` could be the same as a `FileSystemFileHandle`
    // that exists in `tokenMap`. Possibly even the `File` currently open. But
    // that's OK. E.g. the next overwrite-file request will just invoke
    // `saveBlobToFile` in the same way.
    await saveBlobToFile(pickedHandle, blob);
  } catch (e: unknown) {
    // If something went wrong revert the token back to its original
    // owner so future file actions function correctly.
    if (oldFileDescriptor && oldFileToken) {
      oldFileDescriptor.token = oldFileToken;
      tokenMap.set(oldFileToken, oldFileDescriptor.handle);
    }
    throw e;
  }

  // Note: oldFileIndex may be `-1` here which causes the new file to be added
  // to the start of the array, this is WAI.
  currentFiles.splice(oldFileIndex + 1, 0, pickedFileDescriptor);
  // Silently update entry index without triggering a reload of the media app.
  entryIndex = oldFileIndex + 1;

  const response: SaveAsResponse = {newFilename: pickedHandle.name};
  return response;
});

guestMessagePipe.registerHandler(Message.OPEN_FILES_WITH_PICKER, async (m) => {
  const {startInToken, accept, isSingleFile} = m as OpenFilesWithPickerMessage;
  const acceptTypes = accept.map(k => OPEN_ACCEPT_ARGS[k]).filter(a => !!a) as
      FilePickerAcceptType[];

  const options: OpenFilePickerOptions = {multiple: !isSingleFile};

  if (startInToken) {
    options.startIn = fileHandleForToken(startInToken);
  }

  if (acceptTypes.length > 0) {
    options.excludeAcceptAllOption = true;
    options.types = acceptTypes;
  }

  const handles = await window.showOpenFilePicker!(options);
  const newDescriptors: FileDescriptor[] = [];
  for (const handle of handles) {
    newDescriptors.push({
      token: generateToken(handle),
      file: null,
      handle: handle,
      inCurrentDirectory: false,
    });
  }
  if (newDescriptors.length === 0) {
    // Be defensive against the file picker returning an empty array rather than
    // throwing an abort exception. Or any filtering we may introduce.
    return;
  }

  // Perform a full "relaunch": replace everything and set focus to index 0.
  currentFiles.splice(0, currentFiles.length, ...newDescriptors);
  entryIndex = 0;
  await sendSnapshotToGuest([...currentFiles], ++globalLaunchNumber);
});

guestMessagePipe.registerHandler(Message.OPEN_ALLOWED_FILE, async (message) => {
  const {fileToken} = message as OpenAllowedFileMessage;
  const handle = fileHandleForToken(fileToken);
  const response:
      OpenAllowedFileResponse = {file: (await getFileFromHandle(handle)).file};
  return response;
});

/**
 * Shows a file picker to get a writable file.
 */
function pickWritableFile(
    suggestedName: string, mimeType: string, startInToken: number,
    accept: string[]): Promise<FileSystemFileHandle> {
  const JPG_EXTENSIONS =
      ['.jpg', '.jpeg', '.jpe', '.jfif', '.jif', '.jfi', '.pjpeg', '.pjp'];
  const ACCEPT_ARGS: {[index: string]: FilePickerAcceptType} = {
    'JPG': {description: 'JPG', accept: {'image/jpeg': JPG_EXTENSIONS}},
    'PNG': {description: 'PNG', accept: {'image/png': ['.png']}},
    'WEBP': {description: 'WEBP', accept: {'image/webp': ['.webp']}},
    'PDF': {description: 'PDF', accept: {'application/pdf': ['.pdf']}},
  };
  const acceptTypes = accept.map(k => ACCEPT_ARGS[k]).filter(a => !!a) as
      FilePickerAcceptType[];

  const options: SaveFilePickerOptions = {
    suggestedName,
  };

  if (startInToken) {
    options.startIn = fileHandleForToken(startInToken);
  }

  if (acceptTypes.length > 0) {
    options.excludeAcceptAllOption = true;
    options.types = acceptTypes;
  } else {
    // Search for the mimeType, and add a single entry. If none is found, the
    // file picker is left "unconfigured"; with just "all files".
    for (const a of Object.values(ACCEPT_ARGS)) {
      if (a.accept[mimeType]) {
        options.excludeAcceptAllOption = true;
        options.types = [a];
      }
    }
  }

  // This may throw an error, but we can handle and recover from it on the
  // unprivileged side.
  return window.showSaveFilePicker!(options);
}

/**
 * Generator instance for unguessable tokens.
 */
const tokenGenerator: Generator<number> = (function*() {
  // To use the regular number type, tokens must stay below
  // Number.MAX_SAFE_INTEGER (2^53). So stick with ~33 bits. Note we can not
  // request more than 64kBytes from crypto.getRandomValues() at a time.
  const randomBuffer = new Uint32Array(1000);
  while (true) {
    assertCast(crypto).getRandomValues(randomBuffer);
    for (let i = 0; i < randomBuffer.length; ++i) {
      const token = randomBuffer[i];
      // Disallow "0" as a token.
      if (token && !tokenMap.has(token)) {
        yield Number(token);
      }
    }
  }
})();

/**
 * Generate a file token, and persist the mapping to `handle`.
 */
function generateToken(handle: FileSystemFileHandle): number {
  const token = tokenGenerator.next().value;
  tokenMap.set(token, handle);
  return token;
}

/**
 * Return the mimetype of a file given it's filename. Returns null if the
 * mimetype could not be determined or if the file does not have a extension.
 * TODO(b/178986064): Remove this once we have a file system access metadata
 * api.
 */
function getMimeTypeFromFilename(filename: string): string|null {
  // This file extension to mime type map is adapted from
  // https://source.chromium.org/chromium/chromium/src/+/main:net/base/mime_util.cc;l=147;drc=51373c4ea13372d7711c59d9929b0be5d468633e
  const mapping: {[index: string]: string} = {
    'avif': 'image/avif',
    'crx': 'application/x-chrome-extension',
    'css': 'text/css',
    'flac': 'audio/flac',
    'gif': 'image/gif',
    'htm': 'text/html',
    'html': 'text/html',
    'jpeg': 'image/jpeg',
    'jpg': 'image/jpeg',
    'js': 'text/javascript',
    'm4a': 'audio/x-m4a',
    'm4v': 'video/mp4',
    'mht': 'multipart/related',
    'mhtml': 'multipart/related',
    'mjs': 'text/javascript',
    'mp3': 'audio/mpeg',
    'mp4': 'video/mp4',
    'oga': 'audio/ogg',
    'ogg': 'audio/ogg',
    'ogm': 'video/ogg',
    'ogv': 'video/ogg',
    'opus': 'audio/ogg',
    'png': 'image/png',
    'shtm': 'text/html',
    'shtml': 'text/html',
    'wasm': 'application/wasm',
    'wav': 'audio/wav',
    'webm': 'video/webm',
    'webp': 'image/webp',
    'xht': 'application/xhtml+xml',
    'xhtm': 'application/xhtml+xml',
    'xhtml': 'application/xhtml+xml',
    'xml': 'text/xml',
    'epub': 'application/epub+zip',
    'woff': 'application/font-woff',
    'gz': 'application/gzip',
    'tgz': 'application/gzip',
    'json': 'application/json',
    'bin': 'application/octet-stream',
    'exe': 'application/octet-stream',
    'com': 'application/octet-stream',
    'pdf': 'application/pdf',
    'p7m': 'application/pkcs7-mime',
    'p7c': 'application/pkcs7-mime',
    'p7z': 'application/pkcs7-mime',
    'p7s': 'application/pkcs7-signature',
    'ps': 'application/postscript',
    'eps': 'application/postscript',
    'ai': 'application/postscript',
    'rdf': 'application/rdf+xml',
    'rss': 'application/rss+xml',
    'apk': 'application/vnd.android.package-archive',
    'xul': 'application/vnd.mozilla.xul+xml',
    'xls': 'application/vnd.ms-excel',
    'xlsx': 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet',
    'zip': 'application/zip',
    'weba': 'audio/webm',
    'bmp': 'image/bmp',
    'jfif': 'image/jpeg',
    'pjpeg': 'image/jpeg',
    'pjp': 'image/jpeg',
    'svg': 'image/svg+xml',
    'svgz': 'image/svg+xml',
    'tiff': 'image/tiff',
    'tif': 'image/tiff',
    'ico': 'image/vnd.microsoft.icon',
    'eml': 'message/rfc822',
    'ics': 'text/calendar',
    'ehtml': 'text/html',
    'txt': 'text/plain',
    'text': 'text/plain',
    'sh': 'text/x-sh',
    'xsl': 'text/xml',
    'xbl': 'text/xml',
    'xslt': 'text/xml',
    'mpeg': 'video/mpeg',
    'mpg': 'video/mpeg',

    // Add more video file types. These are not web-supported types, but are
    // supported on ChromeOS, and have file handlers in media_web_app_info.cc.
    'mkv': 'video/x-matroska',
    '3gp': 'video/3gpp',
    'mov': 'video/quicktime',
    'avi': 'video/x-msvideo',
    'mpeg4': 'video/mp4',
    'mpg4': 'video/mp4',
  };

  const fileParts = filename.split('.');
  if (fileParts.length < 2) {
    return null;
  }
  const extension = fileParts[fileParts.length - 1]!.toLowerCase();
  const mimeType = mapping[extension];
  return mimeType !== undefined ? mimeType : null;
}

/**
 * Returns the `FileSystemFileHandle` for the given `token`. This is
 * "guaranteed" to succeed: tokens are only generated once a file handle has
 * been successfully opened at least once (and determined to be "related"). The
 * handle doesn't expire, but file system operations may fail later on.
 * One corner case, however, is when the initial file open fails and the token
 * gets replaced by `-1`. File operations all need to fail in that case.
 */
function fileHandleForToken(token: number): FileSystemFileHandle {
  const handle = tokenMap.get(token);
  if (!handle) {
    throw new DOMException(`No handle for token(${token})`, 'NotFoundError');
  }
  return handle;
}

/**
 * Saves the provided blob the provided fileHandle. Assumes the handle is
 * writable.
 */
async function saveBlobToFile(
    handle: FileSystemFileHandle, data: Blob): Promise<void> {
  if (data.size === 0) {
    // Bugs or error states in the app could cause an unexpected write of zero
    // bytes to a file, which could cause data loss. Reject it here.
    const error = new Error('saveBlobToFile(): Refusing to write zero bytes.');
    error.name = EMPTY_WRITE_ERROR_NAME;
    throw error;
  }
  const writer = await handle.createWritable();
  await writer.write(data);
  await writer.truncate(data.size);
  await writer.close();
}

/**
 * Warns if a given exception is "uncommon". That is, one that the guest might
 * not provide UX for and should be dumped to console to give additional
 * context.
 */
function warnIfUncommon(e: DOMException, fileName: string) {
  // Errors we expect to be thrown in normal operation.
  const commonErrors = ['NotFoundError', 'NotAllowedError', 'NotAFile'];
  if (commonErrors.includes(e.name)) {
    return;
  }
  console.warn(`Unexpected ${e.name} on ${fileName}: ${e.message}`);
}

/**
 * If `fd.file` is null, re-opens the file handle in `fd`.
 */
async function refreshFile(fd: FileDescriptor) {
  if (fd.file) {
    return;
  }
  fd.lastError = '';
  try {
    fd.file = (await getFileFromHandle(fd.handle)).file;
  } catch (e: any) {
    fd.lastError = e.name;
    // A failure here is only a problem for the "current" file (and that needs
    // to be handled in the unprivileged context), so ignore known errors.
    warnIfUncommon(e, fd.handle.name);
  }
}

/**
 * Loads the current file list into the guest.
 */
async function sendFilesToGuest(): Promise<void> {
  return sendSnapshotToGuest(
      [...currentFiles], globalLaunchNumber);  // Shallow copy.
}

/**
 * Converts a file descriptor from `currentFiles` into a `FileContext` used by
 * the LoadFilesMessage. Closure forgets that some fields may be missing without
 * naming the type explicitly on the signature here.
 */
function fileDescriptorToFileContext(fd: FileDescriptor): FileContext {
  // TODO(b/163285659): Properly detect files that can't be renamed/deleted.
  return {
    token: fd.token,
    file: fd.file,
    name: fd.handle.name,
    error: fd.lastError || '',
    canDelete: fd.inCurrentDirectory || false,
    canRename: fd.inCurrentDirectory || false,
  };
}

/**
 * Loads the provided file list into the guest.
 * Note: code paths can defer loads i.e. `launchWithDirectory()` increment
 * `globalLaunchNumber` to ensure their deferred load is still relevant when it
 * finishes processing. Other code paths that call `sendSnapshotToGuest()` don't
 * have to.
 */
async function sendSnapshotToGuest(
    snapshot: FileDescriptor[], localLaunchNumber: number,
    extraFiles: boolean = false): Promise<void> {
  const focusIndex = entryIndex;

  // Attempt to reopen the focus file only. In future we might also open
  // "nearby" files for preloading. However, reopening *all* files on every
  // navigation attempt to verify they can still be navigated to adds noticeable
  // lag in large directories.
  let targetIndex = -1;
  if (focusIndex >= 0 && focusIndex < snapshot.length) {
    targetIndex = focusIndex;
  } else if (snapshot.length !== 0) {
    targetIndex = 0;
  }
  if (targetIndex >= 0) {
    const descriptor = snapshot[targetIndex];
    await refreshFile(descriptor!);
    await refreshLoadRequiredAssociatedFiles(
        snapshot, descriptor!.handle.name, extraFiles);
    if (extraFiles) {
      snapshot.shift();
    }
  }

  if (localLaunchNumber !== globalLaunchNumber) {
    return;
  }
  const loadFilesMessage: LoadFilesMessage = {
    currentFileIndex: focusIndex,
    // Handle can't be passed through a message pipe.
    files: snapshot.map(fileDescriptorToFileContext),
  };

  // Clear handles to the open files in the privileged context so they are
  // refreshed on a navigation request. The refcount to the File will be alive
  // in the postMessage object until the guest takes its own reference.
  for (const fd of snapshot) {
    fd.file = null;
  }

  // Wait for the signal from first_message_received.js before proceeding.
  await window.firstMessageReceived;

  if (extraFiles) {
    await guestMessagePipe.sendMessage(
        Message.LOAD_EXTRA_FILES, loadFilesMessage);
  } else {
    await guestMessagePipe.sendMessage(Message.LOAD_FILES, loadFilesMessage);
  }
}

/**
 * Throws an error if the file or directory handles don't exist or the token for
 * the file to be mutated is incorrect.
 */
function assertFileAndDirectoryMutable(
    editFileToken: number, operation: string):
    {handle: FileSystemFileHandle, directory: FileSystemDirectoryHandle} {
  if (!currentDirectoryHandle) {
    throw new Error(`${operation} failed. File without launch directory.`);
  }

  return {
    handle: fileHandleForToken(editFileToken),
    directory: currentDirectoryHandle,
  };
}

/**
 * Returns whether `handle` is in `currentDirectoryHandle`. Prevents mutating a
 * file that doesn't exist.
 */
async function isHandleInCurrentDirectory(handle: FileSystemFileHandle):
    Promise<boolean> {
  const file: File|null = await maybeGetFileFromFileHandle(handle);
  // If we were unable to get a file from the handle it must not be in the
  // current directory anymore.
  if (!file) {
    return false;
  }

  // It's unclear if getFile will always give us a NotFoundError if the file has
  // been moved as it's not explicitly stated in the File System Access API
  // spec. As such we perform an additional check here to make sure the file
  // returned by the handle is in fact in the current directory.
  // TODO(b/172628918): Remove this once we have more assurances getFile() does
  // the right thing.
  const currentFilename = file.name;
  const fileHandle = await getFileHandleFromCurrentDirectory(currentFilename);
  return fileHandle ? fileHandle.isSameEntry(handle) : false;
}

/**
 * Returns if a`filename` exists in `currentDirectoryHandle`.
 */
async function filenameExistsInCurrentDirectory(filename: string):
    Promise<boolean> {
  return (await getFileHandleFromCurrentDirectory(filename, true)) !== null;
}

/**
 * Returns the `FileSystemFileHandle` for `filename` if it exists in the current
 * directory, otherwise null.
 */
async function getFileHandleFromCurrentDirectory(
    filename: string, suppressError = false): Promise<FileSystemHandle|null> {
  if (!currentDirectoryHandle) {
    return null;
  }
  try {
    return (
        await currentDirectoryHandle.getFileHandle(filename, {create: false}));
  } catch (e: any) {
    if (!suppressError) {
      // Some filenames (e.g. "thumbs.db") can't be opened (or deleted) by
      // filename. TypeError doesn't give a good error message in the app, so
      // convert to a new Error.
      if (e.name === 'TypeError' &&
          e.message ===
              'Failed to execute \'getFileHandle\' on ' +
                  '\'FileSystemDirectoryHandle\': Name is not allowed.') {
        console.warn(e);  // Warn so a crash report is not generated.
        throw new DOMException(
            'File has a reserved name and can not be opened',
            'InvalidModificationError');
      }
      console.error(e);
    }
    return null;
  }
}

/**
 * Gets a file from a handle received via the fileHandling API. Only handles
 * expected to be files should be passed to this function. Throws a DOMException
 * if opening the file fails - usually because the handle is stale.
 */
async function getFileFromHandle(fileSystemHandle: FileSystemHandle):
    Promise<{file: File, handle: FileSystemFileHandle}> {
  if (!fileSystemHandle || fileSystemHandle.kind !== 'file') {
    // Invent our own exception for this corner case. It might happen if a file
    // is deleted and replaced with a directory with the same name.
    throw new DOMException('Not a file.', 'NotAFile');
  }
  const handle = fileSystemHandle as FileSystemFileHandle;
  const file = await handle.getFile();  // Note: throws DOMException.
  return {file, handle};
}

/**
 * Calls getFile on `handle` and gracefully returns null if it encounters a
 * NotFoundError, which can happen if the file is no longer in the current
 * directory due to being moved or deleted.
 */
async function maybeGetFileFromFileHandle(handle: FileSystemFileHandle):
    Promise<File|null> {
  let file: File|null;
  try {
    file = await handle.getFile();
  } catch (e: any) {
    // NotFoundError can be thrown if `handle` is no longer in the directory we
    // have access to.
    if (e.name === 'NotFoundError') {
      file = null;
    } else {
      // Any other error is unexpected.
      throw e;
    }
  }
  return file;
}

/**
 * Returns whether `fileName` is a file potentially containing subtitles.
 */
function isSubtitleFile(fileName: string): boolean {
  return /\.vtt$/.test(fileName.toLowerCase());
}

/**
 * Returns whether `fileName` is a file likely to be a video.
 */
function isVideoFile(fileName: string): boolean {
  return /^video\//.test(getMimeTypeFromFilename(fileName) ?? '');
}

/**
 * Returns whether `fileName` is a file likely to be an image.
 */
function isImageFile(fileName: string): boolean {
  // Detect RAW images, which often don't have a mime type set.
  return /\.(arw|cr2|dng|nef|nrw|orf|raf|rw2)$/.test(fileName.toLowerCase()) ||
      /^image\//.test(getMimeTypeFromFilename(fileName) ?? '');
}

/**
 * Returns whether `fileName` is a file likely to be audio.
 */
function isAudioFile(fileName: string): boolean {
  return /^audio\//.test(getMimeTypeFromFilename(fileName) ?? '');
}

/**
 * Returns whether fileName is the filename for a video or image, or a related
 * file type (e.g. video subtitles).
 */
function isVideoOrImage(fileName: string): boolean {
  return isImageFile(fileName) || isVideoFile(fileName) ||
      isSubtitleFile(fileName);
}

/**
 * Returns whether `siblingFile` is related to `focusFile`. That is, whether
 * they should be traversable from one another. Usually this means they share a
 * similar (non-empty) MIME type.
 * @param focusFile The file selected by the user.
 * @param siblingFileName Filename for a file in the same directory as
 *     `focusFile`.
 */
function isFileRelated(focusFile: File, siblingFileName: string): boolean {
  const siblingFileType = getMimeTypeFromFilename(siblingFileName);
  return focusFile.name === siblingFileName ||
      (!!focusFile.type && !!siblingFileType &&
       focusFile.type === siblingFileType) ||
      (isVideoOrImage(focusFile.name) && isVideoOrImage(siblingFileName));
}

/**
 * Enum like return value of `processOtherFilesInDirectory()`.
 */
enum ProcessOtherFilesResult {
  // Newer load in progress, can abort loading these files.
  ABORT = -2,
  // The focusFile is missing, treat this as a normal load.
  FOCUS_FILE_MISSING = -1,
  // The focusFile is present, load these files as extra files.
  FOCUS_FILE_RELEVANT = 0,
}

/**
 * Loads related files the working directory to initialize file iteration
 * according to the type of the opened file. If `globalLaunchNumber` changes
 * (i.e. another launch occurs), this will abort early and not change
 * `currentFiles`.
 */
async function processOtherFilesInDirectory(
    directory: FileSystemDirectoryHandle, focusFile: File|null,
    localLaunchNumber: number): Promise<ProcessOtherFilesResult> {
  if (!focusFile || !focusFile.name) {
    return ProcessOtherFilesResult.ABORT;
  }

  let relatedFiles: FileDescriptor[] = [];
  // TODO(b/158149714): Clear out old tokens as well? Care needs to be taken to
  // ensure any file currently open with unsaved changes can still be saved.
  try {
    for await (const handle of directory.values()) {
      if (localLaunchNumber !== globalLaunchNumber) {
        // Abort, another more up to date launch in progress.
        return ProcessOtherFilesResult.ABORT;
      }

      if (handle.kind !== 'file') {
        continue;
      }
      const fileHandle = handle as FileSystemFileHandle;
      // Only allow traversal of related file types.
      if (isFileRelated(focusFile, handle.name)) {
        // Note: The focus file will be processed here again but will be skipped
        // over when added to `currentFiles`.
        relatedFiles.push({
          token: generateToken(fileHandle),
          // This will get populated by refreshFile before the file gets opened.
          file: null,
          handle: fileHandle,
          inCurrentDirectory: true,
        });
      }
    }
  } catch (e: unknown) {
    console.warn(e, '(failed to traverse directory)');
    // It's unlikely traversal can "resume", but try to continue with anything
    // obtained so far.
  }

  if (currentFiles.length > 1) {
    // Related files identified as required for the initial load must be removed
    // so they don't appear in the file list twice.
    const atLoadCurrentFiles = currentFiles.slice(1);
    relatedFiles = relatedFiles.filter(
        f => !atLoadCurrentFiles.find(c => c.handle.name === f.handle.name));
  }

  if (localLaunchNumber !== globalLaunchNumber) {
    return ProcessOtherFilesResult.ABORT;
  }

  await sortFiles(relatedFiles);
  const name = focusFile.name;
  const focusIndex = relatedFiles.findIndex(i => i.handle.name === name);
  entryIndex = 0;
  if (focusIndex === -1) {
    // The focus file is no longer there i.e. might have been deleted, should be
    // missing from `currentFiles` as well.
    currentFiles.push(...relatedFiles);
    return ProcessOtherFilesResult.FOCUS_FILE_MISSING;
  } else {
    // Rotate the sorted files so focusIndex becomes index 0 such that we have
    // [focus file, ...files larger, ...files smaller].
    currentFiles.push(...relatedFiles.slice(focusIndex + 1));
    currentFiles.push(...relatedFiles.slice(0, focusIndex));
    return ProcessOtherFilesResult.FOCUS_FILE_RELEVANT;
  }
}

/**
 * Sorts the given `files` by `sortOrder`.
 */
async function sortFiles(files: FileDescriptor[]) {
  if (sortOrder === SortOrder.NEWEST_FIRST) {
    // If we are sorting by modification time we need to have the actual File
    // object available.
    for (const descriptor of files) {
      // TODO(b/166210455): Remove this call to getFile as it may be slow for
      // android files see b/172529567. Leaving it in at the moment since sort
      // order is not set to NEWEST_FIRST in any production release and there is
      // no way to get modified time without calling getFile.
      try {
        descriptor.file = (await getFileFromHandle(descriptor.handle)).file;
      } catch (e: any) {
        warnIfUncommon(e, descriptor.handle.name);
      }
    }
  }

  // Iteration order is not guaranteed using `directory.entries()`, so we
  // sort it afterwards by modification time to ensure a consistent and logical
  // order. More recent (i.e. higher timestamp) files should appear first. In
  // the case where timestamps are equal, the files will be sorted
  // lexicographically according to their names.
  files.sort((a, b) => {
    if (sortOrder === SortOrder.NEWEST_FIRST) {
      // Sort null files last if they racily appear.
      if (!a.file && !b.file) {
        return 0;
      } else if (!b.file) {
        return -1;
      } else if (!a.file) {
        return 1;
      } else if (a.file.lastModified === b.file.lastModified) {
        return a.file.name.localeCompare(b.file.name);
      }
      return b.file.lastModified - a.file.lastModified;
    }
    // Else default to lexicographical sort.
    // Match the Intl.Collator params used for sorting in the files app in
    // file_manager/common/js/util.js.
    const direction = sortOrder === SortOrder.A_FIRST ? 1 : -1;
    return direction *
        a.handle.name.localeCompare(
            b.handle.name, [],
            {usage: 'sort', numeric: true, sensitivity: 'base'});
  });
}

/**
 * Loads related files in the working directory and sends them to the guest. If
 * the focus file (currentFiles[0]) is no longer relevant i.e. is has been
 * deleted, we load files as usual.
 */
async function loadOtherRelatedFiles(
    directory: FileSystemDirectoryHandle, focusFile: File|null,
    _focusHandle: FileSystemFileHandle|null, localLaunchNumber: number) {
  const processResult = await processOtherFilesInDirectory(
      directory, focusFile, localLaunchNumber);
  if (localLaunchNumber !== globalLaunchNumber ||
      processResult === ProcessOtherFilesResult.ABORT) {
    return;
  }

  const shallowCopy = [...currentFiles];
  // If the focus file is no longer relevant, loads files as normal.
  await sendSnapshotToGuest(
      shallowCopy, localLaunchNumber,
      processResult === ProcessOtherFilesResult.FOCUS_FILE_RELEVANT);
}

/**
 * Sets state for the files opened in the current directory.
 */
function setCurrentDirectory(
    directory: FileSystemDirectoryHandle,
    focusFile: {file: File, handle: FileSystemFileHandle}) {
  // Load currentFiles into the guest.
  currentFiles.length = 0;
  currentFiles.push({
    token: generateToken(focusFile.handle),
    file: focusFile.file,
    handle: focusFile.handle,
    inCurrentDirectory: true,
  });
  currentDirectoryHandle = directory;
  entryIndex = 0;
}

/**
 * Returns a filename associated with `focusFileName` that may be required to
 * properly load the file. The file might not exist.
 * TODO(b/175099007): Support multiple associated files.
 */
function requiredAssociatedFileName(focusFileName: string): string {
  // Subtitles must be identified for the initial load to be properly attached.
  if (!isVideoFile(focusFileName)) {
    return '';
  }
  // To match the video player app, just look for `.vtt` until alternative
  // heuristics are added inside the app layer. See b/175099007.
  return focusFileName.replace(/\.[^\.]+$/, '.vtt');
}

/**
 * Adds file handles for associated files to the set of launch files.
 */
async function detectLoadRequiredAssociatedFiles(
    directory: FileSystemDirectoryHandle, focusFileName: string) {
  const vttFileName = requiredAssociatedFileName(focusFileName);
  if (!vttFileName) {
    return;
  }
  try {
    const vttFileHandle = await directory.getFileHandle(vttFileName);
    currentFiles.push({
      token: generateToken(vttFileHandle),
      file: null,  // Will be set by `refreshLoadRequiredAssociatedFiles()`.
      handle: vttFileHandle,
      inCurrentDirectory: true,
    });
  } catch (e: unknown) {
    // Do nothing if not found or not permitted.
  }
}

/**
 * Refreshes the File object for all file handles associated with the focus
 * file.
 */
async function refreshLoadRequiredAssociatedFiles(
    snapshot: FileDescriptor[], focusFileName: string,
    forExtraFilesMessage: boolean) {
  const vttFileName = requiredAssociatedFileName(focusFileName);
  if (!vttFileName) {
    return;
  }
  const index = snapshot.findIndex(d => d.handle.name === vttFileName);
  if (index >= 0) {
    await refreshFile(snapshot[index]!);
    // In the extra files message, it's necessary to remove the vtt file from
    // the snapshot to avoid it being added again in the receiver.
    if (forExtraFilesMessage) {
      snapshot.splice(index, 1);
    }
  }
}

/**
 * Launch the media app with the files in the provided directory, using `handle`
 * as the initial launch entry.
 */
async function launchWithDirectory(
    directory: FileSystemDirectoryHandle, handle: FileSystemHandle) {
  const localLaunchNumber = ++globalLaunchNumber;

  let asFile;
  try {
    asFile = await getFileFromHandle(handle);
  } catch (e: any) {
    console.warn(`${handle.name}: ${e.message}`);
    sendSnapshotToGuest(
        [{
          token: -1,
          file: null,
          handle: handle as FileSystemFileHandle,
          lastError: e.name,
        }],
        localLaunchNumber);
    return;
  }
  // Load currentFiles into the guest.
  setCurrentDirectory(directory, asFile);
  await detectLoadRequiredAssociatedFiles(directory, handle.name);
  await sendSnapshotToGuest([...currentFiles], localLaunchNumber);
  // The app is operable with the first file now.

  // Process other files in directory.
  // TODO(https://github.com/WICG/file-system-access/issues/215): Don't process
  // other files if there is only 1 file which is already loaded by
  // `sendSnapshotToGuest()` above.
  await loadOtherRelatedFiles(
      directory, asFile.file, asFile.handle, localLaunchNumber);
}

/**
 * Launch the media app with the selected files.
 */
async function launchWithMultipleSelection(
    directory: FileSystemDirectoryHandle,
    handles: Array<FileSystemHandle|null|undefined>) {
  currentFiles.length = 0;
  for (const handle of handles) {
    if (handle && handle.kind === 'file') {
      const fileHandle = handle as FileSystemFileHandle;
      currentFiles.push({
        token: generateToken(fileHandle),
        file: null,  // Just let sendSnapshotToGuest() "refresh" it.
        handle: fileHandle,
        // TODO(b/163285659): Enable delete/rename for multi-select files.
      });
    }
  }
  await sortFiles(currentFiles);
  entryIndex = currentFiles.length > 0 ? 0 : -1;
  currentDirectoryHandle = directory;
  await sendFilesToGuest();
}

/**
 * Advance to another file.
 *
 * @param direction How far to advance (e.g. +/-1).
 * @param currentFileToken The token of the file that
 *     direction is in reference to. If unprovided it's assumed that
 *     currentFiles[entryIndex] is the current file.
 */
async function advance(direction: number, currentFileToken?: number) {
  let currIndex = entryIndex;
  if (currentFileToken) {
    const fileIndex =
        currentFiles.findIndex(fd => fd.token === currentFileToken);
    currIndex = fileIndex === -1 ? currIndex : fileIndex;
  }

  if (currentFiles.length) {
    entryIndex = (currIndex + direction) % currentFiles.length;
    if (entryIndex < 0) {
      entryIndex += currentFiles.length;
    }
  } else {
    entryIndex = -1;
  }
  await sendFilesToGuest();
}

/**
 * The launchQueue consumer. This returns a promise to help tests, but the file
 * handling API will ignore it.
 */
async function launchConsumer(params?: LaunchParams): Promise<void> {
  // The MediaApp sets `include_launch_directory = true` in its SystemAppInfo
  // struct compiled into Chrome. That means files[0] is guaranteed to be a
  // directory, with remaining launch files following it. Validate that this is
  // true and abort the launch if is is not.
  if (!params || !params.files || params.files.length < 2) {
    console.error('Invalid launch (missing files): ', params);
    return;
  }

  if (assertCast(params.files[0]).kind !== 'directory') {
    console.error('Invalid launch: files[0] is not a directory: ', params);
    return;
  }
  const directory = params.files[0] as FileSystemDirectoryHandle;
  // With a single file selected, that file is the focus file. Otherwise, there
  // is no inherent focus file.
  const maybeFocusEntry = assertCast(params.files[1]);

  // With a single file selected, launch with all files in the directory as
  // navigation candidates. Otherwise, launch with all selected files (except
  // the launch directory itself) as navigation candidates. The only exception
  // to this is audio files, which we explicitly don't load the directory for.
  if (params.files.length === 2 && !isAudioFile(maybeFocusEntry.name)) {
    try {
      await launchWithDirectory(directory, maybeFocusEntry);
    } catch (e: unknown) {
      console.error(e, '(launchWithDirectory aborted)');
    }
  } else {
    try {
      await launchWithMultipleSelection(directory, params.files.slice(1));
    } catch (e: unknown) {
      console.error(e, '(launchWithMultipleSelection aborted)');
    }
  }
}

/**
 * Wrapper for the launch consumer to ensure it doesn't return a Promise, nor
 * propagate exceptions. Tests will want to target `launchConsumer` directly so
 * that they can properly await launch results.
 */
function wrappedLaunchConsumer(params?: LaunchParams) {
  launchConsumer(params).catch(e => {
    console.error(e, '(launch aborted)');
  });
}

/**
 * Installs the handler for launch files, if window.launchQueue is available.
 */
function installLaunchHandler() {
  if (!window.launchQueue) {
    console.error('FileHandling API missing.');
    return;
  }
  window.launchQueue.setConsumer(wrappedLaunchConsumer);
}

installLaunchHandler();

// Make sure the guest frame has focus.
const guest = assertCast(document.querySelector<HTMLIFrameElement>(
    'iframe[src^="chrome-untrusted://media-app"]'));
guest.addEventListener('load', () => {
  guest.focus();
});

export const TEST_ONLY = {
  Message,
  SortOrder,
  advance,
  currentDirectoryHandle,
  currentFiles,
  fileHandleForToken,
  globalLaunchNumber,
  guestMessagePipe,
  launchConsumer,
  launchWithDirectory,
  loadOtherRelatedFiles,
  pickWritableFile,
  processOtherFilesInDirectory,
  sendFilesToGuest,
  setCurrentDirectory,
  sortOrder,
  tokenGenerator,
  tokenMap,
  mediaAppPageHandler,
  error_reporter,
  getGlobalLaunchNumber: () => globalLaunchNumber,
  incrementLaunchNumber: () => ++globalLaunchNumber,
  setCurrentDirectoryHandle: (d: FileSystemDirectoryHandle|null) => {
    currentDirectoryHandle = d;
  },
  setSortOrder: (s: SortOrder) => {
    sortOrder = s;
  },
  getEntryIndex: () => entryIndex,
  setEntryIndex: (i: number) => {
    entryIndex = i;
  },
};

// Small, auxiliary file that adds hooks to support test cases relying on the
// "real" app context (e.g. for stack traces).
import './app_context_test_support.js';

// Expose `advance()` for MediaAppIntegrationTest.FileOpenCanTraverseDirectory.
window['advance'] = advance;
