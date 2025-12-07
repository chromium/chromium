// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays the freeform subpage.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';
import 'chrome://resources/ash/common/personalization/personalization_shared_icons.html.js';
import 'chrome://resources/ash/common/personalization/wallpaper.css.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {IronA11yAnnouncer} from 'chrome://resources/polymer/v3_0/iron-a11y-announcer/iron-a11y-announcer.js';
import type {IronA11yKeysElement} from 'chrome://resources/polymer/v3_0/iron-a11y-keys/iron-a11y-keys.js';

import type {SeaPenSamplePrompt} from './constants.js';
import {FreeformTab, SEA_PEN_SAMPLES} from './constants.js';
import type {MantaStatusCode, SeaPenQuery, SeaPenThumbnail} from './sea_pen.mojom-webui.js';
import {getTemplate} from './sea_pen_freeform_element.html.js';
import {logSamplePromptShuffleClicked, logSeaPenFreeformTabClicked} from './sea_pen_metrics_logger.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {IronAnnounceEvent, isArrayEqual, isNonEmptyArray, shuffle} from './sea_pen_utils.js';

export interface SeaPenFreeformElement {
  $: {
    samplePromptsTab: HTMLElement,
    resultsTab: HTMLElement,
    tabContainer: HTMLElement,
    tabKeys: IronA11yKeysElement,
  };
}

export class SeaPenFreeformElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-freeform';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      freeformTab_: {
        type: String,
        value: FreeformTab.SAMPLE_PROMPTS,
      },

      seaPenQuery_: {
        type: Object,
        observer: 'onSeaPenQueryChanged_',
      },

      samples: {
        type: Array,
      },

      thumbnailResponseStatusCode_: {
        type: Object,
        observer: 'onThumbnailResponseStatusCodeChanged_',
      },

      thumbnails_: {
        type: Object,
      },
    };
  }

  samples: SeaPenSamplePrompt[];
  private freeformTab_: FreeformTab;
  private seaPenQuery_: SeaPenQuery|null;
  private thumbnailResponseStatusCode_: MantaStatusCode|null;
  private thumbnails_: SeaPenThumbnail[]|null;

  override ready() {
    super.ready();
    IronA11yAnnouncer.requestAvailability();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenFreeformElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.watch<SeaPenFreeformElement['thumbnailResponseStatusCode_']>(
        'thumbnailResponseStatusCode_',
        state => state.thumbnailResponseStatusCode);
    this.watch<SeaPenFreeformElement['thumbnails_']>(
        'thumbnails_', state => state.thumbnails);
    this.updateFromStore();
    this.shuffleSamplePrompts_();
    // The tab container is hidden when there are no results to show. In that
    // case, show the sample prompts tab.
    this.freeformTab_ =
        this.isTabContainerHidden_(
            this.seaPenQuery_, this.thumbnailResponseStatusCode_,
            this.thumbnails_) ?
        FreeformTab.SAMPLE_PROMPTS :
        FreeformTab.RESULTS;
    this.$.tabKeys.target = this.$.tabContainer;
  }

  /** Invoked on tab selected. */
  private onTabSelected_(e: Event) {
    const currentTarget: HTMLElement = e.currentTarget as HTMLElement;
    switch (currentTarget.id) {
      case 'samplePromptsTab':
        this.freeformTab_ = FreeformTab.SAMPLE_PROMPTS;
        break;
      case 'resultsTab':
        this.freeformTab_ = FreeformTab.RESULTS;
        break;
      default:
        assertNotReached();
    }
    logSeaPenFreeformTabClicked(this.freeformTab_);
  }

  /** Handle keyboard navigation. */
  private onTabKeysPressed_(e: CustomEvent) {
    const focusedElement = this.shadowRoot!.activeElement;
    // Remove focus state of focused tab.
    focusedElement?.removeAttribute('tabindex');

    const nextTab = this.getOtherTab_(focusedElement);
    if (nextTab) {
      // Add focus state for next tab.
      nextTab.setAttribute('tabindex', '0');
      nextTab.focus();
    }

    e.detail.keyboardEvent.preventDefault();
  }

  private onSeaPenQueryChanged_(query: SeaPenQuery|null) {
    // Update selected tab to Results tab once a freeform query search starts.
    // Otherwise, stay in Sample Prompts tab.
    this.freeformTab_ =
        query?.textQuery ? FreeformTab.RESULTS : FreeformTab.SAMPLE_PROMPTS;
  }

  private onThumbnailResponseStatusCodeChanged_(statusCode: MantaStatusCode|
                                                null): void {
    if (statusCode) {
      this.freeformTab_ = FreeformTab.RESULTS;
    }
  }

  private isTabContainerHidden_(
      query: SeaPenQuery|null,
      thumbnailResponseStatusCode: MantaStatusCode|null,
      thumbnails: SeaPenThumbnail[]|null): boolean {
    // The tab container should appear whenever the user has generated images.
    return !query?.textQuery && !thumbnailResponseStatusCode &&
        !isNonEmptyArray(thumbnails);
  }

  private isSamplePromptsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.SAMPLE_PROMPTS;
  }

  private isResultsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.RESULTS;
  }

  private getSamplePromptsTabIndex_(tab: FreeformTab): string {
    return this.isSamplePromptsTabSelected_(tab) ? '0' : '-1';
  }

  private getResultsTabIndex_(tab: FreeformTab): string {
    return this.isResultsTabSelected_(tab) ? '0' : '-1';
  }

  private getOtherTab_(element: Element|null): HTMLElement|null {
    if (element === this.$.resultsTab) {
      return this.$.samplePromptsTab;
    }
    if (element === this.$.samplePromptsTab) {
      return this.$.resultsTab;
    }
    return null;
  }

  private onShuffleClicked_(): void {
    logSamplePromptShuffleClicked();
    this.shuffleSamplePrompts_();
    this.dispatchEvent(
        new IronAnnounceEvent(this.i18n('ariaAnnounceSamplePromptsShuffled')));
  }

  private shuffleSamplePrompts_(): void {
    // Run shuffle (5 times at most) until the shuffled samples are
    // different from current, which is highly likely to happen the first time.
    for (let i = 0; i < 5; i++) {
      const newSamples = shuffle(SEA_PEN_SAMPLES).slice(0, 6);
      if (!this.samples || !isArrayEqual(newSamples, this.samples)) {
        this.samples = newSamples;
        break;
      }
    }
  }
}

customElements.define(SeaPenFreeformElement.is, SeaPenFreeformElement);
