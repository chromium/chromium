// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LensErrorType, LensFormElement, LensSubmitType} from './lens_form.js';
import {getTemplate} from './lens_upload_dialog.html.js';
import {WindowProxy} from './window_proxy.js';

enum DialogState {
  // Dialog is currently hidden from the user.
  HIDDEN,
  // Dialog is open and awaiting user input.
  NORMAL,
  // User is dragging a file over the UI.
  DRAGGING,
  // User dropped a file and a request to Lens is started.
  LOADING,
  // User selected a file that resulted in an error.
  ERROR,
  // User is offline.
  OFFLINE,
}

enum LensErrorMessage {
  // No error.
  NONE,
  // User provided an invalid file format.
  FILE_TYPE,
  // User provided a file that is too large to handle.
  FILE_SIZE,
  // User provided multiple files.
  MULTIPLE_FILES,
  // User provided URL with improper scheme.
  SCHEME,
  // User provided invalid URL.
  CONFORMANCE,
  // User provided multiple URLs.
  MULTIPLE_URLS,
}

const EventKeys = {
  ENTER: 'Enter',
  ESCAPE: 'Escape',
  SPACE: ' ',
  TAB: 'Tab',
};

export interface LensUploadDialogElement {
  $: {
    dialog: HTMLDivElement,
    lensForm: LensFormElement,
    dragDropArea: HTMLDivElement,
    closeButton: CrIconButtonElement,
  };
}

/**
 * List of possible upload dialog actions. This enum must match with the
 * numbering for NewTabPageLensUploadDialogActions in histogram/enums.xml. These
 * values are persisted to logs. Entries should not be renumbered, removed or
 * reused.
 */
export enum LensUploadDialogAction {
  URL_SUBMITTED = 0,
  FILE_SUBMITTED = 1,
  IMAGE_DROPPED = 2,
  DIALOG_OPENED = 3,
  DIALOG_CLOSED = 4,
  ERROR_SHOWN = 5,
}

/**
 * List of possible upload dialog errors. This enum must match with the
 * numbering for NewTabPageLensUploadDialogErrors in histogram/enums.xml. These
 * values are persisted to logs. Entries should not be renumbered, removed or
 * reused.
 */
export enum LensUploadDialogError {
  FILE_SIZE = 0,
  FILE_TYPE = 1,
  MULTIPLE_FILES = 2,
  MULTIPLE_URLS = 3,
  LENGTH_TOO_GREAT = 4,
  INVALID_SCHEME = 5,
  INVALID_URL = 6,
  NETWORK_ERROR = 7,
}

export function recordLensUploadDialogAction(action: LensUploadDialogAction) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Lens.UploadDialog.DialogAction', action,
      Object.keys(LensUploadDialogAction).length);
}

export function recordLensUploadDialogError(action: LensUploadDialogError) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.Lens.UploadDialog.DialogError', action,
      Object.keys(LensUploadDialogError).length);
}

const LensUploadDialogElementBase = I18nMixin(PolymerElement);

// Modal that lets the user upload images for search on Lens.
export class LensUploadDialogElement extends LensUploadDialogElementBase {
  static get is() {
    return 'ntp-lens-upload-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      dialogState_: {
        type: DialogState,
      },
      lensErrorMessage_: {
        type: LensErrorMessage,
      },
      isHidden_: {
        type: Boolean,
        computed: `computeIsHidden_(dialogState_)`,
      },
      isNormalOrError_: {
        type: Boolean,
        computed: `computeIsNormalOrError_(dialogState_)`,
        reflectToAttribute: true,
      },
      isDragging_: {
        type: Boolean,
        computed: `computeIsDragging_(dialogState_)`,
        reflectToAttribute: true,
      },
      isLoading_: {
        type: Boolean,
        computed: `computeIsLoading_(dialogState_)`,
        reflectToAttribute: true,
      },
      isError_: {
        type: Boolean,
        computed: `computeIsError_(dialogState_)`,
        reflectToAttribute: true,
      },
      isOffline_: {
        type: Boolean,
        computed: `computeIsOffline_(dialogState_)`,
        reflectToAttribute: true,
      },
      uploadUrl_: {
        type: String,
      },
    };
  }

  private dialogState_ = DialogState.HIDDEN;
  private lensErrorMessage_ = LensErrorMessage.NONE;
  private outsideHandlerAttached_ = false;
  private uploadUrl_: string = '';
  private dragCount: number = 0;

  private computeIsHidden_(dialogState: DialogState): boolean {
    return dialogState === DialogState.HIDDEN;
  }

  private computeIsNormalOrError_(dialogState: DialogState): boolean {
    return dialogState === DialogState.NORMAL ||
        dialogState === DialogState.ERROR;
  }

  private computeIsDragging_(dialogState: DialogState): boolean {
    return dialogState === DialogState.DRAGGING;
  }

  private computeIsLoading_(dialogState: DialogState): boolean {
    return dialogState === DialogState.LOADING;
  }

  private computeIsError_(dialogState: DialogState): boolean {
    return dialogState === DialogState.ERROR;
  }

  private computeIsOffline_(dialogState: DialogState): boolean {
    return dialogState === DialogState.OFFLINE;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.openDialog();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.detachOutsideHandler_();
  }

  openDialog() {
    this.setOnlineState_();
    // Click handler needs to be attached outside of the initial event handler,
    // otherwise the click of the icon which initially opened the dialog would
    // also be registered in the outside click handler, causing the dialog to
    // immediately close after opening.
    afterNextRender(this, () => {
      this.attachOutsideHandler_();
      if (this.computeIsOffline_(this.dialogState_)) {
        this.shadowRoot!.getElementById('offlineRetryButton')?.focus();
      } else {
        this.shadowRoot!.getElementById('uploadText')?.focus();
      }
    });
    recordLensUploadDialogAction(LensUploadDialogAction.DIALOG_OPENED);
  }

  closeDialog() {
    this.dialogState_ = DialogState.HIDDEN;
    this.detachOutsideHandler_();
    this.dispatchEvent(new Event('close-lens-search'));
    recordLensUploadDialogAction(LensUploadDialogAction.DIALOG_CLOSED);
  }

  private getErrorString_(lensErrorMessage: LensErrorMessage) {
    switch (lensErrorMessage) {
      case LensErrorMessage.FILE_TYPE:
        return this.i18n('lensSearchUploadDialogErrorFileType');
      case LensErrorMessage.FILE_SIZE:
        return this.i18n('lensSearchUploadDialogErrorFileSize');
      case LensErrorMessage.MULTIPLE_FILES:
        return this.i18n('lensSearchUploadDialogErrorMultipleFiles');
      case LensErrorMessage.SCHEME:
        return this.i18n('lensSearchUploadDialogValidationErrorScheme');
      case LensErrorMessage.CONFORMANCE:
        return this.i18n('lensSearchUploadDialogValidationErrorConformance');
      case LensErrorMessage.MULTIPLE_URLS:
        return this.i18n('lensSearchUploadDialogErrorMultipleUrls');
      default:
        return '';
    }
  }

  /**
   * Checks to see if the user is online or offline and sets the dialog state
   * accordingly.
   */
  private setOnlineState_() {
    this.dialogState_ = WindowProxy.getInstance().onLine ? DialogState.NORMAL :
                                                           DialogState.OFFLINE;
  }

  private outsideKeyHandler_ = (event: KeyboardEvent) => {
    if (event.key === EventKeys.ESCAPE) {
      this.closeDialog();
    }
  };

  private attachOutsideHandler_() {
    if (!this.outsideHandlerAttached_) {
      document.addEventListener('keydown', this.outsideKeyHandler_);
      this.outsideHandlerAttached_ = true;
    }
  }

  private detachOutsideHandler_() {
    if (this.outsideHandlerAttached_) {
      document.removeEventListener('keydown', this.outsideKeyHandler_);
      this.outsideHandlerAttached_ = false;
    }
  }

  private onCloseButtonKeydown_ = (event: KeyboardEvent) => {
    if (event.key === EventKeys.TAB &&
        (this.computeIsDragging_(this.dialogState_) ||
         this.computeIsLoading_(this.dialogState_))) {
      event.preventDefault();
      // In the dragging and loading states, the close button is the only
      // tabbable element in the dialog, so focus should stay on it.
    } else if (event.key === EventKeys.TAB && event.shiftKey) {
      event.preventDefault();
      if (this.computeIsNormalOrError_(this.dialogState_)) {
        this.shadowRoot!.getElementById('inputSubmit')?.focus();
      } else if (this.computeIsOffline_(this.dialogState_)) {
        this.shadowRoot!.getElementById('offlineRetryButton')?.focus();
      }
    }
  };

  private onOfflineRetryButtonKeydown_ = (event: KeyboardEvent) => {
    if (event.key === EventKeys.TAB && !event.shiftKey) {
      event.preventDefault();
      this.$.closeButton.focus();
    }
  };

  private onCloseButtonClick_() {
    this.closeDialog();
  }

  private onOfflineRetryButtonClick_() {
    this.setOnlineState_();
  }

  private onUploadFileKeyDown_(event: KeyboardEvent) {
    if (event.key === EventKeys.ENTER || event.key === EventKeys.SPACE) {
      this.$.lensForm.openSystemFilePicker();
    }
  }

  private onUploadFileClick_() {
    this.$.lensForm.openSystemFilePicker();
  }

  private handleFormLoading_(event: CustomEvent<LensSubmitType>) {
    this.dialogState_ = DialogState.LOADING;
    switch (event.detail) {
      case LensSubmitType.FILE:
        recordLensUploadDialogAction(LensUploadDialogAction.FILE_SUBMITTED);
        break;
      case LensSubmitType.URL:
        recordLensUploadDialogAction(LensUploadDialogAction.URL_SUBMITTED);
        break;
    }
  }

  private handleFormError_(event: CustomEvent<LensErrorType>) {
    switch (event.detail) {
      case LensErrorType.MULTIPLE_FILES:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.MULTIPLE_FILES;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.MULTIPLE_FILES);
        break;
      case LensErrorType.NO_FILE:
        this.dialogState_ = DialogState.NORMAL;
        this.lensErrorMessage_ = LensErrorMessage.NONE;
        break;
      case LensErrorType.FILE_TYPE:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.FILE_TYPE;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.FILE_TYPE);
        break;
      case LensErrorType.FILE_SIZE:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.FILE_SIZE;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.FILE_SIZE);
        break;
      case LensErrorType.INVALID_SCHEME:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.SCHEME;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.INVALID_SCHEME);
        break;
      case LensErrorType.INVALID_URL:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.CONFORMANCE;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.INVALID_URL);
        break;
      case LensErrorType.LENGTH_TOO_GREAT:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.CONFORMANCE;
        recordLensUploadDialogAction(LensUploadDialogAction.ERROR_SHOWN);
        recordLensUploadDialogError(LensUploadDialogError.LENGTH_TOO_GREAT);
        break;
      default:
        this.dialogState_ = DialogState.NORMAL;
        this.lensErrorMessage_ = LensErrorMessage.NONE;
    }
  }

  private onUrlKeyDown_(event: KeyboardEvent) {
    if (event.key === EventKeys.ENTER) {
      event.preventDefault();
      this.onSubmitUrl_();
    }
  }

  private onInputSubmitKeyDown_(event: KeyboardEvent) {
    if (event.key === EventKeys.ENTER || event.key === EventKeys.SPACE) {
      this.onSubmitUrl_();
    } else if (event.key === EventKeys.TAB && !event.shiftKey) {
      event.preventDefault();
      this.$.closeButton.focus();
    }
  }

  private onSubmitUrl_() {
    const url = this.uploadUrl_.trim();
    if (url.length > 0) {
      this.$.lensForm.submitUrl(url);
    }
  }

  private onDragEnter_(e: DragEvent) {
    e.preventDefault();
    this.dragCount += 1;

    if (this.dragCount === 1) {
      this.dialogState_ = DialogState.DRAGGING;
    }
  }

  private onDragOver_(e: DragEvent) {
    e.preventDefault();
  }

  private onDragLeave_(e: DragEvent) {
    e.preventDefault();
    this.dragCount -= 1;

    if (this.dragCount === 0) {
      this.dialogState_ = DialogState.NORMAL;
    }
  }

  private onDrop_(e: DragEvent) {
    e.preventDefault();
    this.dragCount = 0;

    if (e.dataTransfer) {
      this.$.lensForm.submitFileList(e.dataTransfer.files);
      recordLensUploadDialogAction(LensUploadDialogAction.IMAGE_DROPPED);
    }
  }

  private onFocusOut_(event: FocusEvent) {
    // Focus ensures that the file picker pop-up does not close dialog.
    const outsideDialog = document.hasFocus() &&
        (!event.relatedTarget ||
         !this.$.dialog.contains(event.relatedTarget as Node));
    if (outsideDialog) {
      this.closeDialog();
    }
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'ntp-lens-upload-dialog': LensUploadDialogElement;
  }
}

customElements.define(LensUploadDialogElement.is, LensUploadDialogElement);
