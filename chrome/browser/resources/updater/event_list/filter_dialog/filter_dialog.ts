// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './filter_dialog.css.js';
import {getHtml} from './filter_dialog.html.js';

export interface FilterDialogElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class FilterDialogElement extends CrLitElement {
  static get is() {
    return 'filter-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      anchorElement: {type: Object},
    };
  }

  private eventTracker: EventTracker = new EventTracker();
  accessor anchorElement: HTMLElement|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker.add(window, 'scroll', () => {
      this.positionDialog();
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker.removeAll();
  }

  override firstUpdated() {
    this.$.dialog.showModal();

    // firstUpdated guarantees that this element is in the DOM but does not
    // guarantee that the browser has completed a full layout pass for its
    // children. Because positionDialog is sensitive to the height of the
    // dialog, ensure that a paint has occurred before attempting to position
    // the dialog.
    requestAnimationFrame(() => {
      this.positionDialog();
    });
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (changedProperties.has('anchorElement')) {
      requestAnimationFrame(() => {
        this.positionDialog();
      });
    }
  }

  private positionDialog() {
    const anchor = this.anchorElement || this.parentElement;
    if (!anchor) {
      return;
    }

    const rect = anchor.getBoundingClientRect();
    const dialog = this.$.dialog;

    dialog.style.top = `${rect.bottom + 4}px`;
    dialog.style.left = `${rect.left}px`;

    // Ensure the dialog is not cut off by the window edges
    const dialogRect = dialog.getBoundingClientRect();
    if (dialogRect.right > window.innerWidth) {
      dialog.style.left = `${window.innerWidth - dialogRect.width - 4}px`;
    }
    if (dialogRect.bottom > window.innerHeight) {
      dialog.style.top = `${rect.top - dialogRect.height - 4}px`;
    }
  }

  protected onClose() {
    this.fire('close');
  }

  protected onCancel(e: Event) {
    e.preventDefault();
    this.$.dialog.close();
  }

  protected onPointerDown(e: PointerEvent) {
    if (e.target === this.$.dialog) {
      this.$.dialog.close();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'filter-dialog': FilterDialogElement;
  }
}

customElements.define(FilterDialogElement.is, FilterDialogElement);
