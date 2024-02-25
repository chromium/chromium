// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Types for go/media-app-externs. Currently this only exists in the chromium
 * repository and is handcrafted. When no media_app JS exists, this file will
 * replace go/media-app-externs, and it can be a regular `.ts` file that both
 * toolchains consume directly. Until then, the internal toolchain builds only
 * off the JS externs and has no knowledge of this file.
 */

/**
 * Wraps an HTML File object (or a mock, or media loaded through another means).
 */
declare interface AbstractFile {
  /**
   * The native Blob representation.
   */
  blob: Blob;
  /**
   * A name to represent this file in the UI. Usually the filename.
   */
  name: string;
  /**
   * A unique number that represents this file, used to communicate the file in
   * IPC with a parent frame.
   */
  token?: number;
  /**
   * Size of the file, e.g., from the HTML5 File API.
   */
  size: number;
  /**
   * Mime Type of the file, e.g., from the HTML5 File API. Note that the name
   * intentionally does not match the File API version because 'type' is a
   * reserved word in TypeScript.
   */
  mimeType: string;
  /**
   * Whether the file came from the clipboard or a similar in-memory source not
   * backed by a file on disk.
   */
  fromClipboard?: boolean;
  /**
   * An error associated with this file.
   * @type {string|undefined}
   */
  error?: string;
  /**
   * A function that queries the original file's path to see if it is in a
   * filesystem that ARC is able to write to. Returns a promise that resolves
   * once this has been determined.
   */
  isArcWritable?: () => Promise<boolean>;
  /**
   * A function that queries the original file's path to see if it is writable
   * according to Ash. Returns a promise that resolves once this has been
   * determined.
   */
  isBrowserWritable?: () => Promise<boolean>;
  /**
   * A function that attempts to launch the file in Photos in editing mode.
   * Returns a promise that resolves when the launch has initiated.
   */
  editInPhotos?: () => Promise<void>;
  /**
   * A function that will overwrite the original file with the provided Blob.
   * Returns a promise that resolves when the write operations are complete. Or
   * rejects. Upon success, `size` will reflect the new file size.
   * If null, then in-place overwriting is not supported for this file.
   * Note the "overwrite" may be simulated with a download operation.
   */
  overwriteOriginal?: (blob: Blob) => Promise<void>;
  /**
   * A function that will delete the original file. Returns a promise that
   * resolves on success. Errors encountered are thrown from the message pipe
   * and handled by invoking functions in Google3.
   */
  deleteOriginalFile?: () => Promise<void>;
  /**
   * A function that will rename the original file. Returns a promise that
   * resolves to an enum value (see RenameResult in message_types) reflecting
   * the result of the rename. Errors encountered are thrown from the message
   * pipe and handled by invoking functions in Google3.
   */
  renameOriginalFile?: (newName: string) => Promise<number>;
  /**
   * A function that will save the provided blob in the file pointed to by
   * pickedFileToken. Once saved, the new file takes over this.token and becomes
   * currently writable. The original file is given a new token
   * and pushed forward in the navigation order.
   */
  saveAs?: (data: Blob, pickedFileToken: number) => Promise<void>;
  /**
   * A function that will show a file picker using the filename and an
   * appropriate starting folder for `this` file. Returns a writable file picked
   * by the user. The argument configures the dialog with the provided set of
   * predefined file extensions that the user may select from. This also helps
   * populate an extension on the chosen file. Contains an array of strings that
   * are "keys" to preconfigured settings for the file picker `accept` option.
   * E.g. ["PNG", "JPG", "PDF", "WEBP"].
   */
  getExportFile?:
      (filePickerAcceptOptionKeys: string[]) => Promise<AbstractFile>;
  /**
   * If defined, resolves a placeholder `blob` to a DOM File object using IPC
   * with the trusted context. Propagates exceptions. Does not modify `this`,
   * and always performs IPC to handle cases where the previously obtained File
   * is no longer accessible. E.g. if a file changes on disk (i.e. its mtime
   * changes), security checks may invalidate an old `File`, requiring it to be
   * "reopened".
   */
  openFile?: () => Promise<File>;
}

/**
 * Wraps an HTML FileList object.
 */
declare interface AbstractFileList {
  length: number;
  /**
   * The index of the currently active file which navigation and other file
   * operations are performed relative to. Defaults to -1 if file list is empty.
   */
  currentFileIndex: number;
  item(index: number): AbstractFile|null;
  /**
   * Loads the next file in the navigation order into the media app.
   * @param currentFileToken the token of the file that is currently
   *     loaded into the media app.
   */
  loadNext(currentFileToken?: number): Promise<void>;
  /**
   * Loads the previous file in the navigation order into the media app.
   */
  loadPrev(currentFileToken?: number): Promise<void>;
  /**
   * @param observer invoked when the size or contents of the file list changes.
   */
  addObserver(observer: (fileList: AbstractFileList) => void): void;
  /**
   * Request for the user to be prompted with an open file dialog. Files chosen
   * will be added to the last received file list.
   * TODO(b/230670565): Remove the undefined here once we can ensure all file
   * lists implement a openFilesWithFilePicker function.
   */
  openFilesWithFilePicker?:
      (acceptTypeKeys: string[], startInFolder?: AbstractFile,
       isSingleFile?: boolean) => Promise<void>;
  /**
   * Filters items represented by this file list in place, possibly changing the
   * length. Only items for which the filter returns true are kept.
   */
  filterInPlace?: (filter: (file: AbstractFile) => boolean) => void;
}

/**
 * Represents a box with top-left coordinates and a width and height.
 */
declare class Rect {
  /**
   * Represents a box with top-left coordinates and a width and height.
   * @param left Left.
   * @param top Top.
   * @param width Width.
   * @param height Height.
   */
  constructor(left: number, top: number, width: number, height: number);
  height: number;
  left: number;
  top: number;
  width: number;
}

/**
 * The page metadata using Closure. This should be deleted on migrating to the
 * geometry RectF in the MediaApp code.
 */
declare class PageMetadataWithClosureRect {
  id: string;
  rect: Rect;
}

/**
 * The delegate which exposes open source privileged WebUi functions to
 * MediaApp.
 */
declare interface ClientApiDelegate {
  /**
   * Opens up the built-in chrome feedback dialog.
   * @return Promise which resolves when the request has been
   *     acknowledged, if the dialog could not be opened the promise resolves
   *     with an error message, resolves with null otherwise.
   */
  openFeedbackDialog(): Promise<string>;
  /**
   * Toggles browser fullscreen mode.
   */
  toggleBrowserFullscreenMode?: () => Promise<void>;
  /**
   * Request for the user to be prompted with a save file dialog. Once the user
   * selects a location a new file handle is created and a new AbstractFile
   * representing that file will be returned. This can be then used in a save as
   * operation.
   * @param suggestedName The name to suggest in the file picker.
   * @param mimeType Fallback MIME type (deprecated).
   * @param acceptTypeKeys File filter configuration for the file
   * picker dialog. See getExportFile.
   */
  requestSaveFile(
      suggestedName: string, mimeType: string,
      acceptTypeKeys: string[]): Promise<AbstractFile>;
  /**
   * Notify MediaApp that the current file has been updated.
   */
  notifyCurrentFile(name?: string, type?: string): void;
  /**
   * Attempts to extract a JPEG "preview" from a RAW image file. Throws on any
   * failure. Note this is typically a full-sized preview, not a thumbnail.
   * @return A Blob-backed File with type: image/jpeg.
   */
  extractPreview(file: Blob): Promise<File>;
  /**
   * Passes the provided `blobUuid` to a sandboxed viewer in a popup window.
   * This enables the trusted context to open the popup so that it does not
   * appear with UI suggesting to the user that it is insecure. Only the UUID of
   * the blob should be passed (hex digits and hyphens), which will reconstruct
   * the blob URL in the sandbox. The provided `title` will be used to set the
   * document title (e.g., to include the filename), which will appear in the
   * popup title bar and shelf context menu.
   */
  openInSandboxedViewer?: (title: string, blobUuid: string) => void;
  /**
   * Opens the provided `url` in a browser tab.
   */
  openUrlInBrowserTab?: (url: string) => void;
  /**
   * Reloads the main frame, reloading launch files.
   */
  reloadMainFrame?: () => void;
  /**
   * Indicates to the WebUI Controller that a trigger for displaying the PDF
   * HaTS survey has occurred.
   */
  maybeTriggerPdfHats?: () => void;
  /**
   * Alert the OCR service that the PDF's page metadata has changed.
   */
  pageMetadataUpdated(pageMetadata: PageMetadataWithClosureRect[]): void;
  /**
   * Alert the OCR service that a specific page's contents has changed and
   * should have OCR applied again.
   */
  pageContentsUpdated(dirtyPageId: string): void;
  /**
   * Called whenever the viewport changes, e.g. due to scrolling, zooming,
   * resizing the window, or opening and closing toolbars/panels.
   * @param viewportBox The new bounding box of the viewport.
   * @param scaleFactor The ratio between CSS pixels (i.e. ignoring browser
   *     and pinch zoom) and ink units. Larger numbers indicate the document
   *     is more zoomed in.
   */
  viewportUpdated(viewportBox: Rect, scaleFactor: number): void;
}

/**
 * The client Api for interacting with the media app instance.
 */
declare interface ClientApi {
  /**
   * Looks up handler(s) and loads media via FileList.
   */
  loadFiles(files: AbstractFileList): Promise<void>;
  /**
   * Sets the delegate through which MediaApp can access open-source privileged
   * WebUI methods.
   */
  setDelegate(delegate: ClientApiDelegate|null): void;
  /**
   * If a document is currently loaded, scrolls and zooms to the given viewport.
   */
  setViewport(viewport: Rect): Promise<void>;
}

/**
 * Launch data that can be read by the app when it first loads.
 */
declare interface CustomLaunchData {
  delegate?: ClientApiDelegate;
  files: AbstractFileList;
}

interface Window {
  customLaunchData: CustomLaunchData;
}
