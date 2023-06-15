// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import './shared_style.css.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';
import {getTemplate} from './review_panel.html.js';

export interface ExtensionsReviewPanelElement {
  $: {
    makeExceptionMenu: CrActionMenuElement,
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
      delegate: Object,

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

  delegate: ItemDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  private unsafeExtensions_: chrome.developerPrivate.ExtensionInfo[]|null;
  private headerString_: string;
  private subtitleString_: string;
  private unsafeExtensionsReviewListExpanded_: boolean;

  private async onExtensionsChanged_() {
    this.unsafeExtensions_ = this.getUnsafeExtensions_(this.extensions);
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckTitle', this.unsafeExtensions_.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckDescription', this.unsafeExtensions_.length);
  }

  private getUnsafeExtensions_(extensions:
                                   chrome.developerPrivate.ExtensionInfo[]):
      chrome.developerPrivate.ExtensionInfo[] {
    // TODO(crbug.com/1432194): Update this filter criteria when new trigger
    // texts are added to getExtensionInfo API.
    return extensions.filter(
        extension => extension.disableReasons.corruptInstall ||
            extension.disableReasons.suspiciousInstall ||
            extension.runtimeWarnings.length || !!extension.blacklistText);
  }

  /**
   * Opens the extension action menu.
   */
  private onMakeExceptionMenuClick_(e: Event) {
    this.$.makeExceptionMenu.showAt(e.target as HTMLElement);
  }

  /**
   * Acknowledges the extension safety check warning.
   */
  private onKeepExtensionClick_() {
    this.$.makeExceptionMenu.close();
    // TODO(crbug.com/1432194): Call the private API to keep the extension in
    // pref.
  }

  private onRemoveExtensionClick_(
      e: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>): void {
    this.delegate.deleteItem(e.model.item.id);
  }

  private onRemoveAllExtensions_(): void {
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
