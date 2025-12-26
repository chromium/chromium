// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageHandlerInterface as SearchboxPageHandlerInterface, SearchContextStub} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './aim_app.css.js';
import {getHtml} from './aim_app.html.js';
import {BrowserProxy} from './aim_browser_proxy.js';
import type {PageCallbackRouter, PageHandlerInterface} from './omnibox_popup_aim.mojom-webui.js';

export interface OmniboxAimAppElement {
  $: {
    composebox: ComposeboxElement,
  };
}

export class OmniboxAimAppElement extends CrLitElement {
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
  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];
  private preserveContextOnClose_: boolean = false;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.searchboxPageHandler_ = SearchboxBrowserProxy.getInstance().handler;
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    this.pageHandler_ = BrowserProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.callbackRouter_.onPopupShown.addListener(
          this.onPopupShown_.bind(this)),
      this.callbackRouter_.addContext.addListener(this.addContext_.bind(this)),
      this.callbackRouter_.onPopupHidden.addListener(
          this.onPopupHidden_.bind(this)),
      this.callbackRouter_.setPreserveContextOnClose.addListener(
          this.setPreserveContextOnClose_.bind(this)),
    ];

    this.$.composebox.focusInput();

    if (!this.isDebug_) {
      this.eventTracker_.add(
          document.documentElement, 'contextmenu', (e: Event) => {
            e.preventDefault();
          });
    }

    this.setupLocalizedLinkListener();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();

    for (const listenerId of this.listenerIds_) {
      this.callbackRouter_.removeListener(listenerId);
    }
    this.listenerIds_ = [];
  }

  // As links do not navigate in the omnibox as they do in normal
  // web ui pages, set up a listener to open the link in the current
  // tab.
  private setupLocalizedLinkListener() {
    const link = this.$.composebox.shadowRoot.querySelector('localized-link')
                     ?.shadowRoot!.querySelector('#container a');
    if (link) {
      link.addEventListener('click', this.onLinkClick_.bind(this));
    }
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
    this.pageHandler_.requestClose();
  }

  protected setPreserveContextOnClose_(preserveContextOnClose: boolean) {
    assert(document.visibilityState === 'visible');
    this.preserveContextOnClose_ = preserveContextOnClose;
  }

  private onPopupShown_(context: SearchContextStub) {
    if (!this.preserveContextOnClose_) {
      // Avoid showing the glow animation when coming back from a preserved
      // context on close as this indicates that the user is returning to the
      // widget after adding context.
      this.$.composebox.playGlowAnimation();
    }
    this.$.composebox.addSearchContext(context);
    this.$.composebox.focusInput();
    this.preserveContextOnClose_ = false;
  }

  private addContext_(context: SearchContextStub) {
    this.$.composebox.addSearchContext(context);
    this.$.composebox.focusInput();
  }

  private onPopupHidden_(): Promise<{input: string}> {
    const input = this.$.composebox.getInputText();
    if (!this.preserveContextOnClose_) {
      this.$.composebox.clearAllInputs(/* querySubmitted= */ false);
      this.$.composebox.clearAutocompleteMatches();
      this.$.composebox.resetModes();
    }
    return Promise.resolve({input});
  }

  protected onComposeboxSubmit_() {
    this.$.composebox.clearAllInputs(/* querySubmitted= */ true);
  }

  private onLinkClick_(e: Event) {
    e.preventDefault();
    const href = (e.currentTarget as HTMLAnchorElement).href;
    this.pageHandler_.navigateCurrentTab({url: href});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-aim-app': OmniboxAimAppElement;
  }
}

customElements.define(OmniboxAimAppElement.is, OmniboxAimAppElement);
