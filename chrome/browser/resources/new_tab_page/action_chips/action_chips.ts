// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://resources/cr_components/composebox/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import './icons.html.js';

import {ToolMode} from '//resources/cr_components/composebox/composebox_query.mojom-webui.js';
import type {TabUpload} from 'chrome://resources/cr_components/composebox/common.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {getFaviconUrl} from 'chrome://resources/js/icon.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {FuseboxAction} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {QueryActionOverride} from 'chrome://resources/mojo/components/omnibox/browser/fusebox_action.mojom-webui.js';
import {TabAttachmentSource} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {IconType} from 'chrome://resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';

import type {ActionChip, ActionChipsHandlerInterface, PageCallbackRouter} from '../action_chips.mojom-webui.js';
import {WindowProxy} from '../window_proxy.js';

import {getCss} from './action_chips.css.js';
import {getHtml} from './action_chips.html.js';
import {ActionChipsApiProxyImpl} from './action_chips_proxy.js';

export interface ActionChipClickDetail {
  suggestion: string;
  files: TabUpload[];
  fuseboxAction?: FuseboxAction;
}

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

export interface ActionChipsElement {
  $: {
    actionMenu: CrActionMenuElement,
  };
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

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showBackground: {type: Boolean, reflect: true},
      actionChips_: {type: Array, state: true},
      showDismissalUI_: {
        type: Boolean,
        reflect: true,
      },
      disablementContextMenuEnabled_: {
        type: Boolean,
        reflect: true,
      },
      smallChipsEnabled_: {
        type: Boolean,
        reflect: true,
        attribute: 'small-chips-enabled',
      },
    };
  }

  accessor showBackground: boolean = false;

  protected accessor actionChips_: ActionChip[] = [];
  protected accessor showDismissalUI_: boolean =
      loadTimeData.getBoolean('ntpNextShowDismissalUIEnabled');
  protected accessor disablementContextMenuEnabled_: boolean =
      loadTimeData.getBoolean('ntpNextDisablementContextMenuEnabled');
  protected accessor smallChipsEnabled_: boolean =
      loadTimeData.getBoolean('ntpSmallActionChipsEnabled');
  private webuiRoundedIconsEnabled_: boolean =
      loadTimeData.getBoolean('webuiRoundedIconsEnabled');

  private callbackRouter: PageCallbackRouter;
  private delayTabUploads_: boolean =
      loadTimeData.getBoolean('addTabUploadDelayOnActionChipClick');
  private handler: ActionChipsHandlerInterface;
  private initialLoadStartTime_: number|null = null;
  private onActionChipChangedListenerId_: number|null = null;

  // Returns the <cr-icon> icon for the chip, or an empty string if the chip
  // has no icon or shows a favicon instead.
  protected getIcon_(chip: ActionChip): string {
    const oldSuffix = this.webuiRoundedIconsEnabled_ ? '' : '-old';
    switch (chip.suggestTemplateInfo.typeIcon) {
      case IconType.kBanana:
        // Same image as the Nano Banana tool icon in the composebox.
        return 'composebox:nanoBanana-custom';
      case IconType.kGlobeWithSearchLoop:
        return `ntp-action-chips:globe-with-search-loop${oldSuffix}`;
      case IconType.kSubArrowRight:
        return `ntp-action-chips:sub-arrow-right${oldSuffix}`;
      case IconType.kDraftSpark:
        return `ntp-action-chips:draft-spark${oldSuffix}`;
      case IconType.kSearchLoopWithSparkle:
        return 'ntp-action-chips:search-spark';
      case IconType.kLightbulb:
        return 'ntp-action-chips:lightbulb';
      case IconType.kAttachFile:
        return 'ntp-action-chips:attach-file';
      case IconType.kSchool:
        return 'ntp-action-chips:school';
      case IconType.kInkPen:
        return 'ntp-action-chips:ink-pen';
      case IconType.kTab:
        return 'ntp-action-chips:tab';
      case IconType.kPhotoSpark:
        return 'ntp-action-chips:photo-spark';
      case IconType.kBolt:
        return 'ntp-action-chips:bolt';
      case IconType.kImageCreate:
        return 'ntp-action-chips:image-create';
      default:
        return '';
    }
  }

  protected getAdditionalIconClasses_(chip: ActionChip): string {
    switch (chip.suggestTemplateInfo.typeIcon) {
      case IconType.kBanana:
        return 'icon-type-banana';
      case IconType.kGlobeWithSearchLoop:
        return 'icon-type-globe-with-search-loop';
      case IconType.kSubArrowRight:
        return 'icon-type-sub-arrow-right';
      case IconType.kDraftSpark:
        return 'icon-type-draft-spark';
      case IconType.kFavicon:
        return 'icon-type-favicon';
      case IconType.kSearchLoopWithSparkle:
        return 'icon-type-search-spark';
      case IconType.kLightbulb:
        return 'icon-type-lightbulb';
      case IconType.kAttachFile:
        return 'icon-type-attach-file';
      case IconType.kSchool:
        return 'icon-type-school';
      case IconType.kInkPen:
        return 'icon-type-ink-pen';
      case IconType.kTab:
        return 'icon-type-tab';
      case IconType.kPhotoSpark:
        return 'icon-type-photo-spark';
      case IconType.kBolt:
        return 'icon-type-bolt';
      case IconType.kImageCreate:
        return 'icon-type-image-create';
      default:
        return '';
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
              this.toggleAttribute('has-chips', actionChips.length > 0);
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

  protected onClick_(e: MouseEvent|KeyboardEvent): void {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const chip = this.actionChips_[index]!;
    switch (chip.suggestTemplateInfo.fuseboxAction?.preselectedTool) {
      case ToolMode.kImageGen:
        this.handler.activateMetricsFunnel('CreateImageChip');
        break;
      case ToolMode.kDeepSearch:
        this.handler.activateMetricsFunnel('DeepSearchChip');
        break;
      case ToolMode.kCanvas:
        this.handler.activateMetricsFunnel('CanvasChip');
        break;
      case ToolMode.kUnspecified:
        if (chip.suggestTemplateInfo.typeIcon === IconType.kFavicon) {
          this.handler.activateMetricsFunnel('RecentTabChip');
        } else if (
            chip.suggestTemplateInfo.typeIcon === IconType.kSubArrowRight) {
          this.handler.activateMetricsFunnel('DeepDiveChip');
        } else if (
            chip.suggestTemplateInfo.typeIcon ===
            IconType.kSearchLoopWithSparkle) {
          this.handler.activateMetricsFunnel('PromptSuggestionChip');
        }
        break;
      default:
        // Do nothing yet...
    }
    this.onActionChipClick_(chip, e);
  }

  // Auxclick fires for all non-primary mouse buttons. Only handle middle clicks
  // (button 1) to open in a new background tab, and ignore other buttons.
  protected onAuxclick_(e: MouseEvent): void {
    if (e.button === 1) {
      this.onClick_(e);
    }
  }

  protected onRemoveClick_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const chip = this.actionChips_[index]!;
    this.actionChips_ =
        this.actionChips_.filter((c) => c.suggestion !== chip.suggestion);
    this.toggleAttribute('has-chips', this.actionChips_.length > 0);
  }

  protected onContextmenu_(e: MouseEvent) {
    e.preventDefault();
    e.stopPropagation();
    this.$.actionMenu.showAt(e.target as HTMLElement);
  }

  protected onDisableSuggestionClick_() {
    this.$.actionMenu.close();
    this.handler.setActionChipsVisibility(false);
    this.fire('action-chips-disabled', {
      message: loadTimeData.getString('actionChipsUndoDisablementToastMessage'),
      undo: () => this.handler.setActionChipsVisibility(true),
    });
  }


  // Returns a chrome://favicon2 URL, or, if the element is hosted in an
  // extension page, the extension's /_favicon/ URL.
  protected getFaviconUrl_(url: string): string {
    const faviconUrl =
        new URL(getFaviconUrl(url, {size: 24, scaleFactor: '1x'}));
    faviconUrl.searchParams.set('showFallbackMonogram', '');
    return faviconUrl.href;
  }

  protected getMostRecentTabFaviconUrl_(chip: ActionChip) {
    return chip.tab ? this.getFaviconUrl_(chip.tab.url) : '';
  }

  private onActionChipClick_(chip: ActionChip, e: MouseEvent|KeyboardEvent) {
    recordClick(chip.suggestTemplateInfo.typeIcon);
    if (chip.suggestTemplateInfo.fuseboxAction?.queryActionOverride ===
        QueryActionOverride.kDefault) {
      this.handler.navigateToAim(
          chip.suggestion, (e as MouseEvent).button || 0, {
            altKey: e.altKey,
            ctrlKey: e.ctrlKey,
            metaKey: e.metaKey,
            shiftKey: e.shiftKey,
          });
      return;
    }
    const contextFiles: TabUpload[] = [];
    const tab = chip.tab;
    if (tab) {
      const tabInfo: TabUpload = {
        tabId: tab.tabId,
        url: tab.url,
        title: tab.title,
        delayUpload: this.delayTabUploads_,
        origin: TabAttachmentSource.kActionChip,
      };
      contextFiles.push(tabInfo);
    }
    this.fire<ActionChipClickDetail>('action-chip-click', {
      suggestion: chip.suggestion,
      files: contextFiles,
      fuseboxAction: chip.suggestTemplateInfo.fuseboxAction ?? undefined,
    });
  }

  protected recentTabChipTitle_(chip: ActionChip) {
    if (!chip.tab) {
      return '';
    }
    const url = new URL(chip.tab.url);
    const domain = url.hostname.replace(/^www\./, '');
    return `${chip.suggestTemplateInfo.secondaryText?.text ?? ''} - ${domain}`;
  }


  protected getChipSubtitle_(chip: ActionChip): string {
    return chip.suggestTemplateInfo.secondaryText?.text ?? '';
  }

  protected getChipTitle_(chip: ActionChip): string {
    const primaryText = chip.suggestTemplateInfo.primaryText;
    const secondaryText = chip.suggestTemplateInfo.secondaryText;

    const primary = primaryText.a11yText || primaryText.text;
    const secondary = secondaryText?.a11yText || secondaryText?.text || '';

    if (primary && secondary) {
      return `${primary} ${secondary}`;
    }
    return primary || secondary || '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-action-chips': ActionChipsElement;
  }
}

customElements.define(ActionChipsElement.is, ActionChipsElement);
