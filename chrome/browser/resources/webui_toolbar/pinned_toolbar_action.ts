// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import './pinned_toolbar_action_icons.html.js';

// <if expr="_google_chrome">
import './internal/icons.html.js';
// </if>

import {assertNotReached, assertNotReachedCase} from '//resources/js/assert.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {ContextMenuType} from './browser_proxy.js';
import {getCss} from './pinned_toolbar_action.css.js';
import {getHtml} from './pinned_toolbar_action.html.js';
import {getContextMenuPosition, getContextMenuSourceType} from './toolbar_button.js';
import {PinnedToolbarAction} from './toolbar_ui_api_data_model.mojom-webui.js';
import type {PinnedToolbarActionState} from './toolbar_ui_api_data_model.mojom-webui.js';

export class PinnedToolbarActionElement extends CrLitElement {
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
      state: {type: Object},
    };
  }

  accessor state: PinnedToolbarActionState = {
    action: PinnedToolbarAction.kUnspecified,
    highlighted: false,
    enabled: true,
    tooltip: '',
    accessibilityText: '',
    elementId: null,
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private trackedElementManager_: TrackedElementManager =
      TrackedElementManager.getInstance();

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.trackedElementManager_.stopTracking(this);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const oldId = oldState?.elementId;
      const newId = this.state.elementId;

      if (oldId !== newId) {
        if (oldId) {
          this.trackedElementManager_.stopTracking(this);
        }
        if (newId) {
          this.trackedElementManager_.startTracking(this, newId);
        }
      }
    }
  }

  protected getIcon_(): {ironIcon?: string, className?: string} {
    const type = this.state.action;

    // TODO(crbug.com/474061420): Fill this in.
    switch (type) {
      case PinnedToolbarAction.kUnspecified:
      case PinnedToolbarAction.kDivider:
        assertNotReached();
      case PinnedToolbarAction.kShowPasswordsBubbleOrPage:
        return {className: 'icon-password-manager'};
      case PinnedToolbarAction.kShowAddressesBubbleOrPage:
        return {className: 'icon-location-on-chrome-refresh'};
      case PinnedToolbarAction.kNewIncognitoWindow:
      case PinnedToolbarAction.kShowPaymentsBubbleOrPage:
      case PinnedToolbarAction.kSidePanelShowBookmarks:
      case PinnedToolbarAction.kSidePanelShowReadingList:
      case PinnedToolbarAction.kSidePanelShowHistoryCluster:
      case PinnedToolbarAction.kShowDownloads:
      case PinnedToolbarAction.kClearBrowsingData:
      case PinnedToolbarAction.kPrint:
      case PinnedToolbarAction.kShowTranslate:
      case PinnedToolbarAction.kQrCodeGenerator:
      case PinnedToolbarAction.kRouteMedia:
      case PinnedToolbarAction.kRouteMediaIdle:
      case PinnedToolbarAction.kRouteMediaWarning:
      case PinnedToolbarAction.kRouteMediaPaused:
      case PinnedToolbarAction.kRouteMediaActive:
      case PinnedToolbarAction.kSidePanelShowReadAnything:
      case PinnedToolbarAction.kCopyUrl:
      case PinnedToolbarAction.kSendTabToSelf:
      case PinnedToolbarAction.kTaskManager:
      case PinnedToolbarAction.kDevTools:
      case PinnedToolbarAction.kTabSearch:
      case PinnedToolbarAction.kSidePanelShowContextualTasks:
      case PinnedToolbarAction.kSidePanelShowLens:
      case PinnedToolbarAction.kSidePanelShowCustomizeChrome:
      case PinnedToolbarAction.kSidePanelShowShoppingInsights:
      case PinnedToolbarAction.kSidePanelShowMerchantTrust:
      case PinnedToolbarAction.kSendSharedTabGroupFeedback:
      case PinnedToolbarAction.kSidePanelShowComments: {
        const iconName = PinnedToolbarAction[type].slice(1);
        return {ironIcon: `pinned-toolbar-action:${iconName}`};
      }
      case PinnedToolbarAction.kSidePanelShowAboutThisSite:
        // <if expr="_google_chrome">
        return {ironIcon: 'internal-icons:page_insights'};
        // </if>
        // <if expr="not _google_chrome">
        return {ironIcon: 'pinned-toolbar-action:SidePanelShowAboutThisSite'};
        // </if>
      case PinnedToolbarAction.kSidePanelShowLensOverlayResults:
        // <if expr="_google_chrome">
        return {ironIcon: 'internal-icons:google_lens_monochrome_logo'};
        // </if>
        // <if expr="not _google_chrome">
        return {ironIcon: 'pinned-toolbar-action:SidePanelShowLensOverlayResults'};
        // </if>
      default:
        assertNotReachedCase(type);
    }
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
      case PinnedToolbarAction.kRouteMediaIdle:
      case PinnedToolbarAction.kRouteMediaWarning:
      case PinnedToolbarAction.kRouteMediaPaused:
      case PinnedToolbarAction.kRouteMediaActive:
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
