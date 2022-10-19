// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LensErrorType, LensFormElement} from './lens_form.js';
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

export interface LensUploadDialogElement {
  $: {
    dialog: HTMLDivElement,
    lensForm: LensFormElement,
    dragDropArea: HTMLDivElement,
  };
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
    afterNextRender(this, () => this.attachOutsideHandler_());
  }

  closeDialog() {
    this.dialogState_ = DialogState.HIDDEN;
    this.detachOutsideHandler_();
    this.dispatchEvent(new Event('close-lens-search'));
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

  private outsideClickHandler_ = (event: MouseEvent) => {
    const outsideDialog = !event.composedPath().includes(this.$.dialog);
    if (outsideDialog) {
      this.closeDialog();
    }
  };

  private outsideKeyHandler_ = (event: KeyboardEvent) => {
    if (event.key === 'Escape') {
      this.closeDialog();
    }
  };

  private attachOutsideHandler_() {
    if (!this.outsideHandlerAttached_) {
      document.addEventListener('click', this.outsideClickHandler_);
      document.addEventListener('keydown', this.outsideKeyHandler_);
      this.outsideHandlerAttached_ = true;
    }
  }

  private detachOutsideHandler_() {
    if (this.outsideHandlerAttached_) {
      document.removeEventListener('click', this.outsideClickHandler_);
      document.removeEventListener('keydown', this.outsideKeyHandler_);
      this.outsideHandlerAttached_ = false;
    }
  }

  private onCloseButtonClick_() {
    this.closeDialog();
  }

  private onOfflineRetryButtonClick_() {
    this.setOnlineState_();
  }

  private onUploadFileClick_() {
    this.$.lensForm.openSystemFilePicker();
  }

  private handleFormLoading_() {
    this.dialogState_ = DialogState.LOADING;
  }

  private handleFormError_(_event: CustomEvent<LensErrorType>) {
    switch (_event.detail) {
      case LensErrorType.MULTIPLE_FILES:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.MULTIPLE_FILES;
        break;
      case LensErrorType.NO_FILE:
        this.dialogState_ = DialogState.NORMAL;
        this.lensErrorMessage_ = LensErrorMessage.NONE;
        break;
      case LensErrorType.FILE_TYPE:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.FILE_TYPE;
        break;
      case LensErrorType.FILE_SIZE:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.FILE_SIZE;
        break;
      case LensErrorType.INVALID_SCHEME:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.SCHEME;
        break;
      case LensErrorType.INVALID_URL:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.CONFORMANCE;
        break;
      case LensErrorType.LENGTH_TOO_GREAT:
        this.dialogState_ = DialogState.ERROR;
        this.lensErrorMessage_ = LensErrorMessage.CONFORMANCE;
        break;
      default:
        this.dialogState_ = DialogState.NORMAL;
        this.lensErrorMessage_ = LensErrorMessage.NONE;
    }
  }

  private onUrlKeyDown_(event: KeyboardEvent) {
    if (event.key === 'Enter') {
      event.preventDefault();
      this.onSubmitUrl_();
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
    }
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'ntp-lens-upload-dialog': LensUploadDialogElement;
  }
}

customElements.define(LensUploadDialogElement.is, LensUploadDialogElement);
