// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {ContextualUpload, TabUpload} from 'chrome://resources/cr_components/composebox/common.js';
import {ComposeboxMode} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ActionChip, ActionChipsHandlerInterface, TabInfo} from '../action_chips.mojom-webui.js';
import {ChipType} from '../action_chips.mojom-webui.js';

import {getCss} from './action_chips.css.js';
import {getHtml} from './action_chips.html.js';
import {ActionChipsApiProxyImpl} from './action_chips_proxy.js';

/**
 * TODO: Move the enum to the Mojo model once it's created.
 * Elements on the Action Chips. This enum must match the numbering for
 * ActionChipsType in enums.xml. These values are persisted to logs. Entries
 * should not be renumbered, removed or reused.
 */

// LINT.IfChange(ActionChipsType)
export enum ActionChipsType {
  CREATE_IMAGE = 0,
  DEEP_SEARCH = 1,
  MAX_VALUE = DEEP_SEARCH,
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/new_tab_page/enums.xml:ActionChipsType)

// Records a click metric for the given action chip type.
function recordClick(element: ActionChipsType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Click', element, ActionChipsType.MAX_VALUE + 1);
}

// Records an impression metric for the given action chip type.
function recordShown(element: ActionChipsType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Shown', element, ActionChipsType.MAX_VALUE + 1);
}

/**
 * The element for displaying Action Chips.
 */
export class ActionChipsElement extends CrLitElement {
  static get is() {
    return 'ntp-action-chips';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      actionChips_: {type: Array, state: true},
    };
  }

  private handler: ActionChipsHandlerInterface;
  protected accessor actionChips_: ActionChip[] = [];

  private delayTabUploads_: boolean =
      loadTimeData.getBoolean('addTabUploadDelayOnActionChipClick');

  override render() {
    return getHtml.bind(this)();
  }


  protected getAdditionalIconClasses_(chip: ActionChip): string {
    switch (chip.type) {
      case ChipType.kImage:
        return 'banana';
      case ChipType.kDeepSearch:
        return 'deep-search';
      default:
        return '';
    }
  }

  protected getId(chip: ActionChip): string|null {
    switch (chip.type) {
      case ChipType.kImage:
        return 'nano-banana';
      case ChipType.kDeepSearch:
        return 'deep-search';
      case ChipType.kRecentTab:
        return 'tab-context';
      default:
        return null;
    }
  }

  constructor() {
    super();
    this.handler = ActionChipsApiProxyImpl.getInstance().getHandler();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.handler.getActionChips().then(
        (result: {actionChips: ActionChip[]}) => {
          this.actionChips_ = result.actionChips;
        });
  }

  // TODO (crbug.com/453650248): Move the impression metrics logging to the
  // handler logic once the changes are merged in. Right now we are only
  // statically showing these two chips.
  override firstUpdated() {
    recordShown(ActionChipsType.CREATE_IMAGE);
    recordShown(ActionChipsType.DEEP_SEARCH);
  }

  protected onCreateImageClick_() {
    recordClick(ActionChipsType.CREATE_IMAGE);
    this.onActionChipClick_(
        'Create an image ', [], ComposeboxMode.CREATE_IMAGE);
  }

  protected onDeepSearchClick_() {
    recordClick(ActionChipsType.DEEP_SEARCH);
    this.onActionChipClick_(
        'Help me research ', [], ComposeboxMode.DEEP_SEARCH);
  }

  protected onTabContextClick_(tab: TabInfo) {
    const recentTabInfo: TabUpload = {
      tabId: tab.tabId,
      url: tab.url,
      title: tab.title,
      delayUpload: this.delayTabUploads_,
    };
    this.onActionChipClick_('', [recentTabInfo], ComposeboxMode.DEFAULT);
  }

  protected handleClick_(chip: ActionChip): void {
    switch (chip.type) {
      case ChipType.kImage:
        this.onCreateImageClick_();
        break;
      case ChipType.kDeepSearch:
        this.onDeepSearchClick_();
        break;
      case ChipType.kRecentTab:
        this.onTabContextClick_(chip.tab!);
        break;
      default:
        // Do nothing yet...
    }
  }

  protected getFaviconUrl_(url: string): string {
    const faviconUrl = new URL('chrome://favicon2/');
    faviconUrl.searchParams.set('size', '24');
    faviconUrl.searchParams.set('scaleFactor', '1x');
    faviconUrl.searchParams.set('showFallbackMonogram', '');
    faviconUrl.searchParams.set('pageUrl', url);
    return faviconUrl.href;
  }

  protected getMostRecentTabFaviconUrl_(chip: ActionChip) {
    return chip.tab ? this.getFaviconUrl_(chip.tab.url.url) : '';
  }

  private onActionChipClick_(
      query: string, contextFiles: ContextualUpload[], mode: ComposeboxMode) {
    this.fire('action-chip-click', {searchboxText: query, contextFiles, mode});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-action-chips': ActionChipsElement;
  }
}

customElements.define(ActionChipsElement.is, ActionChipsElement);
