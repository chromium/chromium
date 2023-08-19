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
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ItemDelegate} from './item.js';
import {getTemplate} from './review_panel.html.js';

export interface ReviewItemDelegate {
  setItemSafetyCheckWarningAcknowledged(id: string): void;
  uninstallItem(id: string): Promise<void>;
}

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

const ExtensionsReviewPanelElementBase = I18nMixin(PolymerElement);

export class ExtensionsReviewPanelElement extends
    ExtensionsReviewPanelElementBase {
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
       * Indicates whether to show completion info after user has finished the
       * review process.
       */
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed:
            'computeShouldShowCompletionInfo_(extensions.*, hasChangeBeenMade_)',
      },

      /**
       * Indicates whether to show the potentially unsafe extensions or not.
       */
      shouldShowUnsafeExtensions_: {
        type: Boolean,
        computed: 'computeShouldShowUnsafeExtensions_(extensions.*)',
      },

      /**
       * Indicates if the list of unsafe extensions is expanded or collapsed.
       */
      unsafeExtensionsReviewListExpanded_: {
        type: Boolean,
        value: true,
      },

      /**
       * Indicates if any potential unsafe extensions has been kept or removed.
       */
      hasChangeBeenMade_: {
        type: Boolean,
        value: false,
      },

      /**
       * The latest id of an extension whose action menu (Keep the extension)
       * was expanded.
       * */
      lastClickedExtensionId_: String,
    };
  }

  static get observers() {
    return ['onExtensionsChanged_(extensions.*)'];
  }

  delegate: ItemDelegate&ReviewItemDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  private hasChangeBeenMade_: boolean;
  private completionMetricLogged_: boolean;
  private unsafeExtensions_: chrome.developerPrivate.ExtensionInfo[];
  private headerString_: string;
  private subtitleString_: string;
  private unsafeExtensionsReviewListExpanded_: boolean;
  private shouldShowCompletionInfo_: boolean;
  private shouldShowUnsafeExtensions_: boolean;
  private lastClickedExtensionId_: string;

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
    return extensions?.filter(
        extension =>
            !!(extension.safetyCheckText &&
               extension.safetyCheckText.panelString &&
               !extension.controlledInfo &&
               extension.acknowledgeSafetyCheckWarning !== true));
  }

  /**
   * Determines whether or not to show the completion info after the user
   * finished reviewing extensions.
   */
  private computeShouldShowCompletionInfo_(): boolean {
    const updatedUnsafeExtensions =
        this.getUnsafeExtensions_(this.extensions) || [];
    if (this.hasChangeBeenMade_ && updatedUnsafeExtensions.length === 0) {
      if (!this.completionMetricLogged_) {
        this.completionMetricLogged_ = true;
        chrome.metricsPrivate.recordUserAction('SafetyCheck.ReviewCompletion');
      }
      return true;
    } else {
      return false;
    }
  }

  private computeShouldShowUnsafeExtensions_(): boolean {
    const updatedUnsafeExtensions =
        this.getUnsafeExtensions_(this.extensions) || [];
    if (updatedUnsafeExtensions.length !== 0) {
      if (!this.shouldShowUnsafeExtensions_) {
        chrome.metricsPrivate.recordUserAction('SafetyCheck.ReviewPanelShown');
      }
      this.completionMetricLogged_ = false;
      return true;
    } else {
      return false;
    }
  }

  /**
   * Opens the extension action menu.
   */
  private onMakeExceptionMenuClick_(
      e: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>) {
    this.lastClickedExtensionId_ = e.model.item.id;
    this.$.makeExceptionMenu.showAt(e.target as HTMLElement);
  }

  /**
   * Acknowledges the extension safety check warning.
   */
  private onKeepExtensionClick_() {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelKeepClicked');
    this.$.makeExceptionMenu.close();
    if (this.lastClickedExtensionId_) {
      this.delegate.setItemSafetyCheckWarningAcknowledged(
          this.lastClickedExtensionId_);
      this.hasChangeBeenMade_ = true;
    }
  }

  private getRemoveButtonA11yLabel_(extensionName: string): string {
    return loadTimeData.substituteString(
        this.i18n('safetyCheckRemoveButtonA11yLabel'), extensionName);
  }

  private getOptionMenuA11yLabel_(extensionName: string) {
    return loadTimeData.substituteString(
        this.i18n('safetyCheckOptionMenuA11yLabel'), extensionName);
  }

  private async onRemoveExtensionClick_(
      e: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>): Promise<void> {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelRemoveClicked');
    try {
      await this.delegate.uninstallItem(e.model.item.id);
      this.hasChangeBeenMade_ = true;
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Do nothing.
    }
  }

  private async onRemoveAllClick_(): Promise<void> {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelRemoveAllClicked');
    try {
      await this.delegate.deleteItems(
          this.unsafeExtensions_.map(extension => extension.id));
      // If the Remove button was clicked and no errors were thrown, change
      // the flag.
      this.hasChangeBeenMade_ = true;
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Do nothing.
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-review-panel': ExtensionsReviewPanelElement;
  }
}

customElements.define(
    ExtensionsReviewPanelElement.is, ExtensionsReviewPanelElement);
