// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loading subpage in Cellular Setup flow that shows an in progress operation or
 * an error. This element contains error image asset and loading animation.
 */
import './base_page.js';
import '//resources/ash/common/cr_elements/cr_hidden_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/cros_components/lottie_renderer/lottie-renderer.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './setup_loading_page.html.js';

export class SetupLoadingPageElement extends PolymerElement {
  static get is() {
    return 'setup-loading-page' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Message displayed with spinner when in LOADING state.
       */
      loadingMessage: {
        type: String,
        value: '',
      },

      /**
       * Title for page if needed.
       */
      loadingTitle: {
        type: String,
        value: '',
      },

      /**
       * Displays a sim detect error graphic if true.
       */
      isSimDetectError: {
        type: Boolean,
        value: false,
      },
    };
  }

  loadingMessage: string;
  loadingTitle: string|null;
  isSimDetectError: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    [SetupLoadingPageElement.is]: SetupLoadingPageElement;
  }
}

customElements.define(SetupLoadingPageElement.is, SetupLoadingPageElement);
