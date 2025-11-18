// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageHandlerInterface as SearchboxPageHandlerInterface, SearchContextStub} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './aim_app.css.js';
import {getHtml} from './aim_app.html.js';
import type {Page} from './omnibox_popup_aim.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './omnibox_popup_aim.mojom-webui.js';

export class OmniboxAimAppElement extends CrLitElement implements Page {
  static get is() {
    return 'omnibox-aim-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  protected searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');

  private isDebug_: boolean =
      new URLSearchParams(window.location.search).has('debug');
  private eventTracker_ = new EventTracker();
  private searchboxPageHandler_: SearchboxPageHandlerInterface;
  private pageHandler_: PageHandlerRemote;
  private callbackRouter_: PageCallbackRouter;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.searchboxPageHandler_ = SearchboxBrowserProxy.getInstance().handler;

    this.callbackRouter_ = new PageCallbackRouter();
    this.pageHandler_ = new PageHandlerRemote();
    PageHandlerFactory.getRemote().createPageHandler(
        this.callbackRouter_.$.bindNewPipeAndPassRemote(),
        this.pageHandler_.$.bindNewPipeAndPassReceiver());

    this.callbackRouter_.onShow.addListener(this.onShow_.bind(this));
    this.callbackRouter_.onClose.addListener(this.onClose_.bind(this));
    this.callbackRouter_.addContext.addListener(this.addContext_.bind(this));
  }

  override connectedCallback() {
    super.connectedCallback();

    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    composebox.focusInput();

    if (!this.isDebug_) {
      this.eventTracker_.add(
          document.documentElement, 'contextmenu', (e: Event) => {
            e.preventDefault();
          });
    }

    this.eventTracker_.add(
        composebox, 'composebox-submit', this.onComposeboxSubmit_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected onContextualEntryPointClicked_(
      e: CustomEvent<{x: number, y: number}>) {
    e.preventDefault();
    const point = {
      x: e.detail.x,
      y: e.detail.y,
    };
    this.searchboxPageHandler_.showContextMenu(point);
  }

  protected onCloseComposebox_() {
    this.pageHandler_.close();
  }

  private onShow_(context: SearchContextStub|null) {
    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    composebox.playGlowAnimation();
    composebox.setSearchContext(context);
    composebox.focusInput();
  }

  private addContext_(context: SearchContextStub) {
    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    composebox.setSearchContext(context);
    composebox.focusInput();
  }

  private onClose_(): Promise<{input: string}> {
    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    const input = composebox.getInputText();
    composebox.clearAllInputs();
    composebox.clearAutocompleteMatches();
    return Promise.resolve({input});
  }

  private onComposeboxSubmit_() {
    const composebox = this.shadowRoot.querySelector('cr-composebox');
    assert(composebox);
    composebox.clearAllInputs();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-aim-app': OmniboxAimAppElement;
  }
}

customElements.define(OmniboxAimAppElement.is, OmniboxAimAppElement);
