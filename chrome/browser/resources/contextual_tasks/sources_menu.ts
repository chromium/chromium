// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {ContextInfo} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './sources_menu.css.js';
import {getHtml} from './sources_menu.html.js';

export interface SourcesMenuElement {
  $: {
    menu: CrActionMenuElement,
  };
}

export class SourcesMenuElement extends CrLitElement {
  static get is() {
    return 'contextual-tasks-sources-menu';
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
    };
  }
  accessor contextInfos: ContextInfo[] = [];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  showAt(target: HTMLElement) {
    this.$.menu.showAt(target, {
      noOffset: true,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
    });
  }

  close() {
    this.$.menu.close();
  }

  private getContextInfoFromEvent_(e: Event): ContextInfo {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    return this.contextInfos[index]!;
  }

  protected onTabClick_(e: Event) {
    this.close();

    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.TabFromSourcesMenuClicked');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.TabFromSourcesMenuClicked', true);
    const contextInfo = this.getContextInfoFromEvent_(e);
    this.browserProxy_.handler.onTabClickedFromSourcesMenu(
        contextInfo.tab!.tabId, contextInfo.tab!.url);
  }

  protected onFileClick_(e: Event) {
    this.close();

    const contextInfo = this.getContextInfoFromEvent_(e);
    this.browserProxy_.handler.onFileClickedFromSourcesMenu(
        contextInfo.file!.url);
  }

  protected onImageClick_(e: Event) {
    this.close();
    const contextInfo = this.getContextInfoFromEvent_(e);
    this.browserProxy_.handler.onImageClickedFromSourcesMenu(
        contextInfo.image!.url);
  }

  protected faviconUrl_(url: string): string {
    return getFaviconForPageURL(url, false);
  }

  protected getHostname_(url: string): string {
    try {
      return new URL(url).hostname;
    } catch (e) {
      return url;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-sources-menu': SourcesMenuElement;
  }
}

customElements.define(SourcesMenuElement.is, SourcesMenuElement);
