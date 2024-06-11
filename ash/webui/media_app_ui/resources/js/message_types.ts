// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Message definitions passed over the MediaApp privileged/unprivileged pipe.
 */

/** Enum for message types. */
export enum Message {
  DELETE_FILE = 'delete-file',
  EDIT_IN_PHOTOS = 'edit-in-photos',
  IFRAME_READY = 'iframe-ready',
  IS_FILE_ARC_WRITABLE = 'is-file-arc-writable',
  IS_FILE_BROWSER_WRITABLE = 'is-file-browser-writable',
  LOAD_EXTRA_FILES = 'load-extra-files',
  LOAD_FILES = 'load-files',
  MAYBE_TRIGGER_PDF_HATS = 'maybe-trigger-pdf-hats',
  NAVIGATE = 'navigate',
  NOTIFY_CURRENT_FILE = 'notify-current-file',
  OPEN_ALLOWED_FILE = 'open-allowed-file',
  OPEN_FEEDBACK_DIALOG = 'open-feedback-dialog',
  OPEN_FILES_WITH_PICKER = 'open-files-with-picker',
  OPEN_IN_SANDBOXED_VIEWER = 'open-in-sandboxed-viewer',
  OVERWRITE_FILE = 'overwrite-file',
  RELOAD_MAIN_FRAME = 'reload-main-frame',
  RENAME_FILE = 'rename-file',
  REQUEST_SAVE_FILE = 'request-save-file',
  SAVE_AS = 'save-as',
  SUBMIT_FORM = 'submit-form',
  TOGGLE_BROWSER_FULLSCREEN_MODE = 'toggle-browser-fullscreen-mode',
}

/**
 * Message sent by the unprivileged context to request the privileged context to
 * delete the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 */
export interface DeleteFileMessage {
  token: number;
}

/** Representation of a file passed in on the LoadFilesMessage. */
export interface FileContext {
  token: number;
  file: File|null;
  name: string;
  error: string;
  canDelete: boolean;
  canRename: boolean;
}

/**
 * Message sent by the privileged context to the unprivileged context indicating
 * the files available to open.
 */
export interface LoadFilesMessage {
  currentFileIndex: number;
  files: FileContext[];
}

/**
 * Message sent by the unprivileged context to the privileged context to check
 * whether or not the current file is writable according to ARC. If the supplied
 * file `token` is invalid the request is rejected.
 */
export interface IsFileArcWritableMessage {
  token: number;
}

export interface IsFileArcWritableResponse {
  writable: boolean;
}

/**
 * Message sent by the unprivileged context to the privileged context to check
 * whether or not the current file is writable according to Ash. If the supplied
 * file `token` is invalid the request is rejected.
 */
export interface IsFileBrowserWritableMessage {
  token: number;
}

export interface IsFileBrowserWritableResponse {
  writable: boolean;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * the current file to be launched in Photos with an edit intent. If the
 * supplied file `token` is invalid the request is rejected.
 */
export interface EditInPhotosMessage {
  token: number;
  mimeType: string;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * that the currently writable file be overridden with the provided `blob`.
 * If the supplied file `token` is invalid the request is rejected.
 */
export interface OverwriteFileMessage {
  token: number;
  blob: Blob;
}

/**
 * Response message to a successful overwrite (no error thrown). If fields are
 * defined, indicates that an overwrite failed, but the user was able to select
 * a new file from a file picker. The UI should update to reflect the new name.
 * `errorName` is the error on the write attempt that triggered the picker.
 */
export interface OverwriteViaFilePickerResponse {
  renamedTo?: string;
  errorName?: string;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * the app be relaunched with the next/previous file in the current directory
 * set to writable. Direction is a number specifying how many files to advance
 * by, positive integers specify files "next" in the navigation order whereas
 * negative integers specify files "back" in the navigation order.
 * The `currentFileToken` is the token of the file which is currently opened,
 * this is used to decide what `direction` is in reference to.
 */
export interface NavigateMessage {
  direction: number;
  currentFileToken?: number;
}

/** Enum for results of renaming a file. */
export enum RenameResult {
  FILE_NO_LONGER_IN_LAST_OPENED_DIRECTORY = -1,
  SUCCESS = 0,
  FILE_EXISTS = 1
}

/**
 * Message sent by the unprivileged context to request the privileged context to
 * rename the currently writable file.
 * If the supplied file `token` is invalid the request is rejected.
 */
export interface RenameFileMessage {
  token: number;
  newFilename: string;
}

export interface RenameFileResponse {
  renameResult: RenameResult;
}

/**
 * Message sent by the unprivileged context to notify the privileged context
 * that the current file has been updated.
 */
export interface NotifyCurrentFileMessage {
  name?: string;
  type?: string;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the user to be prompted with a save file dialog. Once the user selects a
 * location a new file handle is created and a unique token to that file will be
 * returned. The file extension on `suggestedName` and the provided `mimeType`
 * are used to inform the save as dialog what file should be created. Once the
 * native filesystem api allows, this save as dialog will additionally have the
 * filename input be pre-filled with `suggestedName`.
 * If a non-zero startInToken is provided, the corresponding file handle is used
 * to start the file picker in the same folder as that file.
 * The `accept` array contains keys of preconfigured file filters to include on
 * the file picker file type dropdown. These are keys such as "PDF", "JPG",
 * "PNG", etc. that are known on both sides of API boundary.
 */
export interface RequestSaveFileMessage {
  suggestedName: string;
  mimeType: string;
  startInToken: number;
  accept: string[];
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * to show a file picker. Picked files will be sent down the launch path.
 * The `accept` array contains keys of preconfigured file filters to include on
 * the file picker file type dropdown. These are keys such as "AUDIO", "IMAGE",
 * "PDF", etc. that are known on both sides of API boundary.
 * `isSingleFile` prevents a user selecting more than one file.
 */
export interface OpenFilesWithPickerMessage {
  startInToken: number;
  accept: string[];
  isSingleFile?: boolean;
}

/**
 * Response message sent by the privileged context with a unique identifier for
 * the new writable file created on disk by the corresponding request save file
 * message.
 */
export interface RequestSaveFileResponse {
  pickedFileContext: FileContext;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * for the provided blob to be saved in the location specified by
 * `pickedFileToken`. Once saved the new file takes over oldFileToken if it is
 * provided, else it gives itself a fresh token, then it becomes currently
 * writable. The file specified by oldFileToken is given a new token and pushed
 * forward in the navigation order. This method can be called with any file, not
 * just the currently writable file.
 */
export interface SaveAsMessage {
  blob: Blob;
  oldFileToken?: number;
  pickedFileToken: number;
}

/**
 * Response message sent by the privileged context with the name of the new
 * current file.
 */
export interface SaveAsResponse {
  newFilename: string;
}

/**
 * Message sent from the app to open a sandboxed viewer for a Blob in a popup.
 * `title` is the window title for the popup and `blobUuid` is the UUID of the
 * blob which will be reconstructed as a blob URL in the sandbox.
 */
export interface OpenInSandboxedViewerMessage {
  title: string;
  blobUuid: string;
}

/**
 * Message sent by the unprivileged context to the privileged context requesting
 * an "allowed" file to be opened.
 */
export interface OpenAllowedFileMessage {
  fileToken: number;
}

/**
 * Response message sent by the privileged context to "open" an allowed file.
 */
export interface OpenAllowedFileResponse {
  file: File;
}
