// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './sandboxed_load_time_data.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';

import {assertCast, MessagePipe} from './message_pipe.js';
import {EditInPhotosMessage, FileContext, IsFileArcWritableMessage, IsFileArcWritableResponse, IsFileBrowserWritableMessage, IsFileBrowserWritableResponse, LoadFilesMessage, Message, OpenAllowedFileMessage, OpenAllowedFileResponse, OpenFilesWithPickerMessage, OverwriteFileMessage, OverwriteViaFilePickerResponse, RenameFileResponse, RenameResult, RequestSaveFileMessage, RequestSaveFileResponse, SaveAsMessage, SaveAsResponse} from './message_types.js';
import {loadPiex} from './piex_module_loader.js';

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

/**
 * Placeholder Blob used when a null file is received. For null files we only
 * know the name until the file is navigated to.
 */
const PLACEHOLDER_BLOB = new Blob([]);

/**
 * A file received from the privileged context, and decorated with IPC methods
 * added in the untrusted (this) context to communicate back.
 * @implements {mediaApp.AbstractFile}
 */
class ReceivedFile {
  /** @param {!FileContext} file */
  constructor(file) {
    this.blob = file.file || PLACEHOLDER_BLOB;
    this.name = file.name;
    this.size = this.blob.size;
    this.mimeType = this.blob.type;
    this.token = file.token;
    this.error = file.error;
    this.fromClipboard = false;
    if (file.canDelete) {
      this.deleteOriginalFile = () => this.deleteOriginalFileImpl();
    }
    if (file.canRename) {
      this.renameOriginalFile = (/** string */ newName) =>
          this.renameOriginalFileImpl(newName);
    }
  }

  /**
   * @override
   * @return {!Promise<boolean>}
   */
  async isArcWritable() {
    /** @type {!IsFileArcWritableMessage} */
    const message = {token: this.token};

    const {writable} = /** @type {!IsFileArcWritableResponse} */ (
        await parentMessagePipe.sendMessage(
            Message.IS_FILE_ARC_WRITABLE, message));
    return writable;
  }

  /**
   * @override
   * @return {!Promise<boolean>}
   */
  async isBrowserWritable() {
    /** @type {!IsFileBrowserWritableMessage} */
    const message = {token: this.token};

    const {writable} = /** @type {!IsFileBrowserWritableResponse} */ (
        await parentMessagePipe.sendMessage(
            Message.IS_FILE_BROWSER_WRITABLE, message));
    return writable;
  }

  /**
   * @override
   */
  async editInPhotos() {
    /** @type {!EditInPhotosMessage} */
    const message = {token: this.token, mimeType: this.mimeType};

    await parentMessagePipe.sendMessage(Message.EDIT_IN_PHOTOS, message);
  }

  /**
   * @override
   * @param{!Blob} blob
   */
  async overwriteOriginal(blob) {
    /** @type {!OverwriteFileMessage} */
    const message = {token: this.token, blob: blob};

    const result = /** @type {!OverwriteViaFilePickerResponse} */ (
        await parentMessagePipe.sendMessage(Message.OVERWRITE_FILE, message));
    // Note the following are skipped if an exception is thrown above.
    if (result.renamedTo) {
      this.name = result.renamedTo;
      // Assume a rename could have moved the file to a new folder via a file
      // picker, which will break rename/delete functionality.
      delete this.deleteOriginalFile;
      delete this.renameOriginalFile;
    }
    this.error = result.errorName || '';
    this.updateFile(blob, this.name);
  }

  /**
   * @return {!Promise<number>}
   */
  async deleteOriginalFileImpl() {
    await parentMessagePipe.sendMessage(
        Message.DELETE_FILE, {token: this.token});
    // TODO(b/156571159): Remove when app_main.js no longer needs this.
    return 0; /* "SUCCESS" */
  }

  /**
   * @param {string} newName
   * @return {!Promise<number>}
   */
  async renameOriginalFileImpl(newName) {
    const renameResponse =
        /** @type {!RenameFileResponse} */ (await parentMessagePipe.sendMessage(
            Message.RENAME_FILE, {token: this.token, newFilename: newName}));
    if (renameResponse.renameResult === RenameResult.SUCCESS) {
      this.name = newName;
    }
    return renameResponse.renameResult;
  }

  /**
   * @override
   * @param {!Blob} blob
   * @param {number} pickedFileToken
   * @return {!Promise<undefined>}
   */
  async saveAs(blob, pickedFileToken) {
    /** @type {!SaveAsMessage} */
    const message = {blob, oldFileToken: this.token, pickedFileToken};
    const result = /** @type {!SaveAsResponse} */ (
        await parentMessagePipe.sendMessage(Message.SAVE_AS, message));
    this.updateFile(blob, result.newFilename);
    // Files obtained by a file picker currently can not be renamed/deleted.
    // TODO(b/163285659): Detect when the new file is in the same folder as an
    // on-launch file. Those should still be able to be renamed/deleted.
    delete this.deleteOriginalFile;
    delete this.renameOriginalFile;
  }

  /**
   * @override
   * @param {!Array<string>} accept
   * @return {!Promise<!mediaApp.AbstractFile>}
   */
  async getExportFile(accept) {
    /** @type {!RequestSaveFileMessage} */
    const msg = {
      suggestedName: this.name,
      mimeType: this.mimeType,
      startInToken: this.token,
      accept,
    };
    const response =
        /** @type {!RequestSaveFileResponse} */ (
            await parentMessagePipe.sendMessage(
                Message.REQUEST_SAVE_FILE, msg));
    return new ReceivedFile(response.pickedFileContext);
  }

  /**
   * @override
   * @return {!Promise<!File>}
   */
  async openFile() {
    /** @type {!OpenAllowedFileMessage} */
    const msg = {
      fileToken: this.token,
    };
    const response =
        /** @type {!OpenAllowedFileResponse} */ (
            await parentMessagePipe.sendMessage(
                Message.OPEN_ALLOWED_FILE, msg));
    return response.file;
  }

  /**
   * Updates the wrapped file to reflect a change written to disk.
   * @private
   * @param {!Blob} blob
   * @param {string} name
   */
  updateFile(blob, name) {
    // Wrap the blob to acquire "now()" as the lastModified time. Note this may
    // differ from the actual mtime recorded on the inode.
    this.blob = new File([blob], name, {type: blob.type});
    this.size = blob.size;
    this.mimeType = blob.type;
    this.name = name;
  }
}

/**
 * Source of truth for what files are loaded in the app. This can be appended to
 * via `ReceivedFileList.addFiles()`.
 * @type {?ReceivedFileList}
 */
let lastLoadedReceivedFileList = null;

/**
 * A file list consisting of all files received from the parent. Exposes all
 * readable files in the directory, some of which may be writable.
 * @implements mediaApp.AbstractFileList
 */
export class ReceivedFileList {
  /** @param {!LoadFilesMessage} filesMessage */
  constructor(filesMessage) {
    const {files, currentFileIndex} = filesMessage;
    if (files.length) {
      // If we were not provided with a currentFileIndex, default to making the
      // first file the current file.
      this.currentFileIndex = currentFileIndex >= 0 ? currentFileIndex : 0;
    } else {
      // If we are empty we have no current file.
      this.currentFileIndex = -1;
    }

    this.length = files.length;
    /** @type {!Array<!ReceivedFile>} */
    this.files = files.map(f => new ReceivedFile(f));
    /** @type {!Array<function(!mediaApp.AbstractFileList): void>} */
    this.observers = [];
  }

  /** @override */
  item(index) {
    return this.files[index] || null;
  }

  /** @override */
  async loadNext(currentFileToken) {
    // Awaiting this message send allows callers to wait for the full effects of
    // the navigation to complete. This may include a call to load a new set of
    // files, and the initial decode, which replaces this AbstractFileList and
    // alters other app state.
    await parentMessagePipe.sendMessage(
        Message.NAVIGATE, {currentFileToken, direction: 1});
  }

  /** @override */
  async loadPrev(currentFileToken) {
    await parentMessagePipe.sendMessage(
        Message.NAVIGATE, {currentFileToken, direction: -1});
  }

  /** @override */
  addObserver(observer) {
    this.observers.push(observer);
  }

  /**
   * @override
   * @param {!Array<string>} acceptTypeKeys
   * @param {?mediaApp.AbstractFile} startInFolder
   * @param {?boolean} isSingleFile
   * @return {!Promise<undefined>}
   */
  async openFilesWithFilePicker(acceptTypeKeys, startInFolder, isSingleFile) {
    // AbstractFile doesn't guarantee tokens. Use one from a ReceivedFile if
    // there is one, after ensuring it is valid.
    const fileRep = /** @type {{token: (number|undefined)}} */ (startInFolder);
    const startInToken = startInFolder ? (fileRep.token || 0) : 0;
    /** @type {!OpenFilesWithPickerMessage} */
    const msg = {
      startInToken: startInToken > 0 ? startInToken : 0,
      accept: acceptTypeKeys,
      isSingleFile,
    };
    await parentMessagePipe.sendMessage(Message.OPEN_FILES_WITH_PICKER, msg);
  }

  /**
   * @override
   * @param {!function(!mediaApp.AbstractFile): boolean} filter
   */
  filterInPlace(filter) {
    this.files = this.files.filter(filter);
    this.length = this.files.length;
    this.currentFileIndex = this.length > 0 ? 0 : -1;
  }

  /** @param {!Array<!ReceivedFile>} files */
  addFiles(files) {
    if (files.length === 0) {
      return;
    }
    this.files = [...this.files, ...files];
    this.length = this.files.length;
    // Call observers with the new underlying files.
    this.observers.map(o => o(this));
  }
}

parentMessagePipe.registerHandler(Message.LOAD_FILES, async (message) => {
  const filesMessage = /** @type {!LoadFilesMessage} */ (message);
  lastLoadedReceivedFileList = new ReceivedFileList(filesMessage);
  await loadFiles(lastLoadedReceivedFileList);
});

// Load extra files by appending to the current `ReceivedFileList`.
parentMessagePipe.registerHandler(Message.LOAD_EXTRA_FILES, async (message) => {
  if (!lastLoadedReceivedFileList) {
    return;
  }
  const extraFilesMessage = /** @type {!LoadFilesMessage} */ (message);
  const newFiles = extraFilesMessage.files.map(f => new ReceivedFile(f));
  lastLoadedReceivedFileList.addFiles(newFiles);
});

// As soon as the LOAD_FILES handler is installed, signal readiness to the
// parent frame (privileged context).
parentMessagePipe.sendMessage(Message.IFRAME_READY);

/**
 * A delegate which exposes privileged WebUI functionality to the media
 * app.
 * @type {!mediaApp.ClientApiDelegate}
 */
const DELEGATE = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  },
  async toggleBrowserFullscreenMode() {
    await parentMessagePipe.sendMessage(Message.TOGGLE_BROWSER_FULLSCREEN_MODE);
  },
  /**
   * @param {string} suggestedName
   * @param {string} mimeType
   * @param {!Array<string>} accept
   * @return {!Promise<!mediaApp.AbstractFile>}
   */
  async requestSaveFile(suggestedName, mimeType, accept) {
    /** @type {!RequestSaveFileMessage} */
    const msg = {suggestedName, mimeType, startInToken: 0, accept};
    const response =
        /** @type {!RequestSaveFileResponse} */ (
            await parentMessagePipe.sendMessage(
                Message.REQUEST_SAVE_FILE, msg));
    return new ReceivedFile(response.pickedFileContext);
  },
  /**
   * @param {string|undefined} name
   * @param {string|undefined} type
   */
  notifyCurrentFile(name, type) {
    parentMessagePipe.sendMessage(Message.NOTIFY_CURRENT_FILE, {name, type});
  },
  /**
   * @param {!Blob} file
   * @return {!Promise<!File>}
   */
  async extractPreview(file) {
    try {
      const bufferPromise = file.arrayBuffer();
      const extractFromRawImageBuffer = await loadPiex();
      return await extractFromRawImageBuffer(await bufferPromise);
    } catch (/** @type {!Error} */ e) {
      console.warn(e);
      if (e.name === 'Error') {
        e.name = 'JpegNotFound';
      }
      throw e;
    }
  },
  /**
   * @param {string} title
   * @param {string} blobUuid
   */
  openInSandboxedViewer(title, blobUuid) {
    parentMessagePipe.sendMessage(
        Message.OPEN_IN_SANDBOXED_VIEWER, {title, blobUuid});
  },
  reloadMainFrame() {
    parentMessagePipe.sendMessage(Message.RELOAD_MAIN_FRAME);
  },
  maybeTriggerPdfHats() {
    parentMessagePipe.sendMessage(Message.MAYBE_TRIGGER_PDF_HATS);
  },
  // TODO(b/219631600): Implement openUrlInBrowserTab() for LacrOS if needed.
};

/**
 * Returns the media app if it can find it in the DOM.
 * @return {?mediaApp.ClientApi}
 */
function getApp() {
  return /** @type {?mediaApp.ClientApi} */ (
      document.querySelector('backlight-app'));
}

/**
 * Loads a file list into the media app.
 * @param {!ReceivedFileList} fileList
 * @return {!Promise<undefined>}
 */
async function loadFiles(fileList) {
  const app = getApp();
  if (app) {
    await app.loadFiles(fileList);
  } else {
    // Note we don't await in this case, which may affect b/152729704.
    window.customLaunchData.files = fileList;
  }
}

/**
 * Runs any initialization code on the media app once it is in the dom.
 * @param {!mediaApp.ClientApi} app
 */
function initializeApp(app) {
  app.setDelegate(DELEGATE);
}

/**
 * Called when a mutation occurs on document.body to check if the media app is
 * available.
 * @param {!Array<!MutationRecord>} mutationsList
 * @param {!MutationObserver} observer
 */
function mutationCallback(mutationsList, observer) {
  const app = getApp();
  if (!app) {
    return;
  }
  // The media app now exists so we can initialize it.
  initializeApp(app);
  observer.disconnect();
}

window.addEventListener('DOMContentLoaded', () => {
  // Start listening to color change events. These events get picked up by logic
  // in ts_helpers.ts on the google3 side.
  /** @suppress {checkTypes} */
  (function() {
    ColorChangeUpdater.forDocument().start();
  })();

  const app = getApp();
  if (app) {
    initializeApp(app);
    return;
  }
  // If translations need to be fetched, the app element may not be added yet.
  // In that case, observe <body> until it is.
  const observer = new MutationObserver(mutationCallback);
  observer.observe(document.body, {childList: true});
});

// Ensure that if no files are loaded into the media app there is a default
// empty file list available.
window.customLaunchData = {
  delegate: DELEGATE,
  files: new ReceivedFileList({files: [], currentFileIndex: -1}),
};

// Attempting to show file pickers in the sandboxed <iframe> is guaranteed to
// result in a SecurityError: hide them.
// TODO(crbug/1040328): Remove this when we have a polyfill that allows us to
// talk to the privileged frame.
window['chooseFileSystemEntries'] = null;
window['showOpenFilePicker'] = null;
window['showSaveFilePicker'] = null;
window['showDirectoryPicker'] = null;

// Expose functions to bind to color change events to window so they can be
// automatically picked up by installColors(). See ts_helpers.ts in google3.
window['addColorChangeListener'] =
    /** @suppress {checkTypes} */ function(listener) {
      ColorChangeUpdater.forDocument().eventTarget.addEventListener(
          COLOR_PROVIDER_CHANGED, listener);
    };
window['removeColorChangeListener'] =
    /** @suppress {checkTypes} */ function(listener) {
      ColorChangeUpdater.forDocument().eventTarget.removeEventListener(
          COLOR_PROVIDER_CHANGED, listener);
    };

export const TEST_ONLY = {
  RenameResult,
  DELEGATE,
  assertCast,
  parentMessagePipe,
  loadFiles,
  setLoadFiles: spy => {
    loadFiles = spy;
  },
};

// Small, auxiliary file that adds hooks to support test cases relying on the
// "real" app context (e.g. for stack traces).
import './app_context_test_support.js';

// Temporarily expose lastLoadedReceivedFileList on `window` for
// MediaAppIntegrationWithFilesAppAllProfilesTest.RenameFile.
// TODO(b/185957537): Convert the test case to a JS module.
window['lastLoadedReceivedFileList'] = () => lastLoadedReceivedFileList;
