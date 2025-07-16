// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getPDFSharedCss} from './pdf_shared.css.js';
import {ViewerSaveControlsMixin} from './viewer_save_controls_mixin.js';
import {getCss as getViewerSaveControlsSharedCss} from './viewer_save_controls_shared.css.js';
import {getHtml} from './viewer_save_to_drive_controls.html.js';

const ViewerSaveControlsBase = ViewerSaveControlsMixin(CrLitElement);

export interface ViewerSaveToDriveControlsElement {
  $: {
    save: CrIconButtonElement,
    menu: CrActionMenuElement,
  };
}

export class ViewerSaveToDriveControlsElement extends ViewerSaveControlsBase {
  static get is() {
    return 'viewer-save-to-drive-controls';
  }

  static override get styles() {
    return [
      getPDFSharedCss(),
      getViewerSaveControlsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
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
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-save-to-drive-controls': ViewerSaveToDriveControlsElement;
  }
}

customElements.define(
    ViewerSaveToDriveControlsElement.is, ViewerSaveToDriveControlsElement);
