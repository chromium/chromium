// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';

import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {isRTL} from 'chrome://resources/js/util.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SaveToDriveState} from '../constants.js';

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

  protected isUploading_(): boolean {
    return this.state === SaveToDriveState.UPLOADING;
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
    switch (this.state) {
      case SaveToDriveState.UPLOADING:
        this.dialogTitle_ = this.i18n('saveToDriveDialogUploadingTitle');
        this.description_ = window.trustedTypes!.emptyHTML;
        // TODO(crbug.com/427451594): Use a translated fileMetadata string.
        this.fileMetadata_ = '304/503 KB · 4 seconds left';
        break;
      // TODO(crbug.com/427451594): Set the strings for the following states.
      case SaveToDriveState.SUCCESS:
      case SaveToDriveState.CONNECTION_ERROR:
      case SaveToDriveState.STORAGE_FULL_ERROR:
      case SaveToDriveState.SESSION_TIMEOUT_ERROR:
      case SaveToDriveState.UNKNOWN_ERROR:
        this.dialogTitle_ = `Save to Drive ${String(this.state)}`;
        this.description_ = sanitizeInnerHtml(
            `A string that contains a <a>link</a> ${String(this.state)}`);
        this.fileMetadata_ =
            `304/503 KB · 4 seconds left (${String(this.state)})`;
        break;
      default:
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
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-save-to-drive-bubble': ViewerSaveToDriveBubbleElement;
  }
}

customElements.define(
    ViewerSaveToDriveBubbleElement.is, ViewerSaveToDriveBubbleElement);
