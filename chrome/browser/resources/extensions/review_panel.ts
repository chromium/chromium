// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';
import './shared_style.css.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {DomRepeatEvent} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ExtensionsHatsBrowserProxyImpl} from './extension_hats_browser_proxy.js';
import type {ItemDelegate} from './item.js';
import {convertSafetyCheckReason, SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME, SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME, SAFETY_HUB_EXTENSION_SHOWN_HISTOGRAM_NAME, SAFETY_HUB_WARNING_REASON_MAX_SIZE} from './item_util.js';
import {getTemplate} from './review_panel.html.js';

export interface ExtensionsReviewPanelElement {
  $: {
    makeExceptionMenu: CrActionMenuElement,
    reviewPanelContainer: HTMLDivElement,
    expandButton: CrExpandButtonElement,
    safetyHubTitleContainer: HTMLElement,
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

      /**
       * List of potentially unsafe extensions. If this list is empty, all the
       * unsafe extensions were reviewed and the completion info should be
       * visible.
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
       * The text of the safety check completion state.
       */
      completionMessage_: String,

      /**
       * Indicates whether to show the Remove All button.
       */
      shouldShowSafetyHubRemoveAllButton_: {
        type: Boolean,
        computed: 'computeShouldShowSafetyHubRemoveAllButton_(extensions.*)',
      },

      /**
       * Indicates whether to show the potentially unsafe extensions or not.
       */
      shouldShowUnsafeExtensions_: {
        type: Boolean,
        computed: 'computeShouldShowUnsafeExtensions_(extensions.*)',
      },

      /**
       * Indicates whether to show completion info after user has finished the
       * review process.
       */
      shouldShowCompletionInfo_: {
        type: Boolean,
        computed:
            'computeShouldShowCompletionInfo_(extensions.*, reviewPanelShown_)',
      },

      /**
       * Indicates whether to show the panel header.
       */
      shouldShowSafetyHubHeader_: {
        type: Boolean,
        computed: 'computeShouldShowSafetyHubHeader_(extensions.*)',
      },

      /**
       * Indicates if the list of unsafe extensions is expanded or collapsed.
       */
      unsafeExtensionsReviewListExpanded_: {
        type: Boolean,
        value: true,
      },

      /**
       * Tracks if the last action that led to the number of extensions
       * under review going to 0 was taken in the review panel. If it was
       * the completion state is shown. If not the review panel is removed.
       * This prevents actions like toggling dev mode or removing a
       * extension using the item card's Remove button from triggering the
       * completion message.
       */
      numberOfExtensionsChangedByLastReviewPanelAction_: {
        type: Number,
        value: 0,
      },

      /**
       * Indicates if the review panel has ever been shown.
       */
      reviewPanelShown_: {
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

  delegate: ItemDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[];
  private numberOfExtensionsChangedByLastReviewPanelAction_: number;
  private reviewPanelShown_: boolean;
  private completionMetricLogged_: boolean;
  private headerString_: string;
  private subtitleString_: string;
  private unsafeExtensionsReviewListExpanded_: boolean;
  private completionMessage_: string;
  private shouldShowSafetyHubHeader_: boolean;
  private shouldShowSafetyHubRemoveAllButton_: boolean;
  private shouldShowCompletionInfo_: boolean;
  private shouldShowUnsafeExtensions_: boolean;
  private lastClickedExtensionId_: string;
  private lastClickedExtensionTriggerReason_:
      chrome.developerPrivate.SafetyCheckWarningReason;

  private async onExtensionsChanged_() {
    this.headerString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckTitle', this.extensions.length);
    this.subtitleString_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckDescription', this.extensions.length);
    this.completionMessage_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'safetyCheckAllDoneForNow',
            this.numberOfExtensionsChangedByLastReviewPanelAction_);
  }

  /**
   * Determines whether or not to show the completion info when there are no
   * unsafe extensions left.
   */
  private computeShouldShowCompletionInfo_(): boolean {
    if (this.extensions?.length === 0 &&
        this.numberOfExtensionsChangedByLastReviewPanelAction_ !== 0) {
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
    if (this.extensions?.length !== 0) {
      if (!this.shouldShowUnsafeExtensions_) {
        chrome.metricsPrivate.recordUserAction('SafetyCheck.ReviewPanelShown');
        for (const extension of this.extensions) {
          chrome.metricsPrivate.recordEnumerationValue(
              SAFETY_HUB_EXTENSION_SHOWN_HISTOGRAM_NAME,
              convertSafetyCheckReason(extension.safetyCheckWarningReason),
              SAFETY_HUB_WARNING_REASON_MAX_SIZE);
        }
      }
      this.completionMetricLogged_ = false;
      // Reset the `numberOfExtensionsChangedByLastReviewPanelAction_` if
      // the last action completed the review, i.e., a completion message
      // will be shown. Resetting ensures that the completion message is
      // only shown once after a review panel action.
      if (this.shouldShowCompletionInfo_) {
        this.numberOfExtensionsChangedByLastReviewPanelAction_ = 0;
      }
      ExtensionsHatsBrowserProxyImpl.getInstance().panelShown(true);
      return true;
    } else {
      ExtensionsHatsBrowserProxyImpl.getInstance().panelShown(false);
      return false;
    }
  }

  private computeShouldShowSafetyHubHeader_(): boolean {
    return loadTimeData.getBoolean('safetyHubShowReviewPanel') &&
        (this.shouldShowUnsafeExtensions_ || this.shouldShowCompletionInfo_);
  }

  private computeShouldShowSafetyHubRemoveAllButton_(): boolean {
    return this.extensions?.length !== 1;
  }

  /**
   * Opens the extension action menu.
   */
  private onMakeExceptionMenuClick_(
      e: DomRepeatEvent<chrome.developerPrivate.ExtensionInfo>) {
    this.lastClickedExtensionId_ = e.model.item.id;
    this.lastClickedExtensionTriggerReason_ =
        e.model.item.safetyCheckWarningReason;
    this.$.makeExceptionMenu.showAt(e.target as HTMLElement);
  }

  /**
   * Acknowledges the extension safety check warning.
   */
  private onKeepExtensionClick_() {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelKeepClicked');
    chrome.metricsPrivate.recordEnumerationValue(
        SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME,
        convertSafetyCheckReason(this.lastClickedExtensionTriggerReason_),
        SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    ExtensionsHatsBrowserProxyImpl.getInstance().extensionKeptAction();
    if (this.extensions?.length === 1) {
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 1;
    }
    this.$.makeExceptionMenu.close();
    if (this.lastClickedExtensionId_) {
      this.delegate.setItemSafetyCheckWarningAcknowledged(
          this.lastClickedExtensionId_,
          this.lastClickedExtensionTriggerReason_);
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
    chrome.metricsPrivate.recordEnumerationValue(
        SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME,
        convertSafetyCheckReason(e.model.item.safetyCheckWarningReason),
        SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    ExtensionsHatsBrowserProxyImpl.getInstance().extensionRemovedAction();
    if (this.extensions?.length === 1) {
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 1;
    }
    try {
      await this.delegate.uninstallItem(e.model.item.id);
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Update the number of changed extensions.
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 0;
    }
  }

  private async onRemoveAllClick_(event: Event): Promise<void> {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelRemoveAllClicked');
    ExtensionsHatsBrowserProxyImpl.getInstance().removeAllAction(
        this.extensions.length);
    event.stopPropagation();
    this.numberOfExtensionsChangedByLastReviewPanelAction_ =
        this.extensions.length;
    try {
      this.extensions.forEach(extension => {
        chrome.metricsPrivate.recordEnumerationValue(
            SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME,
            convertSafetyCheckReason(extension.safetyCheckWarningReason),
            SAFETY_HUB_WARNING_REASON_MAX_SIZE);
      });
      await this.delegate.deleteItems(
          this.extensions.map(extension => extension.id));
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Reset `numberOfExtensionsChangedByLastReviewPanelAction_`.
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 0;
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
