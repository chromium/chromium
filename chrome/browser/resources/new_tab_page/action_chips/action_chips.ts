// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {TabUpload} from 'chrome://resources/cr_components/composebox/common.js';
import {TabUploadOrigin} from 'chrome://resources/cr_components/composebox/common.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {ToolMode} from 'chrome://resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import type {ActionChip, ActionChipsHandlerInterface, PageCallbackRouter} from '../action_chips.mojom-webui.js';
import {IconType} from '../action_chips.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import {getCss} from './action_chips.css.js';
import {getHtml} from './action_chips.html.js';
import {ActionChipsApiProxyImpl} from './action_chips_proxy.js';

// Records a click metric for the given action chip icon type.
function recordClick(iconType: IconType) {
  chrome.metricsPrivate.recordEnumerationValue(
      'NewTabPage.ActionChips.Click2', iconType, IconType.MAX_VALUE + 1);
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

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      reducedMotionPreferred: {
        type: Boolean,
        reflect: true,
      },
      showBackground: {type: Boolean, reflect: true},
      actionChips_: {type: Array, state: true},
      showDismissalUI_: {
        type: Boolean,
        reflect: true,
      },
      showSimplifiedUI_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor reducedMotionPreferred: boolean = false;
  accessor showBackground: boolean = false;

  protected accessor actionChips_: ActionChip[] = [];
  protected accessor showDismissalUI_: boolean =
      loadTimeData.getBoolean('ntpNextShowDismissalUIEnabled');
  protected accessor showSimplifiedUI_: boolean =
      loadTimeData.getBoolean('ntpNextShowSimplificationUIEnabled');

  private callbackRouter: PageCallbackRouter;
  private delayTabUploads_: boolean =
      loadTimeData.getBoolean('addTabUploadDelayOnActionChipClick');
  private handler: ActionChipsHandlerInterface;
  private initialLoadStartTime_: number|null = null;
  private onActionChipChangedListenerId_: number|null = null;

  protected getAdditionalIconClasses_(chip: ActionChip): string {
    switch (chip.suggestTemplateInfo.typeIcon) {
      case IconType.kBanana:
        return 'icon-type-banana';
      case IconType.kGlobeWithSearchLoop:
        return 'icon-type-globe-with-search-loop';
      case IconType.kSubArrowRight:
        return 'icon-type-sub-arrow-right';
      default:
        return '';
    }
  }

  protected getId_(chip: ActionChip, index: number): string|null {
    switch (chip.suggestTemplateInfo.typeIcon) {
      case IconType.kBanana:
        return 'nano-banana';
      case IconType.kGlobeWithSearchLoop:
        return 'deep-search';
      case IconType.kFavicon:
        return 'tab-context';
      case IconType.kSubArrowRight:
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

    // Records only the first load latency after rendering chips.
    if (this.initialLoadStartTime_ !== null && this.actionChips_.length > 0) {
      recordLatency(
          'NewTabPage.ActionChips.WebUI.InitialLoadLatency',
          WindowProxy.getInstance().now() - this.initialLoadStartTime_);
      this.initialLoadStartTime_ = null;
    }
  }

  protected handleClick_(e: Event): void {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const chip = this.actionChips_[index]!;
    switch (chip.suggestTemplateInfo.typeIcon) {
      case IconType.kBanana:
        this.handler.activateMetricsFunnel('CreateImageChip');
        this.onActionChipClick_(chip, ToolMode.kImageGen);
        break;
      case IconType.kGlobeWithSearchLoop:
        this.handler.activateMetricsFunnel('DeepSearchChip');
        this.onActionChipClick_(chip, ToolMode.kDeepSearch);
        break;
      case IconType.kFavicon:
        this.handler.activateMetricsFunnel('RecentTabChip');
        this.onActionChipClick_(chip, ToolMode.kUnspecified);
        break;
      case IconType.kSubArrowRight:
        this.handler.activateMetricsFunnel('DeepDiveChip');
        this.onActionChipClick_(chip, ToolMode.kUnspecified);
        break;
      default:
        // Do nothing yet...
    }
  }

  protected removeChip_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const chip = this.actionChips_[index]!;
    this.actionChips_ =
        this.actionChips_.filter((c) => c.suggestion !== chip.suggestion);
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
    return chip.tab ? this.getFaviconUrl_(chip.tab.url) : '';
  }

  private onActionChipClick_(chip: ActionChip, mode: ToolMode) {
    recordClick(chip.suggestTemplateInfo.typeIcon);
    const contextFiles: TabUpload[] = [];
    const tab = chip.tab;
    if (tab) {
      const tabInfo: TabUpload = {
        tabId: tab.tabId,
        url: tab.url,
        title: tab.title,
        delayUpload: this.delayTabUploads_,
        origin: TabUploadOrigin.ACTION_CHIP,
      };
      contextFiles.push(tabInfo);
    }
    this.fire(
        'action-chip-click',
        {searchboxText: chip.suggestion, contextFiles, mode});
  }

  protected recentTabChipTitle_(chip: ActionChip) {
    if (!chip.tab) {
      return '';
    }
    const url = new URL(chip.tab.url);
    const domain = url.hostname.replace(/^www\./, '');
    return `${chip.suggestTemplateInfo.secondaryText?.text ?? ''} - ${domain}`;
  }

  protected isDeepDiveChip_(chip: ActionChip) {
    return chip.suggestTemplateInfo.typeIcon === IconType.kSubArrowRight;
  }

  protected isRecentTabChip_(chip: ActionChip) {
    return chip.suggestTemplateInfo.typeIcon === IconType.kFavicon;
  }

  protected showDashSimplifiedUI_(chip: ActionChip) {
    return chip.suggestTemplateInfo.typeIcon !== IconType.kSubArrowRight &&
        this.showSimplifiedUI_;
  }

  protected getChipSubtitle_(chip: ActionChip): string {
    const subtitle = (this.showSimplifiedUI_ && chip.suggestion) ?
        chip.suggestion :
        (chip.suggestTemplateInfo.secondaryText?.text ?? '');
    const prefix = (subtitle && this.showDashSimplifiedUI_(chip)) ? ' - ' : '';
    return `${prefix}${subtitle}`;
  }

  protected getChipTitle_(chip: ActionChip) {
    const suggestion = chip.suggestion;

    if (!chip.tab) {
      return suggestion;
    }

    const tabTitle = chip.tab.title;
    const url = new URL(chip.tab.url);
    const domain = url.hostname.replace(/^www\./, '');

    if (this.isRecentTabChip_(chip)) {
      return `${tabTitle}\n${domain}`;
    }

    if (this.isDeepDiveChip_(chip)) {
      return `${suggestion}\n${domain}`;
    }

    return suggestion;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-action-chips': ActionChipsElement;
  }
}

customElements.define(ActionChipsElement.is, ActionChipsElement);
