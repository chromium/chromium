// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import type {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {getCss} from './error_dialog.css.js';
import {getHtml} from './error_dialog.html.js';

export interface ContextualTasksErrorDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

export class ContextualTasksErrorDialogElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-error-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onReloadClick_() {
    window.location.reload();
    this.$.dialog.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-error-dialog': ContextualTasksErrorDialogElement;
  }
}

customElements.define(
    ContextualTasksErrorDialogElement.is, ContextualTasksErrorDialogElement);
