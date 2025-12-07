// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';
import 'chrome://resources/cr_elements/icons.html.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReached} from 'chrome://resources/js/assert.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SaveToDriveBubbleRequestType, SaveToDriveState} from '../constants.js';

import {getCss} from './viewer_save_to_drive_bubble.css.js';
import {getHtml} from './viewer_save_to_drive_bubble.html.js';

const DISMISS_TIMEOUT_MS = 5000;

const ViewerSaveToDriveBubbleElementBase = I18nMixinLit(CrLitElement);

export interface ViewerSaveToDriveBubbleElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class ViewerSaveToDriveBubbleElement extends
    ViewerSaveToDriveBubbleElementBase {
  static get is() {
    return 'viewer-save-to-drive-bubble';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      docTitle: {type: String},
      progress: {type: Object},
      state: {type: String},

      description_: {
        type: String,
        state: true,
      },

      dialogTitle_: {
        type: String,
        state: true,
      },
    };
  }

  accessor docTitle: string = '';
  accessor progress: chrome.pdfViewerPrivate.SaveToDriveProgress = {
    status: chrome.pdfViewerPrivate.SaveToDriveStatus.NOT_STARTED,
    errorType: chrome.pdfViewerPrivate.SaveToDriveErrorType.NO_ERROR,
  };
  accessor state: SaveToDriveState = SaveToDriveState.UNINITIALIZED;
  protected accessor description_: TrustedHTML = sanitizeInnerHtml('');
  protected accessor dialogTitle_: string = '';

  private anchor_: HTMLElement|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  private dismissTimeoutId_: number|null = null;

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('state')) {
      this.onStateChanged_();
    }
  }

  // If `autoDismiss` is true, the bubble will be automatically dismissed after
  // 5 seconds. However, if the bubble is already open manually, the timeout
  // will be ignored.
  showAt(anchor: HTMLElement, autoDismiss: boolean = false) {
    if (this.$.dialog.open && autoDismiss && !this.dismissTimeoutId_) {
      return;
    }
    this.$.dialog.show();
    this.anchor_ = anchor;
    this.positionDialog_();
    this.$.dialog.focus();
    this.eventTracker_.remove(window, 'resize');
    this.eventTracker_.add(window, 'resize', this.positionDialog_.bind(this));
    if (autoDismiss) {
      this.setDismissTimeout_();
    }
  }

  protected getFileName_(): string {
    return this.progress.fileName ?? this.docTitle;
  }

  protected getFileSizeBytes_(): number {
    return this.progress.fileSizeBytes ?? 0;
  }

  protected getMetadata_(): string {
    return this.progress.fileMetadata ?? '';
  }

  protected getUploadedBytes_(): number {
    return this.progress.uploadedBytes ?? 0;
  }

  protected isSaveToDriveState_(state: SaveToDriveState): boolean {
    return this.state === state;
  }

  protected onRequestButtonClick_() {
    let requestType: SaveToDriveBubbleRequestType;
    switch (this.state) {
      case SaveToDriveState.UPLOADING:
        requestType = SaveToDriveBubbleRequestType.CANCEL_UPLOAD;
        break;
      case SaveToDriveState.STORAGE_FULL_ERROR:
        requestType = SaveToDriveBubbleRequestType.MANAGE_STORAGE;
        break;
      case SaveToDriveState.SUCCESS:
        requestType = SaveToDriveBubbleRequestType.OPEN_IN_DRIVE;
        break;
      case SaveToDriveState.CONNECTION_ERROR:
      case SaveToDriveState.SESSION_TIMEOUT_ERROR:
        requestType = SaveToDriveBubbleRequestType.RETRY;
        break;
      default:
        assertNotReached(`Invalid bubble action: ${this.state}`);
    }
    this.fire('save-to-drive-bubble-action', requestType);
    this.$.dialog.close();
  }

  protected onCloseClick_() {
    this.fire(
        'save-to-drive-bubble-action',
        SaveToDriveBubbleRequestType.DIALOG_CLOSED);
    this.$.dialog.close();
  }

  protected onDialogClose_() {
    this.eventTracker_.removeAll();
    if (this.dismissTimeoutId_) {
      clearTimeout(this.dismissTimeoutId_);
      this.dismissTimeoutId_ = null;
    }
  }

  protected onFocusout_(e: FocusEvent) {
    if (this.$.dialog.contains(e.relatedTarget as Node) ||
        e.composedPath()[0]! !== this.$.dialog) {
      return;
    }
    this.$.dialog.close();
  }

  private onStateChanged_() {
    this.updateDescription_();
    this.updateDialogTitle_();
  }

  private positionDialog_() {
    if (!this.anchor_ || !this.$.dialog.open) {
      return;
    }

    const anchorBoundingClientRect = this.anchor_.getBoundingClientRect();
    if (isRTL()) {
      this.$.dialog.style.right = `${
          window.innerWidth - anchorBoundingClientRect.left -
          this.$.dialog.offsetWidth}px`;
    } else {
      this.$.dialog.style.left = `${
          this.anchor_.offsetLeft + this.anchor_.offsetWidth -
          this.$.dialog.offsetWidth}px`;
    }

    // By default, align the dialog below the anchor. If the window is too
    // small, show it above the anchor.
    if (anchorBoundingClientRect.bottom + this.$.dialog.offsetHeight >=
        window.innerHeight) {
      this.$.dialog.style.top =
          `${this.anchor_.offsetTop - this.$.dialog.offsetHeight}px`;
    } else {
      this.$.dialog.style.top =
          `${this.anchor_.offsetTop + this.anchor_.offsetHeight}px`;
    }
  }

  private setDismissTimeout_() {
    this.dismissTimeoutId_ = setTimeout(() => {
      this.dismissTimeoutId_ = null;
      this.$.dialog.close();
    }, DISMISS_TIMEOUT_MS);
  }

  private updateDescription_() {
    switch (this.state) {
      case SaveToDriveState.UNINITIALIZED:
      case SaveToDriveState.UPLOADING:
        this.description_ = window.trustedTypes!.emptyHTML;
        break;
      case SaveToDriveState.SUCCESS:
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogSuccessMessage', {
              tags: ['b'],
              substitutions: [
                this.progress.parentFolderName ?? '',
              ],
            });
        break;
      case SaveToDriveState.CONNECTION_ERROR:
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogConnectionErrorMessage');
        break;
      case SaveToDriveState.STORAGE_FULL_ERROR:
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogStorageFullErrorMessage');
        break;
      case SaveToDriveState.SESSION_TIMEOUT_ERROR:
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogSessionTimeoutErrorMessage');
        break;
      case SaveToDriveState.UNKNOWN_ERROR:
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogUnknownErrorMessage', {
              tags: ['a'],
              substitutions: [
                this.i18n('pdfSaveToDriveHelpCenterURL'),
              ],
            });
        break;
      default:
        assertNotReached(`Invalid state for description: ${this.state}`);
    }
  }

  private updateDialogTitle_() {
    switch (this.state) {
      case SaveToDriveState.UNINITIALIZED:
        this.dialogTitle_ = this.state;
        break;
      case SaveToDriveState.UPLOADING:
        this.dialogTitle_ = this.i18n('saveToDriveDialogUploadingTitle');
        break;
      case SaveToDriveState.SUCCESS:
        this.dialogTitle_ = this.i18n('saveToDriveDialogSuccessTitle');
        break;
      case SaveToDriveState.CONNECTION_ERROR:
      case SaveToDriveState.STORAGE_FULL_ERROR:
      case SaveToDriveState.SESSION_TIMEOUT_ERROR:
      case SaveToDriveState.UNKNOWN_ERROR:
        this.dialogTitle_ = this.i18n('saveToDriveDialogErrorTitle');
        break;
      default:
        assertNotReached(`Invalid state for dialog title: ${this.state}`);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-save-to-drive-bubble': ViewerSaveToDriveBubbleElement;
  }
}

customElements.define(
    ViewerSaveToDriveBubbleElement.is, ViewerSaveToDriveBubbleElement);
