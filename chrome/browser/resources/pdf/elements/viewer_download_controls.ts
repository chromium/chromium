// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrIconButtonElement} from 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss as getPdfSharedCss} from './pdf_shared.css.js';
import {getHtml} from './viewer_download_controls.html.js';
import {ViewerSaveControlsMixin} from './viewer_save_controls_mixin.js';
import {getCss as getViewerSaveControlsSharedCss} from './viewer_save_controls_shared.css.js';

const ViewerDownloadControlsBase = ViewerSaveControlsMixin(CrLitElement);

export interface ViewerDownloadControlsElement {
  $: {
    save: CrIconButtonElement,
    menu: CrActionMenuElement,
  };
}

export class ViewerDownloadControlsElement extends ViewerDownloadControlsBase {
  static get is() {
    return 'viewer-download-controls';
  }

  static override get styles() {
    return [
      getPdfSharedCss(),
      getViewerSaveControlsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  override getSaveButton(): CrIconButtonElement {
    return this.$.save;
  }

  override getSaveEventType(): string {
    return 'save';
  }

  override getMenu(): CrActionMenuElement {
    return this.$.menu;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-download-controls': ViewerDownloadControlsElement;
  }
}

customElements.define(
    ViewerDownloadControlsElement.is, ViewerDownloadControlsElement);
