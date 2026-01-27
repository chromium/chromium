// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_components/composebox/icons.html.js';

import {assert} from '//resources/js/assert.js';
import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Image, Tab, UploadedFile} from './contextual_tasks.mojom-webui.js';
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
      attachedTabs: {type: Array},
      attachedFiles: {type: Array},
      attachedImages: {type: Array},
    };
  }
  accessor attachedTabs: Tab[] = [];
  accessor attachedFiles: UploadedFile[] = [];
  accessor attachedImages: Image[] = [];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  showAt(target: HTMLElement) {
    this.$.menu.showAt(target);
  }

  close() {
    this.$.menu.close();
  }

  protected onTabClick_(e: Event) {
    this.close();

    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const tab = this.attachedTabs[index];
    assert(tab);

    chrome.metricsPrivate.recordUserAction(
        'ContextualTasks.WebUI.UserAction.TabFromSourcesMenuClicked');
    chrome.metricsPrivate.recordBoolean(
        'ContextualTasks.WebUI.UserAction.TabFromSourcesMenuClicked', true);
    this.browserProxy_.handler.onTabClickedFromSourcesMenu(tab.tabId, tab.url);
  }

  protected onFileClick_(e: Event) {
    this.close();

    const currentTarget = e.currentTarget as HTMLElement;
    const index = Number(currentTarget.dataset['index']);
    const file = this.attachedFiles[index];
    assert(file);
    this.browserProxy_.handler.onFileClickedFromSourcesMenu(file.url);
  }

  protected onImageClick_(e: Event) {
    this.close();
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const image = this.attachedImages[index];
    assert(image);
    this.browserProxy_.handler.onImageClickedFromSourcesMenu(image.url);
  }

  protected faviconUrl_(tab: Tab): string {
    return getFaviconForPageURL(tab.url.url, false);
  }

  protected getImageUrl_(image: Image): string {
    return 'chrome://image/?' + image.url;
  }


  protected getHostname_(url: string): string {
    try {
      return new URL(url).hostname;
    } catch (e) {
      return url;
    }
  }

  protected shouldShowFileDivider_(): boolean {
    return this.attachedTabs.length > 0 && this.attachedFiles.length > 0;
  }

  protected shouldShowImageDivider_(): boolean {
    return (this.attachedTabs.length > 0 || this.attachedFiles.length > 0) &&
        this.attachedImages.length > 0;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'contextual-tasks-sources-menu': SourcesMenuElement;
  }
}

customElements.define(SourcesMenuElement.is, SourcesMenuElement);
