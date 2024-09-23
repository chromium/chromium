// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/// <reference path="media_app.d.ts" />

import './sandboxed_load_time_data.js';

import {COLOR_PROVIDER_CHANGED, ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {RectF} from '//resources/mojo/ui/gfx/geometry/mojom/geometry.mojom-webui.js';
import type {Url as MojoUrl} from '//resources/mojo/url/mojom/url.mojom-webui.js';
import {assertCast, MessagePipe} from '//system_apps/message_pipe.js';

import type {MahiUntrustedPageHandlerRemote, OcrUntrustedPageHandlerRemote, PageMetadata} from './media_app_ui_untrusted.mojom-webui.js';
import {EditInPhotosMessage, FileContext, IsFileArcWritableMessage, IsFileArcWritableResponse, IsFileBrowserWritableMessage, IsFileBrowserWritableResponse, LoadFilesMessage, Message, OpenAllowedFileMessage, OpenAllowedFileResponse, OpenFilesWithPickerMessage, OverwriteFileMessage, OverwriteViaFilePickerResponse, RenameFileResponse, RenameResult, RequestSaveFileMessage, RequestSaveFileResponse, SaveAsMessage, SaveAsResponse} from './message_types.js';
import {connectToMahiHandler, connectToOcrHandler, mahiCallbackRouter, ocrCallbackRouter} from './mojo_api_bootstrap_untrusted.js';
import {loadPiex} from './piex_module_loader.js';

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://media-app', window.parent);

/**
 * Placeholder Blob used when a null file is received. For null files we only
 * know the name until the file is navigated to.
 */
const PLACEHOLDER_BLOB = new Blob([]);

/**
 * On PDF loaded, try to get this byte size of text content to check whether
 * this file contains text.
 */
const PDF_TEXT_CONTENT_PEEK_BYTE_SIZE = 100;

/**
 * A file received from the privileged context, and decorated with IPC methods
 * added in the untrusted (this) context to communicate back.
 */
export class ReceivedFile implements AbstractFile {
  blob: Blob;
  name: string;
  token: number;
  size: number;
  mimeType: string;
  fromClipboard: boolean;
  error: string;

  deleteOriginalFile?: () => Promise<void>;
  renameOriginalFile?: (newName: string) => Promise<number>;

  constructor(file: FileContext) {
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
      this.renameOriginalFile = (newName) =>
          this.renameOriginalFileImpl(newName);
    }
  }

  async isArcWritable() {
    const message: IsFileArcWritableMessage = {token: this.token};

    const {writable} = (await parentMessagePipe.sendMessage(
                           Message.IS_FILE_ARC_WRITABLE, message)) as
        IsFileArcWritableResponse;
    return writable;
  }

  async isBrowserWritable() {
    const message: IsFileBrowserWritableMessage = {token: this.token};

    const {writable} = (await parentMessagePipe.sendMessage(
                           Message.IS_FILE_BROWSER_WRITABLE, message)) as
        IsFileBrowserWritableResponse;
    return writable;
  }

  async editInPhotos() {
    const message: EditInPhotosMessage = {
      token: this.token,
      mimeType: this.mimeType,
    };

    await parentMessagePipe.sendMessage(Message.EDIT_IN_PHOTOS, message);
  }

  async overwriteOriginal(blob: Blob) {
    const message: OverwriteFileMessage = {token: this.token, blob: blob};

    const result: OverwriteViaFilePickerResponse =
        await parentMessagePipe.sendMessage(Message.OVERWRITE_FILE, message);
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

  async deleteOriginalFileImpl() {
    await parentMessagePipe.sendMessage(
        Message.DELETE_FILE, {token: this.token});
  }

  async renameOriginalFileImpl(newName: string) {
    const renameResponse: RenameFileResponse =
        await parentMessagePipe.sendMessage(Message.RENAME_FILE, {
          token: this.token,
          newFilename: newName,
        });
    if (renameResponse.renameResult === RenameResult.SUCCESS) {
      this.name = newName;
    }
    return renameResponse.renameResult;
  }

  async saveAs(blob: Blob, pickedFileToken: number) {
    const message: SaveAsMessage = {
      blob,
      oldFileToken: this.token,
      pickedFileToken,
    };
    const result: SaveAsResponse = await parentMessagePipe.sendMessage(
        Message.SAVE_AS,
        message,
    );
    this.updateFile(blob, result.newFilename);
    // Files obtained by a file picker currently can not be renamed/deleted.
    // TODO(b/163285659): Detect when the new file is in the same folder as an
    // on-launch file. Those should still be able to be renamed/deleted.
    delete this.deleteOriginalFile;
    delete this.renameOriginalFile;
  }

  async getExportFile(accept: string[]) {
    const msg: RequestSaveFileMessage = {
      suggestedName: this.name,
      mimeType: this.mimeType,
      startInToken: this.token,
      accept,
    };
    const response: RequestSaveFileResponse =
        await parentMessagePipe.sendMessage(Message.REQUEST_SAVE_FILE, msg);
    return new ReceivedFile(response.pickedFileContext);
  }

  async openFile() {
    const msg: OpenAllowedFileMessage = {
      fileToken: this.token,
    };
    const response: OpenAllowedFileResponse =
        await parentMessagePipe.sendMessage(Message.OPEN_ALLOWED_FILE, msg);
    return response.file;
  }

  /**
   * Updates the wrapped file to reflect a change written to disk.
   */
  private updateFile(blob: Blob, name: string) {
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
 */
let lastLoadedReceivedFileList: ReceivedFileList|null = null;

/**
 * A file list consisting of all files received from the parent. Exposes all
 * readable files in the directory, some of which may be writable.
 */
export class ReceivedFileList implements AbstractFileList {
  length: number;
  currentFileIndex: number;

  files: ReceivedFile[];  // Public for tests.
  private observers: Array<(files: AbstractFileList) => unknown> = [];

  constructor(filesMessage: LoadFilesMessage) {
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
    this.files = files.map((f) => new ReceivedFile(f));
  }

  item(index: number) {
    return this.files[index] || null;
  }

  async loadNext(currentFileToken: number) {
    // Awaiting this message send allows callers to wait for the full effects of
    // the navigation to complete. This may include a call to load a new set of
    // files, and the initial decode, which replaces this AbstractFileList and
    // alters other app state.
    await parentMessagePipe.sendMessage(
        Message.NAVIGATE, {currentFileToken, direction: 1});
  }

  async loadPrev(currentFileToken: number) {
    await parentMessagePipe.sendMessage(
        Message.NAVIGATE, {currentFileToken, direction: -1});
  }

  addObserver(observer: (files: AbstractFileList) => unknown) {
    this.observers.push(observer);
  }

  async openFilesWithFilePicker(
      acceptTypeKeys: string[],
      startInFolder?: AbstractFile,
      isSingleFile?: boolean,
  ) {
    // AbstractFile doesn't guarantee tokens. Use one from a ReceivedFile if
    // there is one, after ensuring it is valid.
    const startInToken = startInFolder?.token || 0;
    const msg: OpenFilesWithPickerMessage = {
      startInToken: startInToken > 0 ? startInToken : 0,
      accept: acceptTypeKeys,
      isSingleFile: !!isSingleFile,
    };
    await parentMessagePipe.sendMessage(Message.OPEN_FILES_WITH_PICKER, msg);
  }

  filterInPlace(filter: (file: AbstractFile) => boolean) {
    this.files = this.files.filter(filter);
    this.length = this.files.length;
    this.currentFileIndex = this.length > 0 ? 0 : -1;
  }

  addFiles(files: ReceivedFile[]) {
    if (files.length === 0) {
      return;
    }
    this.files = [...this.files, ...files];
    this.length = this.files.length;
    // Call observers with the new underlying files.
    this.observers.map((o) => o(this));
  }
}

parentMessagePipe.registerHandler(
    Message.LOAD_FILES, async (filesMessage: LoadFilesMessage) => {
      lastLoadedReceivedFileList = new ReceivedFileList(filesMessage);
      await loadFiles(lastLoadedReceivedFileList);
    });

// Load extra files by appending to the current `ReceivedFileList`.
parentMessagePipe.registerHandler(
    Message.LOAD_EXTRA_FILES, async (extraFilesMessage: LoadFilesMessage) => {
      if (!lastLoadedReceivedFileList) {
        return;
      }
      const newFiles = extraFilesMessage.files.map((f) => new ReceivedFile(f));
      lastLoadedReceivedFileList.addFiles(newFiles);
    });

// As soon as the LOAD_FILES handler is installed, signal readiness to the
// parent frame (privileged context).
parentMessagePipe.sendMessage(Message.IFRAME_READY);

let ocrUntrustedPageHandler: OcrUntrustedPageHandlerRemote;
let mahiUntrustedPageHandler: MahiUntrustedPageHandlerRemote;

ocrCallbackRouter.requestBitmap.addListener(async (requestedPageId: string) => {
  const app = getApp();
  if (app) {
    const result = await app.requestBitmap(requestedPageId);
    return {page: result};
  }
  return null;
});
ocrCallbackRouter.setViewport.addListener(
    (viewportBox: RectF) => void getApp()?.setViewport(viewportBox));
ocrCallbackRouter.setPdfOcrEnabled.addListener(
    (enabled: boolean) => void getApp()?.setPdfOcrEnabled(enabled));
ocrCallbackRouter.onConnectionError.addListener(() => {
  console.warn('Calling MediaApp RequestBitmap() failed to return bitmap.');
});

mahiCallbackRouter.getPdfContent.addListener(async (limit: number) => {
  const app = getApp();
  if (app) {
    const content = await app.getPdfContent(limit);
    return {content};
  }
  return null;
});
mahiCallbackRouter.hidePdfContextMenu.addListener(
    () => void getApp()?.hidePdfContextMenu());
mahiCallbackRouter.onConnectionError.addListener(() => {
  console.warn('Calling MediaApp GetPdfContent() failed to return content.');
});

/**
 * A delegate which exposes privileged WebUI functionality to the media
 * app.
 */
const DELEGATE: ClientApiDelegate = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return response['errorMessage'] as string;
  },
  async submitForm(
      url: MojoUrl,
      payload: number[],
      header: string,
  ) {
    const msg = {
      url,
      payload,
      header,
    };
    await parentMessagePipe.sendMessage(Message.SUBMIT_FORM, msg);
  },
  async toggleBrowserFullscreenMode() {
    await parentMessagePipe.sendMessage(Message.TOGGLE_BROWSER_FULLSCREEN_MODE);
  },
  async requestSaveFile(
      suggestedName: string,
      mimeType: string,
      accept: string[],
  ) {
    const msg: RequestSaveFileMessage = {
      suggestedName,
      mimeType,
      startInToken: 0,
      accept,
    };
    const response: RequestSaveFileResponse =
        await parentMessagePipe.sendMessage(Message.REQUEST_SAVE_FILE, msg);
    return new ReceivedFile(response.pickedFileContext);
  },
  notifyCurrentFile(name?: string, type?: string) {
    parentMessagePipe.sendMessage(Message.NOTIFY_CURRENT_FILE, {name, type});
  },
  notifyFileOpened(name?: string, type?: string) {
    // Close any existing pipes when opening a new file.
    ocrUntrustedPageHandler?.$.close();
    mahiUntrustedPageHandler?.$.close();

    if (type === 'application/pdf') {
      ocrUntrustedPageHandler = connectToOcrHandler();
      mahiUntrustedPageHandler = connectToMahiHandler(name);
    }
  },
  notifyFilenameChanged(name: string) {
    mahiUntrustedPageHandler?.onPdfFileNameUpdated(name);
  },
  async extractPreview(file: Blob) {
    try {
      const bufferPromise = file.arrayBuffer();
      const extractFromRawImageBuffer = await loadPiex();
      return await extractFromRawImageBuffer(await bufferPromise);
    } catch (e: unknown) {
      console.warn(e);
      if ((e as Error).name === 'Error') {
        (e as Error).name = 'JpegNotFound';
      }
      throw e;
    }
  },
  openInSandboxedViewer(title: string, blobUuid: string) {
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

  // All methods below are on the guest / untrusted frame.

  async pageMetadataUpdated(pageMetadata: PageMetadata[]) {
    await ocrUntrustedPageHandler?.pageMetadataUpdated(pageMetadata);
  },
  async pageContentsUpdated(dirtyPageId: string) {
    await ocrUntrustedPageHandler?.pageContentsUpdated(dirtyPageId);
  },
  async viewportUpdated(viewportBox: RectF, scaleFactor: number) {
    await ocrUntrustedPageHandler?.viewportUpdated(viewportBox, scaleFactor);
  },
  async onPdfLoaded() {
    let hasText = false;
    const app = getApp();
    if (app) {
      const peekContent =
          (await app.getPdfContent(PDF_TEXT_CONTENT_PEEK_BYTE_SIZE))
              ?.toString() ??
          '';
      hasText = peekContent.trim() !== '';
    }

    if (!hasText) {
      mahiUntrustedPageHandler?.$.close();
    } else {
      await mahiUntrustedPageHandler?.onPdfLoaded();
    }
  },
  async onPdfContextMenuShow(anchor: RectF) {
    await mahiUntrustedPageHandler?.onPdfContextMenuShow(anchor);
  },
  async onPdfContextMenuHide() {
    await mahiUntrustedPageHandler?.onPdfContextMenuHide();
  },
};

/**
 * Returns the media app if it can find it in the DOM.
 */
function getApp(): ClientApi {
  const app = document.querySelector('backlight-app')!;
  return app as unknown as ClientApi;
}

/**
 * Loads a file list into the media app.
 */
async function loadFilesImpl(fileList: ReceivedFileList) {
  const app = getApp();
  if (app) {
    await app.loadFiles(fileList);
  } else {
    // Note we don't await in this case, which may affect b/152729704.
    window.customLaunchData.files = fileList;
  }
}

/** Store `loadFilesImpl` into a variable so that tests may spy on it. */
let loadFiles = loadFilesImpl;

/**
 * Runs any initialization code on the media app once it is in the dom.
 */
function initializeApp(app: ClientApi) {
  app.setDelegate(DELEGATE);
}

/**
 * Called when a mutation occurs on document.body to check if the media app is
 * available.
 */
function mutationCallback(
    _mutationsList: MutationRecord[], observer: MutationObserver) {
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

declare global {
  interface Window {
    chooseFileSystemEntries: null;
    addColorChangeListener: (listener: EventListenerOrEventListenerObject|
                             null) => unknown;
    removeColorChangeListener: (listener: EventListenerOrEventListenerObject|
                                null) => unknown;
    lastLoadedReceivedFileList: () => ReceivedFileList | null;
  }
}

// Ensure that if no files are loaded into the media app there is a default
// empty file list available.
window.customLaunchData = {
  delegate: DELEGATE,
  files: new ReceivedFileList({files: [], currentFileIndex: -1}),
};

// Attempting to show file pickers in the sandboxed <iframe> is guaranteed to
// result in a SecurityError: hide them.
window.chooseFileSystemEntries = null;
window.showOpenFilePicker = null;
window.showSaveFilePicker = null;
window.showDirectoryPicker = null;

// Expose functions to bind to color change events to window so they can be
// automatically picked up by installColors(). See ts_helpers.ts in google3.
window.addColorChangeListener = function(listener) {
  ColorChangeUpdater.forDocument().eventTarget.addEventListener(
      COLOR_PROVIDER_CHANGED, listener);
};
window.removeColorChangeListener = function(listener) {
  ColorChangeUpdater.forDocument().eventTarget.removeEventListener(
      COLOR_PROVIDER_CHANGED, listener);
};

export const TEST_ONLY = {
  RenameResult,
  DELEGATE,
  assertCast,
  parentMessagePipe,
  loadFiles,
  setLoadFiles: (spy: any) => {
    loadFiles = spy;
  },
};

// Small, auxiliary file that adds hooks to support test cases relying on the
// "real" app context (e.g. for stack traces).
import './app_context_test_support.js';

// Temporarily expose lastLoadedReceivedFileList on `window` for
// MediaAppIntegrationWithFilesAppAllProfilesTest.RenameFile.
// TODO(b/185957537): Convert the test case to a JS module.
window.lastLoadedReceivedFileList = () => lastLoadedReceivedFileList;
