// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog.js';
import './filter_dialog_footer.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './date_dialog.css.js';
import {getHtml} from './date_dialog.html.js';

export class DateDialogElement extends CrLitElement {
  static get is() {
    return 'date-dialog';
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
      pendingStartDate: {type: Object},
      pendingEndDate: {type: Object},
      initialStartDate: {type: Object},
      initialEndDate: {type: Object},
    };
  }

  accessor anchorElement: HTMLElement|null = null;
  private accessor pendingStartDate: Date|null = null;
  private accessor pendingEndDate: Date|null = null;
  accessor initialStartDate: Date|null = null;
  accessor initialEndDate: Date|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('initialStartDate')) {
      this.pendingStartDate = this.initialStartDate;
    }
    if (changedProperties.has('initialEndDate')) {
      this.pendingEndDate = this.initialEndDate;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.shadowRoot
        .querySelector<HTMLElement>('.filter-menu-date-inputs input')
        ?.focus();
  }

  protected get pendingStartTime(): number {
    return this.pendingStartDate?.getTime() || NaN;
  }

  protected get pendingEndTime(): number {
    return this.pendingEndDate?.getTime() || NaN;
  }

  protected onStartTimeInput(e: Event) {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    this.pendingStartDate = Number.isNaN(value) ? null : new Date(value);
  }

  protected onEndTimeInput(e: Event) {
    const value = (e.target as HTMLInputElement).valueAsNumber;
    this.pendingEndDate = Number.isNaN(value) ? null : new Date(value);
  }

  protected onCancelClick() {
    this.fire('close');
  }

  protected onClose() {
    this.fire('close');
  }

  protected onDateApplyClick() {
    this.fire('filter-change', {
      start: this.pendingStartDate,
      end: this.pendingEndDate,
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'date-dialog': DateDialogElement;
  }
}

customElements.define(DateDialogElement.is, DateDialogElement);
