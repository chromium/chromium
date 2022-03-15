// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays toast notifications to the user.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {dismissErrorAction} from './personalization_actions.js';
import {WithPersonalizationStore} from './personalization_store.js';

export class PersonalizationToastElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-toast';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      error_: {
        type: String,
        value: null,
      },

      isLoading_: {
        type: Boolean,
      },

      showError_: {
        type: Boolean,
        computed: 'computeShowError_(error_, isLoading_)',
      },
    };
  }

  private error_: string;
  private isLoading_: boolean;
  private showError_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.watch('error_', state => state.error);
    this.watch(
        'isLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected ||
            state.wallpaper.loading.refreshWallpaper);
  }

  private onDismissClicked_() {
    this.dispatch(dismissErrorAction());
  }

  private computeShowError_(error: string|null, loading: boolean): boolean {
    return !!error && !loading;
  }
}

customElements.define(
    PersonalizationToastElement.is, PersonalizationToastElement);
