// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './circular_progress_ring.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {SaveToDriveState} from '../constants.js';

import {ViewerSaveControlsMixin} from './viewer_save_controls_mixin.js';
import {getCss} from './viewer_save_to_drive_controls.css.js';
import {getHtml} from './viewer_save_to_drive_controls.html.js';

const ViewerSaveControlsBase = ViewerSaveControlsMixin(CrLitElement);

export interface ViewerSaveToDriveControlsElement {
  $: {
    menu: CrActionMenuElement,
    save: CrIconButtonElement,
  };
}

export class ViewerSaveToDriveControlsElement extends ViewerSaveControlsBase {
  static get is() {
    return 'viewer-save-to-drive-controls';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      progress: {type: Number},
      state: {
        type: String,
        reflect: true,
      },
    };
  }

  accessor progress: number = 0;
  accessor state: SaveToDriveState = SaveToDriveState.UNINITIALIZED;

  protected getIronIcon(): string {
    return this.isUploading_() ? 'pdf:arrow-upward-alt' : 'pdf:add-to-drive';
  }

  protected isUploading_(): boolean {
    return this.state === SaveToDriveState.UPLOADING;
  }

  // ViewerSaveControlsMixin implementation.
  override getSaveButton(): CrIconButtonElement {
    return this.$.save;
  }

  // ViewerSaveControlsMixin implementation.
  override getSaveEventType(): string {
    return 'save-to-drive';
  }

  // ViewerSaveControlsMixin implementation.
  override getMenu(): CrActionMenuElement {
    return this.$.menu;
  }

  // ViewerSaveControlsMixin implementation.
  override shouldShowSaveMenuOnSaveClick(hasEdits: boolean): boolean {
    return hasEdits && this.state === SaveToDriveState.UNINITIALIZED;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-save-to-drive-controls': ViewerSaveToDriveControlsElement;
  }
}

customElements.define(
    ViewerSaveToDriveControlsElement.is, ViewerSaveToDriveControlsElement);
