// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';

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

export interface LensUploadDialogElement {
  $: {
    dialog: HTMLDivElement,
    lensForm: LensFormElement,
  };
}

// Modal that lets the user upload images for search on Lens.
export class LensUploadDialogElement extends PolymerElement {
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
      isHidden_: {
        type: Boolean,
        computed: `computeIsHidden_(dialogState_)`,
      },
      isNormal_: {
        type: Boolean,
        computed: `computeIsNormal_(dialogState_)`,
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
      isOffline_: {
        type: Boolean,
        computed: `computeIsOffline_(dialogState_)`,
        reflectToAttribute: true,
      },
    };
  }

  private outsideClickHandler_: (event: MouseEvent) => void;
  private dialogState_ = DialogState.HIDDEN;
  private outsideClickHandlerAttached_ = false;

  private computeIsHidden_(dialogState: DialogState): boolean {
    return dialogState === DialogState.HIDDEN;
  }

  private computeIsNormal_(dialogState: DialogState): boolean {
    return dialogState === DialogState.NORMAL;
  }

  private computeIsDragging_(dialogState: DialogState): boolean {
    return dialogState === DialogState.DRAGGING;
  }

  private computeIsLoading_(dialogState: DialogState): boolean {
    return dialogState === DialogState.LOADING;
  }

  private computeIsOffline_(dialogState: DialogState): boolean {
    return dialogState === DialogState.OFFLINE;
  }

  constructor() {
    super();
    this.outsideClickHandler_ = (event: MouseEvent) => {
      const outsideDialog = !event.composedPath().includes(this.$.dialog);
      if (outsideDialog) {
        this.closeDialog();
      }
    };
  }

  override connectedCallback() {
    super.connectedCallback();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.detachOutsideClickHandler_();
  }

  openDialog() {
    this.setOnlineState_();
    // Click handler needs to be attached outside of the initial event handler,
    // otherwise the click of the icon which initially opened the dialog would
    // also be registered in the outside click handler, causing the dialog to
    // immediately close after opening.
    afterNextRender(this, () => this.attachOutsideClickHandler_());
  }

  closeDialog() {
    this.dialogState_ = DialogState.HIDDEN;
    this.detachOutsideClickHandler_();
    this.dispatchEvent(new Event('close-lens-search'));
  }

  /**
   * Checks to see if the user is online or offline and sets the dialog state
   * accordingly.
   */
  private setOnlineState_() {
    this.dialogState_ = WindowProxy.getInstance().onLine ? DialogState.NORMAL :
                                                           DialogState.OFFLINE;
  }

  private attachOutsideClickHandler_() {
    if (!this.outsideClickHandlerAttached_) {
      document.addEventListener('click', this.outsideClickHandler_);
      this.outsideClickHandlerAttached_ = true;
    }
  }

  private detachOutsideClickHandler_() {
    if (this.outsideClickHandlerAttached_) {
      document.removeEventListener('click', this.outsideClickHandler_);
      this.outsideClickHandlerAttached_ = false;
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
    // TODO(crbug.com/1367506): Implement error state.
  }
}
declare global {
  interface HTMLElementTagNameMap {
    'ntp-lens-upload-dialog': LensUploadDialogElement;
  }
}

customElements.define(LensUploadDialogElement.is, LensUploadDialogElement);
