// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A polymer element that displays all the sample prompts for
 * freeform query.
 */

import 'chrome://resources/ash/common/personalization/common.css.js';
import 'chrome://resources/ash/common/personalization/cros_button_style.css.js';

import {assertNotReached} from 'chrome://resources/js/assert.js';

import {FreeformTab} from './constants.js';
import {SeaPenQuery} from './sea_pen.mojom-webui.js';
import {getTemplate} from './sea_pen_freeform_tabs_element.html.js';
import {WithSeaPenStore} from './sea_pen_store.js';

export class SeaPenFreeformTabsElement extends WithSeaPenStore {
  static get is() {
    return 'sea-pen-freeform-tabs';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      selectedTab: {
        type: FreeformTab,
        notify: true,
      },

      seaPenQuery_: {
        type: Object,
        observer: 'onSeaPenQueryChanged_',
      },
    };
  }

  selectedTab: FreeformTab;
  private seaPenQuery_: SeaPenQuery|null;

  override connectedCallback() {
    super.connectedCallback();
    this.watch<SeaPenFreeformTabsElement['seaPenQuery_']>(
        'seaPenQuery_', state => state.currentSeaPenQuery);
    this.updateFromStore();
  }

  /** Invoked on tab selected. */
  private onTabSelected_(e: Event) {
    const currentTarget: HTMLElement = e.currentTarget as HTMLElement;
    switch (currentTarget.id) {
      case 'samplePromptsTab':
        this.selectedTab = FreeformTab.SAMPLE_PROMPTS;
        break;
      case 'resultsTab':
        this.selectedTab = FreeformTab.RESULTS;
        break;
      default:
        assertNotReached();
    }
  }

  private onSeaPenQueryChanged_(query: SeaPenQuery|null) {
    // Update selected tab to Results tab once a freeform query search starts.
    // Otherwise, stay in Sample Prompts tab.
    this.selectedTab =
        query?.textQuery ? FreeformTab.RESULTS : FreeformTab.SAMPLE_PROMPTS;
  }

  private isSamplePromptsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.SAMPLE_PROMPTS;
  }

  private isResultsTabSelected_(tab: FreeformTab): boolean {
    return tab === FreeformTab.RESULTS;
  }
}

customElements.define(SeaPenFreeformTabsElement.is, SeaPenFreeformTabsElement);
