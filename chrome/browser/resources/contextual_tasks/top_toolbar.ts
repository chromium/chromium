// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import '//resources/cr_elements/icons.html.js';

import type {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrLazyRenderLitElement} from 'chrome://resources/cr_elements/cr_lazy_render/cr_lazy_render_lit.js';
import {getFaviconForPageURL} from 'chrome://resources/js/icon.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Tab} from './contextual_tasks.mojom-webui.js';
import type {BrowserProxy} from './contextual_tasks_browser_proxy.js';
import {BrowserProxyImpl} from './contextual_tasks_browser_proxy.js';
import {getCss} from './top_toolbar.css.js';
import {getHtml} from './top_toolbar.html.js';

export class TopToolbarElement extends CrLitElement {
  static get is() {
    return 'top-toolbar';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      title: {type: String},
      attachedTabs_: {type: Array},
    };
  }

  override accessor title: string = '';
  accessor attachedTabs_: Tab[] = [];
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  private get menu_(): CrLazyRenderLitElement<CrActionMenuElement>|null {
    return this.shadowRoot
        .querySelector<CrLazyRenderLitElement<CrActionMenuElement>>('#menu');
  }

  private get sourcesMenu_(): CrLazyRenderLitElement<CrActionMenuElement>|null {
    return this.shadowRoot
        .querySelector<CrLazyRenderLitElement<CrActionMenuElement>>(
            '#sourcesMenu');
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected onSigninClick_() {
    this.fire('signin-click');
  }

  protected onCloseButtonClick_() {
    this.fire('close-button-click');
  }

  protected onNewThreadClick_() {
    this.fire('new-thread-click');
  }

  protected onThreadHistoryClick_() {
    this.fire('thread-history-click');
  }

  protected onMoreClick_(e: MouseEvent) {
    this.menu_?.get().showAt(e.target as HTMLElement);
  }

  protected async onSourcesClick_(e: MouseEvent) {
    const {tabs} = await this.browserProxy_.handler.getAttachedTabs();
    this.attachedTabs_ = tabs;
    this.sourcesMenu_?.get().showAt(e.target as HTMLElement);
  }

  protected onTabClick_(tab: Tab) {
    this.sourcesMenu_?.get().close();
    this.fire('tab-click', tab);
  }

  protected onOpenInNewTabClick_() {
    this.menu_?.get().close();
    this.fire('open-in-new-tab-click');
  }

  protected onMyActivityClick_() {
    this.menu_?.get().close();
    this.fire('my-activity-click');
  }

  protected onHelpClick_() {
    this.menu_?.get().close();
    this.fire('help-click');
  }

  protected faviconUrl_(tab: Tab): string {
    return getFaviconForPageURL(tab.url.url, false);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'top-toolbar': TopToolbarElement;
  }
}

customElements.define(TopToolbarElement.is, TopToolbarElement);
