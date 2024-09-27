// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/icons_lit.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ItemDelegate} from './item.js';
import {convertSafetyCheckReason, SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME, SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME, SAFETY_HUB_EXTENSION_SHOWN_HISTOGRAM_NAME, SAFETY_HUB_WARNING_REASON_MAX_SIZE} from './item_util.js';
import {getCss} from './review_panel.css.js';
import {getHtml} from './review_panel.html.js';

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

const ExtensionsReviewPanelElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsReviewPanelElement extends
    ExtensionsReviewPanelElementBase {
  static get is() {
    return 'extensions-review-panel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      delegate: {type: Object},

      /**
       * List of potentially unsafe extensions. If this list is empty, all the
       * unsafe extensions were reviewed and the completion info should be
       * visible.
       */
      extensions: {type: Array},

      /**
       * The string for the primary header label.
       */
      headerString_: {type: String},

      /**
       * The string for secondary text under the header string.
       */
      subtitleString_: {type: String},

      /**
       * The text of the safety check completion state.
       */
      completionMessage_: {type: String},

      /**
       * Indicates whether to show the potentially unsafe extensions or not.
       */
      shouldShowUnsafeExtensions_: {type: Boolean},

      /**
       * Indicates whether to show completion info after user has finished the
       * review process.
       */
      shouldShowCompletionInfo_: {type: Boolean},

      /**
       * Indicates if the list of unsafe extensions is expanded or collapsed.
       */
      unsafeExtensionsReviewListExpanded_: {type: Boolean},
    };
  }

  delegate?: ItemDelegate;
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  protected headerString_: string = '';
  protected subtitleString_: string = '';
  protected unsafeExtensionsReviewListExpanded_: boolean = true;
  protected completionMessage_: string = '';
  protected shouldShowCompletionInfo_: boolean = false;
  protected shouldShowUnsafeExtensions_: boolean = false;

  /**
   * Tracks if the last action that led to the number of extensions
   * under review going to 0 was taken in the review panel. If it was
   * the completion state is shown. If not the review panel is removed.
   * This prevents actions like toggling dev mode or removing a
   * extension using the item card's Remove button from triggering the
   * completion message.
   */
  private numberOfExtensionsChangedByLastReviewPanelAction_: number = 0;
  private completionMetricLogged_: boolean = false;
  private lastClickedExtensionId_: string = '';
  private lastClickedExtensionTriggerReason_:
      chrome.developerPrivate.SafetyCheckWarningReason =
      chrome.developerPrivate.SafetyCheckWarningReason.UNPUBLISHED;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('extensions')) {
      this.shouldShowCompletionInfo_ = this.computeShouldShowCompletionInfo_();
      this.shouldShowUnsafeExtensions_ =
          this.computeShouldShowUnsafeExtensions_();
      this.onExtensionsChanged_();
    }
  }

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
      return true;
    }
    return false;
  }

  protected shouldShowExtensionsSafetyHub_(): boolean {
    return loadTimeData.getBoolean('safetyHubShowReviewPanel') &&
        (this.shouldShowUnsafeExtensions_ || this.shouldShowCompletionInfo_);
  }

  protected shouldShowSafetyHubRemoveAllButton_(): boolean {
    return this.extensions?.length !== 1;
  }

  protected onUnsafeExtensionsReviewListExpandedChanged_(
      e: CustomEvent<{value: boolean}>) {
    this.unsafeExtensionsReviewListExpanded_ = e.detail.value;
  }

  /**
   * Opens the extension action menu.
   */
  protected onMakeExceptionMenuClick_(e: Event) {
    const index = Number((e.target as HTMLElement).dataset['index']);
    const item = this.extensions[index]!;
    this.lastClickedExtensionId_ = item.id;
    this.lastClickedExtensionTriggerReason_ = item.safetyCheckWarningReason;
    this.$.makeExceptionMenu.showAt(e.target as HTMLElement);
  }

  /**
   * Acknowledges the extension safety check warning.
   */
  protected onKeepExtensionClick_() {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelKeepClicked');
    chrome.metricsPrivate.recordEnumerationValue(
        SAFETY_HUB_EXTENSION_KEPT_HISTOGRAM_NAME,
        convertSafetyCheckReason(this.lastClickedExtensionTriggerReason_),
        SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    if (this.extensions?.length === 1) {
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 1;
    }
    this.$.makeExceptionMenu.close();
    if (this.lastClickedExtensionId_) {
      assert(this.delegate);
      this.delegate.setItemSafetyCheckWarningAcknowledged(
          this.lastClickedExtensionId_,
          this.lastClickedExtensionTriggerReason_);
    }
  }

  protected getRemoveButtonA11yLabel_(extensionName: string): string {
    return loadTimeData.substituteString(
        this.i18n('safetyCheckRemoveButtonA11yLabel'), extensionName);
  }

  protected getOptionMenuA11yLabel_(extensionName: string) {
    return loadTimeData.substituteString(
        this.i18n('safetyCheckOptionMenuA11yLabel'), extensionName);
  }

  protected async onRemoveExtensionClick_(e: Event): Promise<void> {
    const index = Number((e.target as HTMLElement).dataset['index']);
    const item = this.extensions[index]!;
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelRemoveClicked');
    chrome.metricsPrivate.recordEnumerationValue(
        SAFETY_HUB_EXTENSION_REMOVED_HISTOGRAM_NAME,
        convertSafetyCheckReason(item.safetyCheckWarningReason),
        SAFETY_HUB_WARNING_REASON_MAX_SIZE);
    if (this.extensions?.length === 1) {
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 1;
    }
    try {
      assert(this.delegate);
      await this.delegate.uninstallItem(item.id);
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Update the number of changed extensions.
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 0;
    }
  }

  protected async onRemoveAllClick_(event: Event): Promise<void> {
    chrome.metricsPrivate.recordUserAction(
        'SafetyCheck.ReviewPanelRemoveAllClicked');
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
      assert(this.delegate);
      await this.delegate.deleteItems(
          this.extensions.map(extension => extension.id));
    } catch (_) {
      // The error was almost certainly the user canceling the dialog.
      // Reset `numberOfExtensionsChangedByLastReviewPanelAction_`.
      this.numberOfExtensionsChangedByLastReviewPanelAction_ = 0;
    }
  }
}

// Exported to be used in the autogenerated Lit template file
export type ReviewPanelElement = ExtensionsReviewPanelElement;

declare global {
  interface HTMLElementTagNameMap {
    'extensions-review-panel': ExtensionsReviewPanelElement;
  }
}

customElements.define(
    ExtensionsReviewPanelElement.is, ExtensionsReviewPanelElement);
