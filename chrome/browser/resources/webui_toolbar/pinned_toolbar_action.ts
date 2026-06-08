// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './pinned_toolbar_action_icons.html.js';
// <if expr="_google_chrome">
import './internal/icons.html.js';

// </if>

import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {ContextMenuType} from './browser_proxy.js';
import {IconTable} from './icon_table.js';
import {getCss} from './pinned_toolbar_action.css.js';
import {getHtml} from './pinned_toolbar_action.html.js';
import {getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';
import {PinnedToolbarAction} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from './toolbar_ui_api_data_model.mojom-webui.js';

const PinnedToolbarActionElementBase = HelpBubbleAnchorMixin(CrLitElement);

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

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private iconTable_: IconTable = IconTable.getInstance();

  protected accessor trackedHighlighted: boolean = false;

  override disconnectedCallback() {
    super.disconnectedCallback();
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldId = oldState?.elementId;
      const newId = this.state.elementId;

      if (oldId !== newId) {
        if (oldId) {
          this.unregisterHelpBubble(oldId);
        }
        if (newId) {
          this.registerHelpBubble(newId, this, {
            onHighlightChanged: (highlighted: boolean) => {
              this.trackedHighlighted = highlighted;
            },
            onHelpBubbleShown: () => setHasHelpBubble(this, true),
            onHelpBubbleHidden: () => setHasHelpBubble(this, false),
          });
        }
      }
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
