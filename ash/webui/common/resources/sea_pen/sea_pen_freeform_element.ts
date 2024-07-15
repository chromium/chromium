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

import {SEA_PEN_SAMPLES, SeaPenSamplePrompt} from './constants.js';
import {SeaPenQuery} from './sea_pen.mojom-webui.js';
import {getTemplate} from './sea_pen_freeform_element.html.js';
import {WithSeaPenStore} from './sea_pen_store.js';
import {isArrayEqual, shuffle} from './sea_pen_utils.js';

/** Enumeration of supported tabs. */
export enum FreeformTab {
  SAMPLE_PROMPTS = 'sample_prompts',
  RESULTS = 'results',
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
        value: SEA_PEN_SAMPLES,
      },
    };
  }

  samples: SeaPenSamplePrompt[];
  private freeformTab_: FreeformTab;
  private seaPenQuery_: SeaPenQuery|null;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenFreeformElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.updateFromStore();
    this.onShuffleClicked_();
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
  }

  private onSeaPenQueryChanged_(query: SeaPenQuery|null) {
    // Update selected tab to Results tab once a freeform query search starts.
    // Otherwise, stay in Sample Prompts tab.
    this.freeformTab_ =
        query?.textQuery ? FreeformTab.RESULTS : FreeformTab.SAMPLE_PROMPTS;
  }

  private isTabContainerEnabled_(query: SeaPenQuery) {
    return !!query?.textQuery;
  }

  private isSamplePromptsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.SAMPLE_PROMPTS;
  }

  private isResultsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.RESULTS;
  }

  private onRecentFreeformImageDelete_() {
    // TODO(b/347328001): add the function implementation.
  }

  private onShuffleClicked_(): void {
    // Run shuffle (5 times at most) until the shuffled samples are
    // different from current, which is highly likely to happen the first time.
    for (let i = 0; i < 5; i++) {
      const newSamples = shuffle(this.samples);
      if (!isArrayEqual(newSamples, this.samples)) {
        this.samples = newSamples;
        break;
      }
    }
  }
}

customElements.define(SeaPenFreeformElement.is, SeaPenFreeformElement);
