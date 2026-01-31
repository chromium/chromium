// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './filter_dialog.js';
import './filter_dialog_footer.js';
import '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import type {CrCheckboxElement} from '//resources/cr_elements/cr_checkbox/cr_checkbox.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {EVENT_TYPES} from '../../event_history.js';
import type {EventType} from '../../event_history.js';

import {getCss} from './event_dialog.css.js';
import {getHtml} from './event_dialog.html.js';

export class EventDialogElement extends CrLitElement {
  static get is() {
    return 'event-dialog';
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
  protected accessor pendingSelections = new Set<EventType>();
  accessor initialSelections = new Set<EventType>();

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

  get commonEventTypes(): EventType[] {
    return ['UPDATE', 'INSTALL', 'UNINSTALL'] as EventType[];
  }

  get otherEventTypes(): EventType[] {
    const common = this.commonEventTypes;
    return Object.values(EVENT_TYPES).filter(et => !common.includes(et));
  }

  protected onCheckedChanged(e: Event) {
    const checkbox = e.target as CrCheckboxElement;
    const eventType = checkbox.dataset['eventType'] as EventType;
    if (checkbox.checked) {
      this.pendingSelections.add(eventType);
    } else {
      this.pendingSelections.delete(eventType);
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
    'event-dialog': EventDialogElement;
  }
}

customElements.define(EventDialogElement.is, EventDialogElement);
