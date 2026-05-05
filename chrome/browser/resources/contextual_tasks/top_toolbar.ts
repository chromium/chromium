// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';
import './favicon_group.js';
import './reopen_tabs.js';
import './sources_menu.js';
import './overflow_menu.js';

import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import type {OverflowMenuElement} from './overflow_menu.js';
import type {SourcesMenuElement} from './sources_menu.js';
import {getCss} from './top_toolbar.css.js';
import {getHtml} from './top_toolbar.html.js';
import {recordAction} from './utils.js';

export interface TopToolbarElement {
  $: {
    closeButton: HTMLImageElement,
    overflowMenu: CrLazyRenderLitElement<OverflowMenuElement>,
    newThreadButton: HTMLImageElement,
    sourcesMenu: CrLazyRenderLitElement<SourcesMenuElement>,
    threadHistoryButton: HTMLImageElement,
  };
}

export class TopToolbarElement extends CrLitElement {
  static get is() {
    return 'top-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      contextInfos: {type: Array},
      darkMode: {
        type: Boolean,
        reflect: true,
        attribute: 'dark-mode',
      },
      isAiPage: {
        type: Boolean,
        reflect: true,
        attribute: 'is-ai-page',
      },
      enableOpenInNewTabButton: {
        type: Boolean,
        reflect: true,
      },
      title: {type: String},
      hideOverflowMenuButton_: {type: Boolean},
      showReopenTabs_: {type: Boolean},
      isExpandButtonEnabled: {type: Boolean},
      isPinButtonEnabled: {type: Boolean},
      isPinned: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
    };
  }

  override accessor title: string = '';
  accessor contextInfos: ContextInfo[] = [];
  accessor darkMode: boolean = false;
  accessor isAiPage: boolean = loadTimeData.getBoolean('isAiPage');
  accessor enableOpenInNewTabButton: boolean = false;
  accessor showReopenTabs_: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  protected accessor isExpandButtonEnabled: boolean =
      loadTimeData.getBoolean('expandButtonEnabled');
  protected accessor isPinButtonEnabled: boolean =
      loadTimeData.getBoolean('enablePinButton');
  private hideOverflowMenuOnAiPageEnabled_: boolean =
      loadTimeData.getBoolean('hideMenuOnAiPageEnabled');
  accessor hideOverflowMenuButton_: boolean =
      this.hideOverflowMenuOnAiPageEnabled_ && this.isAiPage;
  protected accessor isPinned: boolean =
      loadTimeData.getBoolean('isSidePanelPinned');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [
      callbackRouter.onContextUpdated.addListener(contextInfos => {
        this.contextInfos = contextInfos;
      }),
      callbackRouter.setShowReopenTabs.addListener(show => {
        this.showReopenTabs_ = show;
      }),
      callbackRouter.onSidePanelPinStateChanged.addListener(
          (isPinned: boolean) => {
            this.isPinned = isPinned;
          }),
      callbackRouter.setExpandButtonEnabled.addListener((enabled: boolean) => {
        this.isExpandButtonEnabled = enabled;
      }),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('isAiPage')) {
      this.hideOverflowMenuButton_ =
          this.isAiPage && this.hideOverflowMenuOnAiPageEnabled_;
    }
  }

  protected shouldShowPinButton_(): boolean {
    return this.isPinButtonEnabled && this.isAiPage;
  }

  protected getPinButtonTooltip_(): string {
    return this.isPinned ? loadTimeData.getString('unpinTooltip') :
                           loadTimeData.getString('pinTooltip');
  }

  protected shouldShowSourcesMenuButton_(): boolean {
    return this.contextInfos.length > 0;
  }

  protected onPinClick_() {
    this.isPinned = !this.isPinned;
    if (this.isPinned) {
      this.browserProxy_.handler.pinSidePanel();
    } else {
      this.browserProxy_.handler.unpinSidePanel();
    }
  }

  protected onCloseButtonClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.CloseSidePanel');
    this.browserProxy_.handler.closeSidePanel();
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenThreadHistory');
    this.browserProxy_.handler.showThreadHistory();
  }

  protected onOverflowMenuButtonClick_(e: Event) {
    this.$.overflowMenu.get().showAt(e.target as HTMLElement);
  }

  protected onSourcesClick_(e: Event) {
    recordAction('ContextualTasks.WebUI.UserAction.OpenSourcesMenu');
    this.$.sourcesMenu.get().showAt(e.target as HTMLElement);
  }

  protected onOpenInNewTabClick_() {
    recordAction('ContextualTasks.WebUI.UserAction.OpenInNewTab');
    this.browserProxy_.handler.moveTaskUiToNewTab();
  }

  protected onReopenTabsReopenClick_() {
    this.browserProxy_.handler.reopenTabs();
  }

  protected onReopenTabsDismissClick_() {
    this.showReopenTabs_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
