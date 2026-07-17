// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="_google_chrome">
import './internal/icons.html.js';
// </if>

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';

import type {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {IconTable} from '/shared/icon_table.js';
import {PinnedToolbarAction} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {ContextMenuType} from './browser_proxy.js';
import {getHtml} from './pinned_toolbar_action.html.js';
import {getCss} from './toolbar_button.css.js';
import {getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';

const PinnedToolbarActionElementBase = HelpBubbleAnchorMixin(CrLitElement);

export interface PinnedToolbarActionElement {
  $: {
    button: CrIconButtonElement,
  };
}

export class PinnedToolbarActionElement extends PinnedToolbarActionElementBase {
  static get is() {
    return 'pinned-toolbar-action';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      ...super.properties,
      state: {type: Object},
      trackedHighlighted: {type: Boolean},
    };
  }

  accessor state: PinnedToolbarActionState = {
    action: PinnedToolbarAction.kUnspecified,
    highlighted: false,
    enabled: true,
    activated: false,
    tooltip: '',
    accessibilityText: '',
    elementId: null,
    icon: {handleId: 0n},
  };

  private get browserProxy_(): BrowserProxy {
    return BrowserProxyImpl.getInstance();
  }
  private iconTable_: IconTable = IconTable.getInstance();
  private registerHelpBubbleController_: AbortController|null = null;

  protected accessor trackedHighlighted: boolean = false;

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.registerHelpBubbleController_) {
      this.registerHelpBubbleController_.abort();
      this.registerHelpBubbleController_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldId = oldState?.elementId;
      const newId = this.state.elementId;

      if (oldId !== newId) {
        if (this.registerHelpBubbleController_) {
          this.registerHelpBubbleController_.abort();
          this.registerHelpBubbleController_ = null;
        }
        if (oldId) {
          this.unregisterHelpBubble(oldId);
        }
        if (newId) {
          this.registerHelpBubble_(newId);
        }
      }
    }
  }

  private async registerHelpBubble_(newId: string) {
    this.registerHelpBubbleController_ = new AbortController();
    const signal = this.registerHelpBubbleController_.signal;

    const animations = this.getAnimations().filter(anim => {
      const timing = anim.effect?.getTiming();
      // Ignore infinite animations (e.g. pulsing for IPH).
      return timing?.iterations !== Infinity && timing?.duration !== Infinity;
    });

    // Wait for any animations to complete, so button is in final location.
    if (animations.length > 0) {
      try {
        await Promise.all(animations.map(a => a.finished));
      } catch (e) {
        // Ignore animation cancellation.
      }
    }

    if (!signal.aborted) {
      this.registerHelpBubble(newId, this, {
        onHighlightChanged: (highlighted: boolean) => {
          this.trackedHighlighted = highlighted;
        },
        onHelpBubbleShown: () => setHasHelpBubble(this, true),
        onHelpBubbleHidden: () => setHasHelpBubble(this, false),
      });
      this.registerHelpBubbleController_ = null;
    }
  }

  protected getIronIcon_(): string|undefined {
    return this.iconTable_.getIconName(this.state.icon);
  }

  protected getIconStyle_(): string|undefined {
    const providedIconUrl = this.iconTable_.getIconMaskUrl(this.state.icon);
    const providedIconColor = this.iconTable_.getIconColor(this.state.icon);
    let style = '';

    if (providedIconUrl) {
      style += `--cr-icon-image: url(${providedIconUrl});`;
    }
    if (providedIconColor) {
      style += `--cr-icon-button-fill-color: ${providedIconColor};`;
    }
    return style.length > 0 ? style : undefined;
  }

  protected onActionClick_() {
    this.browserProxy_.toolbarUIHandler.invokePinnedToolbarAction(
        this.state.action);
  }

  // Delegate focus to the internal button element. Custom elements do not
  // automatically delegate focus to their shadow DOM content unless configured
  // with delegatesFocus, which Lit doesn't do by default for wrapper elements.
  override focus() {
    this.$.button.focus();
  }

  protected onDragstart_(e: DragEvent) {
    if (!this.state.enabled || !e.dataTransfer) {
      e.preventDefault();
      return;
    }
    const payload = JSON.stringify({
      actionId: this.state.action,
    });
    e.dataTransfer.setData('application/x-webui-pinned-action', payload);
    e.dataTransfer.effectAllowed = 'move';

    this.fire('pinned-action-drag-start', {action: this.state.action});
  }

  protected onDragend_(e: DragEvent) {
    this.fire('pinned-action-drag-end', {
      action: this.state.action,
      dropEffect: e.dataTransfer?.dropEffect,
    });
  }


  protected onKeydown_(e: KeyboardEvent) {
    const isModifier = e.ctrlKey || e.metaKey;
    if (!isModifier || (e.key !== 'ArrowLeft' && e.key !== 'ArrowRight')) {
      return;
    }
    e.preventDefault();
    e.stopPropagation();
    const delta = e.key === 'ArrowLeft' ? -1 : 1;
    // Notify the parent container that a keyboard reorder is occurring.
    // The container will use this to lock and restore focus to this action
    // button after the DOM updates with the new order.
    this.fire('pinned-action-keyboard-reorder', {action: this.state.action});

    this.browserProxy_.toolbarUIHandler.movePinnedToolbarActionBy(
        this.state.action, delta);
  }

  private getContextMenuType_(): ContextMenuType {
    switch (this.state.action) {
      case PinnedToolbarAction.kNewIncognitoWindow:
        return ContextMenuType.kPinnedActionNewIncognitoWindow;
      case PinnedToolbarAction.kShowPasswordsBubbleOrPage:
        return ContextMenuType.kPinnedActionShowPasswordsBubbleOrPage;
      case PinnedToolbarAction.kShowPaymentsBubbleOrPage:
        return ContextMenuType.kPinnedActionShowPaymentsBubbleOrPage;
      case PinnedToolbarAction.kShowAddressesBubbleOrPage:
        return ContextMenuType.kPinnedActionShowAddressesBubbleOrPage;
      case PinnedToolbarAction.kSidePanelShowBookmarks:
        return ContextMenuType.kPinnedActionSidePanelShowBookmarks;
      case PinnedToolbarAction.kSidePanelShowReadingList:
        return ContextMenuType.kPinnedActionSidePanelShowReadingList;
      case PinnedToolbarAction.kSidePanelShowHistoryCluster:
        return ContextMenuType.kPinnedActionSidePanelShowHistoryCluster;
      case PinnedToolbarAction.kShowDownloads:
        return ContextMenuType.kPinnedActionShowDownloads;
      case PinnedToolbarAction.kClearBrowsingData:
        return ContextMenuType.kPinnedActionClearBrowsingData;
      case PinnedToolbarAction.kPrint:
        return ContextMenuType.kPinnedActionPrint;
      case PinnedToolbarAction.kSidePanelShowLensOverlayResults:
        return ContextMenuType.kPinnedActionSidePanelShowLensOverlayResults;
      case PinnedToolbarAction.kShowTranslate:
        return ContextMenuType.kPinnedActionShowTranslate;
      case PinnedToolbarAction.kQrCodeGenerator:
        return ContextMenuType.kPinnedActionQrCodeGenerator;
      case PinnedToolbarAction.kRouteMedia:
        return ContextMenuType.kPinnedActionRouteMedia;
      case PinnedToolbarAction.kSidePanelShowReadAnything:
        return ContextMenuType.kPinnedActionSidePanelShowReadAnything;
      case PinnedToolbarAction.kCopyUrl:
        return ContextMenuType.kPinnedActionCopyUrl;
      case PinnedToolbarAction.kSendTabToSelf:
        return ContextMenuType.kPinnedActionSendTabToSelf;
      case PinnedToolbarAction.kTaskManager:
        return ContextMenuType.kPinnedActionTaskManager;
      case PinnedToolbarAction.kDevTools:
        return ContextMenuType.kPinnedActionDevTools;
      case PinnedToolbarAction.kTabSearch:
        return ContextMenuType.kPinnedActionTabSearch;
      case PinnedToolbarAction.kSidePanelShowContextualTasks:
        return ContextMenuType.kPinnedActionSidePanelShowContextualTasks;
      case PinnedToolbarAction.kSidePanelShowLens:
        return ContextMenuType.kPinnedActionSidePanelShowLens;
      case PinnedToolbarAction.kSidePanelShowAboutThisSite:
        return ContextMenuType.kPinnedActionSidePanelShowAboutThisSite;
      case PinnedToolbarAction.kSidePanelShowCustomizeChrome:
        return ContextMenuType.kPinnedActionSidePanelShowCustomizeChrome;
      case PinnedToolbarAction.kSidePanelShowShoppingInsights:
        return ContextMenuType.kPinnedActionSidePanelShowShoppingInsights;
      case PinnedToolbarAction.kSidePanelShowMerchantTrust:
        return ContextMenuType.kPinnedActionSidePanelShowMerchantTrust;
      case PinnedToolbarAction.kSendSharedTabGroupFeedback:
        return ContextMenuType.kPinnedActionSendSharedTabGroupFeedback;
      case PinnedToolbarAction.kSidePanelShowComments:
        return ContextMenuType.kPinnedActionSidePanelShowComments;
      case PinnedToolbarAction.kUnspecified:
      case PinnedToolbarAction.kDivider:
        return ContextMenuType.kUnspecified;
      default:
        assertNotReachedCase(this.state.action);
    }
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(this.state.tooltip);
  }

  protected onContextmenu_(e: Event) {
    e.preventDefault();
    const type = this.getContextMenuType_();
    if (type !== ContextMenuType.kUnspecified) {
      this.browserProxy_.toolbarUIHandler.showContextMenu(
          type, getContextMenuPosition(this), getContextMenuSourceType(e));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'pinned-toolbar-action': PinnedToolbarActionElement;
  }
}

customElements.define(
    PinnedToolbarActionElement.is, PinnedToolbarActionElement);
