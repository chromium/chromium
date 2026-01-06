// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import type {ContextualUpload, TabUpload} from 'chrome://resources/cr_components/composebox/common.js';
import {ComposeboxMode} from 'chrome://resources/cr_components/composebox/contextual_entrypoint_and_carousel.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ActionChip, ActionChipsHandlerInterface, PageCallbackRouter, TabInfo} from '../action_chips.mojom-webui.js';
import {ChipType} from '../action_chips.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import {getCss} from './action_chips.css.js';
import {getHtml} from './action_chips.html.js';
import {ActionChipsApiProxyImpl} from './action_chips_proxy.js';

namespace ActionChipsConstants {
  export const EMPTY_QUERY_STRING = '';
}  // namespace

// Records a click metric for the given action chip type.
function recordClick(chipType: ChipType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Click', chipType, ChipType.MAX_VALUE + 1);
}

// Records a latency metric.
function recordLatency(metricName: string, latency: number) {
  chrome.metricsPrivate.recordTime(metricName, Math.round(latency));
}

/**
 * The enum value sent as part of action-chips-retrieval-state-changed.
 * The handler of the event should expect to receive UPDATED multiple times.
 */
export enum ActionChipsRetrievalState {
  // The initial state. This is not sent as part of the event and can be used as
  // the default value of a variable containing this enum.
  INITIAL,
  // The state used in an event firing when the first and only retrieval
  // request is sent from this component.
  REQUESTED,
  // The state used in events firing when the action chips are updated by a call
  // from the browser side.
  UPDATED,
}

const kActionChipsRetrievalStateChangedEvent =
    'action-chips-retrieval-state-changed';

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
      showSimplifiedUI_: {
        type: Boolean,
        reflect: true,
      },
      themeHasBackgroundImage: {type: Boolean, reflect: true},
    };
  }

  private handler: ActionChipsHandlerInterface;
  private callbackRouter: PageCallbackRouter;
  protected accessor actionChips_: ActionChip[] = [];
  accessor themeHasBackgroundImage: boolean = false;
  protected accessor showSimplifiedUI_: boolean =
      loadTimeData.getBoolean('ntpNextShowSimplificationUIEnabled');
  private onActionChipChangedListenerId_: number|null = null;
  private initialLoadStartTime_: number|null = null;

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
      case ChipType.kDeepDive:
        return 'deep-dive';
      default:
        return '';
    }
  }

  protected getId_(chip: ActionChip, index: number): string|null {
    switch (chip.type) {
      case ChipType.kImage:
        return 'nano-banana';
      case ChipType.kDeepSearch:
        return 'deep-search';
      case ChipType.kRecentTab:
        return 'tab-context';
      case ChipType.kDeepDive:
        return `deep-dive-${index}`;
      default:
        return null;
    }
  }

  constructor() {
    super();
    const proxy = ActionChipsApiProxyImpl.getInstance();
    this.handler = proxy.getHandler();
    this.callbackRouter = proxy.getCallbackRouter();
    this.initialLoadStartTime_ = WindowProxy.getInstance().now();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.onActionChipChangedListenerId_ =
        this.callbackRouter.onActionChipsChanged.addListener(
            (actionChips: ActionChip[]) => {
              this.actionChips_ = actionChips;
              this.fire(
                  kActionChipsRetrievalStateChangedEvent,
                  {state: ActionChipsRetrievalState.UPDATED});
            });
    this.handler.startActionChipsRetrieval();
    this.fire(
        kActionChipsRetrievalStateChangedEvent,
        {state: ActionChipsRetrievalState.REQUESTED});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.callbackRouter.removeListener(this.onActionChipChangedListenerId_!);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('themeHasBackgroundImage')) {
      this.updateBackgroundColor_();
    }

    // Records only the first load latency after rendering chips.
    if (this.initialLoadStartTime_ !== null && this.actionChips_.length > 0) {
      recordLatency(
          'NewTabPage.ActionChips.WebUI.InitialLoadLatency',
          WindowProxy.getInstance().now() - this.initialLoadStartTime_);
      this.initialLoadStartTime_ = null;
    }
  }

  protected onCreateImageClick_() {
    recordClick(ChipType.kImage);
    this.onActionChipClick_(
        ActionChipsConstants.EMPTY_QUERY_STRING, [],
        ComposeboxMode.CREATE_IMAGE);
  }

  protected onDeepDiveClick_(chip: ActionChip) {
    recordClick(ChipType.kDeepDive);
    const tab = chip.tab!;
    const deepDiveTabInfo: TabUpload = {
      tabId: tab.tabId,
      url: tab.url,
      title: tab.title,
      delayUpload: this.delayTabUploads_,
    };
    this.onActionChipClick_(
        chip.suggestion, [deepDiveTabInfo], ComposeboxMode.DEFAULT);
  }

  protected onDeepSearchClick_() {
    recordClick(ChipType.kDeepSearch);
    this.onActionChipClick_(
        ActionChipsConstants.EMPTY_QUERY_STRING, [],
        ComposeboxMode.DEEP_SEARCH);
  }

  protected onTabContextClick_(tab: TabInfo) {
    recordClick(ChipType.kRecentTab);
    const recentTabInfo: TabUpload = {
      tabId: tab.tabId,
      url: tab.url,
      title: tab.title,
      delayUpload: this.delayTabUploads_,
    };
    this.onActionChipClick_(
        ActionChipsConstants.EMPTY_QUERY_STRING, [recentTabInfo],
        ComposeboxMode.DEFAULT);
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
      case ChipType.kDeepDive:
        this.onDeepDiveClick_(chip);
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

  protected updateBackgroundColor_() {
    if (!this.showSimplifiedUI_) {
      return;
    }

    const simplifiedChipBgColor = this.themeHasBackgroundImage ?
        'var(--color-new-tab-page-action-chip-background)' :
        'transparent';

    this.style.setProperty(
        '--simplified-action-chip-bg', simplifiedChipBgColor);
  }

  private onActionChipClick_(
      query: string, contextFiles: ContextualUpload[], mode: ComposeboxMode) {
    this.fire('action-chip-click', {searchboxText: query, contextFiles, mode});
  }

  protected recentTabChipTitle_(chip: ActionChip) {
    if (!chip.tab) {
      return '';
    }
    const url = new URL(chip.tab.url.url);
    const domain = url.hostname.replace(/^www\./, '');
    return `${chip.title} - ${domain}`;
  }

  protected isDeepDiveChip_(chip: ActionChip) {
    return chip.type === ChipType.kDeepDive;
  }

  protected isRecentTabChip_(chip: ActionChip) {
    return chip.type === ChipType.kRecentTab;
  }

  protected showDashSimplifiedUI_(chip: ActionChip) {
    return chip.type !== ChipType.kDeepDive && this.showSimplifiedUI_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-action-chips': ActionChipsElement;
  }
}

customElements.define(ActionChipsElement.is, ActionChipsElement);
