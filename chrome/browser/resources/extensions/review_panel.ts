// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './shared_style.css.js';

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './review_panel.html.js';

export interface ExtensionsReviewPanelElement {
  $: {
    reviewPanelContainer: HTMLDivElement,
    expandButton: CrExpandButtonElement,
    headingText: HTMLElement,
    secondaryText: HTMLElement,
    removeAllButton: CrButtonElement,
  };
}

export class ExtensionsReviewPanelElement extends PolymerElement {
  static get is() {
    return 'extensions-review-panel';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * List of installed extensions.
       */
      extensions: {
        type: Array,
        notify: true,
      },

      /**
       * The string for the primary header label.
       */
      headerString_: String,

      /**
       * The string for secondary text under the header string.
       */
      subtitleString_: String,

      /**
       * List of potentially unsafe extensions. This list being empty
       * indicates that there are no unsafe extensions to review.
       */
      unsafeExtensions_: Array,

      /**
       * Indicates if the list of unsafe extensions is expanded or collapsed.
       */
      unsafeExtensionsReviewListExpanded_: {
        type: Boolean,
        value: true,
      },
    };
  }

  static get observers() {
    return ['onExtensionsChanged_(extensions.*)'];
  }

  extensions: chrome.developerPrivate.ExtensionInfo[];
  private unsafeExtensions_: chrome.developerPrivate.ExtensionInfo[]|null;
  private headerString_: string;
  private subtitleString_: string;
  private unsafeExtensionsReviewListExpanded_: boolean;

  private async onExtensionsChanged_() {
    this.unsafeExtensions_ = [];
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckTitle', this.unsafeExtensions_.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckDescription', this.unsafeExtensions_.length);
  }

  private onRemoveAllClick_() {
    // TODO(crbug.com/1432194): Call the private API to remove all extensions.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-review-panel': ExtensionsReviewPanelElement;
  }
}

customElements.define(
    ExtensionsReviewPanelElement.is, ExtensionsReviewPanelElement);
