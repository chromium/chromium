// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog.js';
import './filter_dialog_footer.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {CommonUpdateOutcome} from '../../event_history.js';

import {getCss} from './outcome_dialog.css.js';
import {getHtml} from './outcome_dialog.html.js';

export class OutcomeDialogElement extends CrLitElement {
  static get is() {
    return 'outcome-dialog';
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
      pendingSelections: {type: Object},
      initialSelections: {type: Object},
    };
  }

  accessor anchorElement: HTMLElement|null = null;
  protected accessor pendingSelections = new Set<CommonUpdateOutcome>();
  accessor initialSelections = new Set<CommonUpdateOutcome>();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('initialSelections')) {
      this.pendingSelections = new Set(this.initialSelections);
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.shadowRoot.querySelector<HTMLElement>('.filter-menu-item')?.focus();
  }

  protected onCheckedChanged(e: Event) {
    const checkbox = e.target as HTMLInputElement;
    const outcome = checkbox.dataset['outcome'] as CommonUpdateOutcome;
    if (checkbox.checked) {
      this.pendingSelections.add(outcome);
    } else {
      this.pendingSelections.delete(outcome);
    }
    this.requestUpdate();
  }

  protected onApplyClick() {
    this.fire('filter-change', this.pendingSelections);
  }

  protected onCancelClick() {
    this.fire('close');
  }

  protected onClose() {
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'outcome-dialog': OutcomeDialogElement;
  }
}

customElements.define(OutcomeDialogElement.is, OutcomeDialogElement);
