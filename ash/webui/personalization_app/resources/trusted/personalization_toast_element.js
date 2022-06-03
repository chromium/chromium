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

/** @polymer */
export class PersonalizationToastElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-toast';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * @type {?string}
       * @private
       */
      error_: {
        type: String,
        value: null,
      },

      /** @private */
      isLoading_: {
        type: Boolean,
      },

      /**
       * @private
       */
      showError_: {
        type: Boolean,
        computed: 'computeShowError_(error_, isLoading_)',
      },
    };
  }

  /** @override */
  connectedCallback() {
    super.connectedCallback();
    this.watch('error_', state => state.error);
    this.watch(
        'isLoading_',
        state => state.loading.setImage > 0 || state.loading.selected ||
            state.loading.refreshWallpaper);
  }

  /** @private */
  onDismissClicked_() {
    this.dispatch(dismissErrorAction());
  }

  /**
   * Show error when there is an error message and the app is not loading.
   * @param {?string} error
   * @param {boolean} loading
   * @return {boolean}
   * @private
   */
  computeShowError_(error, loading) {
    return !!error && !loading;
  }
}

customElements.define(
    PersonalizationToastElement.is, PersonalizationToastElement);
