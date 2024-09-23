// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays toast notifications to the user.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {dismissSeaPenErrorAction} from './sea_pen_actions.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {getTemplate} from './sea_pen_toast_element.html.js';

// Shows an error notification toast in SeaPen UI.
// TODO(b/333764915) merge with personalization toast.
export class SeaPenToastElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-toast';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      error_: String,

      isLoading_: Boolean,

      showError_: {
        type: Boolean,
        computed: 'computeShowError_(error_, isLoading_)',
      },
    };
  }

  private error_: string|null;
  private isLoading_: boolean;
  private showError_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch('error_', state => state.error);
    this.watch(
        'isLoading_',
        state => state.loading.setImage > 0 || state.loading.currentSelected);
  }

  private onDismissed_() {
    this.dispatch(dismissSeaPenErrorAction());
  }

  private computeShowError_(error: string|null, isLoading: boolean): boolean {
    return typeof error === 'string' && !isLoading;
  }
}

customElements.define(SeaPenToastElement.is, SeaPenToastElement);
