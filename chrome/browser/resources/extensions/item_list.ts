// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_footnote/managed_footnote.js';
import './item.js';
import './mv2_deprecation_panel.js';
import './review_panel.js';

import {getInstance as getAnnouncerInstance} from 'chrome://resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {DummyItemDelegate} from './item.js';
import type {ExtensionsItemElement, ItemDelegate} from './item.js';
import {getCss} from './item_list.css.js';
import {getHtml} from './item_list.html.js';
import {getMv2ExperimentStage, Mv2ExperimentStage} from './mv2_deprecation_util.js';

type Filter = (info: chrome.developerPrivate.ExtensionInfo) => boolean;

const ExtensionsItemListElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsItemListElement extends ExtensionsItemListElementBase {
  static get is() {
    return 'extensions-item-list';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      apps: {type: Array},
      extensions: {type: Array},
      delegate: {type: Object},

      inDevMode: {
        type: Boolean,
        reflect: true,
      },

      isMv2DeprecationNoticeDismissed: {
        type: Boolean,
        notify: true,
        reflect: true,
      },

      filter: {
        type: String,
      },

      computedFilter_: {type: String},
      maxColumns_: {type: Number},

      filteredExtensions_: {type: Array},
      filteredApps_: {type: Array},

      /**
       * List of potentially unsafe extensions that should be visible in the
       * review panel.
       */
      unsafeExtensions_: {type: Array},

      /**
       * Current Manifest V2 experiment stage.
       */
      mv2ExperimentStage_: {type: Number},

      /**
       * List of extensions that are affected by the mv2 deprecation and should
       * be visible in the mv2 deprecation panel.
       */
      mv2DeprecatedExtensions_: {type: Array},

      shownAppsCount_: {type: Number},
      shownExtensionsCount_: {type: Number},

      /**
       * Indicates whether the review panel is shown.
       */
      showSafetyCheckReviewPanel_: {type: Boolean},

      /**
       * Indicates if the review panel has ever been shown.
       */
      reviewPanelShown_: {
        type: Boolean,
        state: true,
      },
    };
  }

  apps: chrome.developerPrivate.ExtensionInfo[] = [];
  extensions: chrome.developerPrivate.ExtensionInfo[] = [];
  delegate: ItemDelegate = new DummyItemDelegate();
  inDevMode: boolean = false;
  isMv2DeprecationNoticeDismissed: boolean = false;
  filter: string = '';
  protected filteredExtensions_: chrome.developerPrivate.ExtensionInfo[] = [];
  protected filteredApps_: chrome.developerPrivate.ExtensionInfo[] = [];
  protected computedFilter_: Filter|null = null;
  protected maxColumns_: number = 3;
  protected unsafeExtensions_: chrome.developerPrivate.ExtensionInfo[] = [];
  protected mv2ExperimentStage_: Mv2ExperimentStage =
      getMv2ExperimentStage(loadTimeData.getInteger('MV2ExperimentStage'));
  protected mv2DeprecatedExtensions_: chrome.developerPrivate.ExtensionInfo[] =
      [];
  protected shownAppsCount_: number = 0;
  protected shownExtensionsCount_: number = 0;
  protected showSafetyCheckReviewPanel_: boolean = false;
  private reviewPanelShown_: boolean = false;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('filter')) {
      this.computedFilter_ = this.computeFilter_();
    }

    if (changedProperties.has('filter') ||
        changedProperties.has('extensions')) {
      this.filteredExtensions_ = this.computedFilter_ ?
          this.extensions.filter(
              extension => this.computedFilter_!(extension)) :
          this.extensions;
      this.shownExtensionsCount_ = this.filteredExtensions_.length;
    }

    if (changedProperties.has('filter') || changedProperties.has('apps')) {
      this.filteredApps_ = this.computedFilter_ ?
          this.apps.filter(app => this.computedFilter_!(app)) :
          this.apps;
      this.shownAppsCount_ = this.filteredApps_.length;
    }

    if (changedProperties.has('extensions')) {
      this.unsafeExtensions_ = this.computeUnsafeExtensions_();
      this.showSafetyCheckReviewPanel_ =
          this.computeShowSafetyCheckReviewPanel_();
      this.mv2DeprecatedExtensions_ = this.computeMv2DeprecatedExtensions_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('computedFilter_')) {
      this.announceSearchResults_();
    }
  }

  getDetailsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getDetailsButton();
  }

  getRemoveButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getRemoveButton();
  }

  getErrorsButton(id: string): HTMLElement|null {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    return item && item.getErrorsButton();
  }

  /**
   * Focus the remove button for the item matching `id`. If the remove button is
   * not visible, focus the details button instead.
   * return: If an item's button has been focused, see comment below.
   */
  focusItemButton(id: string): boolean {
    const item =
        this.shadowRoot!.querySelector<ExtensionsItemElement>(`#${id}`);
    // This function is called from a setTimeout() inside manager.ts. Rarely,
    // the list of extensions rendered in this element may not match the list of
    // extensions stored in manager.ts for a brief moment (not visible to the
    // user). As a result, `item` here may be null even though `id` points to
    // an extension inside `manager.ts`. If this happens, do not focus anything.
    // Observed in crbug.com/1482580.
    if (!item) {
      return false;
    }

    const buttonToFocus = item.getRemoveButton() || item.getDetailsButton();
    buttonToFocus!.focus();
    return true;
  }

  /**
   * Computes the filter function to be used for determining which items
   * should be shown. A |null| value indicates that everything should be
   * shown.
   */
  private computeFilter_(): Filter|null {
    const formattedFilter = this.filter.trim().toLowerCase();
    if (!formattedFilter) {
      return null;
    }

    return i => [i.name, i.id].some(
               s => s.toLowerCase().includes(formattedFilter));
  }

  /**
   * Computes the extensions that are affected by the manifest v2 deprecation
   * and should be visible in the MV2 deprecation panel.
   */
  private computeMv2DeprecatedExtensions_():
      chrome.developerPrivate.ExtensionInfo[] {
    return this.extensions.filter((extension) => {
      switch (this.mv2ExperimentStage_) {
        case Mv2ExperimentStage.NONE:
          return false;
        case Mv2ExperimentStage.WARNING:
          return extension.isAffectedByMV2Deprecation &&
              !extension.didAcknowledgeMV2DeprecationNotice;
        case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
          return extension.isAffectedByMV2Deprecation &&
              extension.disableReasons.unsupportedManifestVersion &&
              !extension.didAcknowledgeMV2DeprecationNotice;
        case Mv2ExperimentStage.UNSUPPORTED:
          return extension.isAffectedByMV2Deprecation &&
              extension.disableReasons.unsupportedManifestVersion;
      }
    });
  }

  /**
   * Computes the extensions that are potentially unsafe and should be visible
   * in the review panel.
   */
  private computeUnsafeExtensions_(): chrome.developerPrivate.ExtensionInfo[] {
    return this.extensions.filter(
        extension =>
            !!(extension.safetyCheckText &&
               extension.safetyCheckText.panelString));
  }

  /**
   * Returns whether the review deprecation panel should be visible.
   */
  private computeShowSafetyCheckReviewPanel_(): boolean {
    // Panel is hidden if neither safety feature is on.
    if (!loadTimeData.getBoolean('safetyCheckShowReviewPanel') &&
        !loadTimeData.getBoolean('safetyHubShowReviewPanel')) {
      return false;
    }

    // If there are any unsafe extensions, they will be shown in the panel.
    // Store this, so we can show the completion info in the panel when there
    // are no unsafe extensions left after the user finished reviewing the
    // extensions.
    if (this.unsafeExtensions_.length !== 0) {
      this.reviewPanelShown_ = true;
    }

    // Panel is visible if there are any unsafe extensions, or the there are
    // none left after the user finished reviewing the extensions.
    return this.unsafeExtensions_.length !== 0 || this.reviewPanelShown_;
  }


  /*
   * Indicates whether the mv2 deprecation panel is shown.
   */
  protected hasSafetyCheckTriggeringExtension_(): boolean {
    for (const extension of this.extensions) {
      if (!!extension.safetyCheckText &&
          !!extension.safetyCheckText.panelString &&
          this.showSafetyCheckReviewPanel_) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns whether the manifest v2 deprecation panel should be visible.
   */
  protected shouldShowMv2DeprecationPanel_(): boolean {
    switch (this.mv2ExperimentStage_) {
      case Mv2ExperimentStage.NONE:
        return false;
      case Mv2ExperimentStage.WARNING:
      case Mv2ExperimentStage.DISABLE_WITH_REENABLE:
      case Mv2ExperimentStage.UNSUPPORTED:
        // Panel is visible when it has not been dismissed and at least one
        // extension is affected by the MV2 deprecation.
        return !this.isMv2DeprecationNoticeDismissed &&
            this.mv2DeprecatedExtensions_?.length !== 0;
    }
  }

  protected shouldShowEmptyItemsMessage_(): boolean {
    return this.apps.length === 0 && this.extensions.length === 0;
  }

  protected shouldShowEmptySearchMessage_(): boolean {
    return !this.shouldShowEmptyItemsMessage_() && this.shownAppsCount_ === 0 &&
        this.shownExtensionsCount_ === 0;
  }

  protected onNoExtensionsClick_(e: Event) {
    if ((e.target as HTMLElement).tagName === 'A') {
      chrome.metricsPrivate.recordUserAction('Options_GetMoreExtensions');
    }
  }

  private announceSearchResults_() {
    if (this.computedFilter_) {
      setTimeout(() => {  // Async to allow list to update.
        const total = this.shownAppsCount_ + this.shownExtensionsCount_;
        getAnnouncerInstance().announce(this.shouldShowEmptySearchMessage_() ?
            this.i18n('noSearchResults') :
            (total === 1 ?
                 this.i18n('searchResultsSingular', this.filter) :
                 this.i18n(
                     'searchResultsPlural', total.toString(), this.filter)));
      }, 0);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-item-list': ExtensionsItemListElement;
  }
}

customElements.define(ExtensionsItemListElement.is, ExtensionsItemListElement);
