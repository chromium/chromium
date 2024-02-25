// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This component displays toast notifications to the user.
 */

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';

import {dismissErrorAction} from './personalization_actions.js';
import {PersonalizationStateError} from './personalization_state.js';
import {WithPersonalizationStore} from './personalization_store.js';
import {getTemplate} from './personalization_toast_element.html.js';

export class PersonalizationToastElement extends WithPersonalizationStore {
  static get is() {
    return 'personalization-toast';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      error_: {
        type: Object,
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

  static get observers() {
    return ['onErrorOrShowErrorChanged_(error_, showError_)'];
  }

  private error_: PersonalizationStateError|null;
  private isLoading_: boolean;
  private showError_: boolean;
  private autoDismissTimeout_: number;

  override connectedCallback() {
    super.connectedCallback();
    this.watch('error_', state => state.error);
    this.watch(
        'isLoading_',
        state => state.wallpaper.loading.setImage > 0 ||
            state.wallpaper.loading.selected.attribution ||
            state.wallpaper.loading.selected.image ||
            state.wallpaper.loading.refreshWallpaper);
  }

  private onDismissClicked_() {
    this.dispatch(dismissErrorAction(/*id=*/ null, /*fromUser=*/ true));
  }

  private onErrorOrShowErrorChanged_(
      _: PersonalizationToastElement['error_'],
      showError: PersonalizationToastElement['showError_']) {
    clearTimeout(this.autoDismissTimeout_);
    if (showError) {
      this.autoDismissTimeout_ = setTimeout(() => {
        this.dispatch(dismissErrorAction(/*id=*/ null, /*fromUser=*/ false));
      }, 10000);
    }
  }

  private computeShowError_(
      error: PersonalizationToastElement['error_'],
      isLoading: PersonalizationToastElement['isLoading_']): boolean {
    return !!error && !isLoading;
  }

  private getErrorMessage_(error: PersonalizationToastElement['error_']): string
      |null {
    return error && error.message || null;
  }

  private getDismissMessage_(error: PersonalizationToastElement['error_']):
      string|null {
    return error && error.dismiss && error.dismiss.message ||
        this.i18n('dismiss');
  }
}

customElements.define(
    PersonalizationToastElement.is, PersonalizationToastElement);
