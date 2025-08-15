// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';

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
      bytesToTransfer: {type: Number},
      bytesTransferred: {type: Number},
      fileName: {type: String},
      state: {type: String},

      description_: {
        type: String,
        state: true,
      },

      dialogTitle_: {
        type: String,
        state: true,
      },

      fileMetadata_: {
        type: String,
        state: true,
      },
    };
  }

  accessor bytesToTransfer: number = 0;
  accessor bytesTransferred: number = 0;
  accessor fileName: string = '';
  accessor state: SaveToDriveState = SaveToDriveState.UNINITIALIZED;
  protected accessor description_: TrustedHTML = sanitizeInnerHtml('');
  protected accessor dialogTitle_: string = '';
  protected accessor fileMetadata_: string = '';

  private anchor_: HTMLElement|null = null;
  private eventTracker_: EventTracker = new EventTracker();

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

  showAt(anchor: HTMLElement) {
    this.$.dialog.show();
    this.anchor_ = anchor;
    this.positionDialog_();
    this.$.dialog.focus();
    this.eventTracker_.add(window, 'resize', this.positionDialog_.bind(this));
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
  }

  protected onCloseClick_() {
    this.$.dialog.close();
  }

  protected onDialogClose_() {
    this.eventTracker_.removeAll();
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
    // TODO(crbug.com/427451594): Replace the `fileMetadata_` switch statement
    // below with translated strings from the browser process.
    switch (this.state) {
      case SaveToDriveState.UPLOADING:
        this.fileMetadata_ = '304/503 KB · 4 seconds left';
        break;
      case SaveToDriveState.SUCCESS:
        this.fileMetadata_ = '503 KB · Done';
        break;
      default:
        this.fileMetadata_ = '';
        break;
    }
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

  private updateDescription_() {
    switch (this.state) {
      case SaveToDriveState.UNINITIALIZED:
      case SaveToDriveState.UPLOADING:
        this.description_ = window.trustedTypes!.emptyHTML;
        break;
      case SaveToDriveState.SUCCESS:
        // TODO(crbug.com/427451594): Replace `PLACEHOLDER` with the folder name
        // we get from the server.
        this.description_ =
            this.i18nAdvanced('saveToDriveDialogSuccessMessage', {
              tags: ['b'],
              substitutions: [
                'PLACEHOLDER',
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
