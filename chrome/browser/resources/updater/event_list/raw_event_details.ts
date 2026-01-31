// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_collapse/cr_collapse.js';
import '//resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';

import {getCss} from './raw_event_details.css.js';
import {getHtml} from './raw_event_details.html.js';

export class RawEventDetailsElement extends CrLitElement {
  static get is() {
    return 'raw-event-details';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      label: {type: String, reflect: true},
      events: {type: Array},
      expanded: {type: Boolean},
    };
  }

  accessor label: string = loadTimeData.getString('viewRawDetails');
  accessor events: Array<Record<string, unknown>> = [];
  protected accessor expanded: boolean = false;

  protected onExpandedChanged(e: CustomEvent<{value: boolean}>) {
    this.expanded = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'raw-event-details': RawEventDetailsElement;
  }
}

customElements.define(RawEventDetailsElement.is, RawEventDetailsElement);
