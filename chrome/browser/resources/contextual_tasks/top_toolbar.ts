// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';
import './favicon_group.js';
import './sources_menu.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import type {SourcesMenuElement} from './sources_menu.js';
import {getCss} from './top_toolbar.css.js';
import {getHtml} from './top_toolbar.html.js';

export interface TopToolbarElement {
  $: {
    closeButton: HTMLImageElement,
    menu: CrLazyRenderLitElement<CrActionMenuElement>,
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
      logoImageUrl_: {type: String},
      title: {type: String},
    };
  }

  override accessor title: string = '';
  accessor contextInfos: ContextInfo[] = [];
  accessor darkMode: boolean = false;
  accessor isAiPage: boolean = false;
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  private listenerIds_: number[] = [];
  protected isExpandButtonEnabled: boolean =
      loadTimeData.getBoolean('expandButtonEnabled');

  override connectedCallback() {
    super.connectedCallback();
    const callbackRouter = this.browserProxy_.callbackRouter;
    this.listenerIds_ = [callbackRouter.onContextUpdated.addListener(
        (contextInfos: ContextInfo[]) => {
          this.contextInfos = contextInfos;
        })];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.browserProxy_.callbackRouter.removeListener(id));
    this.listenerIds_ = [];
  }

  protected shouldShowSourcesMenuButton_(): boolean {
    return this.contextInfos.length > 0;
  }

  protected onCloseButtonClick_() {
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.CloseSidePanel');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.CloseSidePanel', true);
    this.browserProxy_.handler.closeSidePanel();
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenThreadHistory');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenThreadHistory', true);
    this.browserProxy_.handler.showThreadHistory();
  }

  protected onMoreClick_(e: Event) {
    this.$.menu.get().showAt(e.target as HTMLElement, {
      noOffset: true,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  protected onSourcesClick_(e: Event) {
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenSourcesMenu');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenSourcesMenu', true);
    this.$.sourcesMenu.get().showAt(e.target as HTMLElement);
  }

  protected onOpenInNewTabClick_() {
    this.$.menu.get().close();
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenInNewTab');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenInNewTab', true);
    this.browserProxy_.handler.moveTaskUiToNewTab();
  }

  protected onMyActivityClick_() {
    this.$.menu.get().close();
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenMyActivity');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenMyActivity', true);
    this.browserProxy_.handler.openMyActivityUi();
  }

  protected onHelpClick_() {
    this.$.menu.get().close();
    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.OpenHelp');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.OpenHelp', true);
    this.browserProxy_.handler.openHelpUi();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
