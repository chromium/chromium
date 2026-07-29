// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/contextual_entrypoint_button.js';

import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {getInstance as getA11yAnnouncer} from '//resources/cr_elements/cr_a11y_announcer/cr_a11y_announcer.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getHtml} from './omnibox_contextual_entrypoint_button.html.js';
import type {BrowserProxy} from './omnibox_popup.mojom-webui.js';
import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';

export class OmniboxContextualEntrypointButtonElement extends CrLitElement {
  static get is() {
    return 'omnibox-contextual-entrypoint-button';
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      inputState: {type: Object},
      applyContextButtonBackground: {
        type: Boolean,
        reflect: true,
      },
      isOblongShape: {
        type: Boolean,
        reflect: true,
      },
      showSuggestionLabel: {
        type: Boolean,
        reflect: true,
      },
      hasPopupFocus: {
        type: Boolean,
        reflect: true,
      },
      isMenuOpen_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor inputState: InputState|null = null;
  accessor applyContextButtonBackground: boolean = false;
  accessor isOblongShape: boolean = false;
  accessor showSuggestionLabel: boolean = false;
  accessor hasPopupFocus: boolean = false;
  protected accessor isMenuOpen_: boolean = false;

  private browserProxy_: BrowserProxy;
  private eventTracker_ = new EventTracker();
  private popupListenerIds_: number[] = [];

  constructor() {
    super();
    this.browserProxy_ = browserProxyFactory.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.popupListenerIds_ = [
      this.browserProxy_.callbackRouter.onShow.addListener(
          this.onShow_.bind(this)),
      this.browserProxy_.callbackRouter.onContextMenuClosed.addListener(
          this.onContextMenuClosed_.bind(this)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.isMenuOpen_ = false;
    this.classList.remove('menu-open');
    this.eventTracker_.removeAll();
    for (const listenerId of this.popupListenerIds_) {
      this.browserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.popupListenerIds_ = [];
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('hasPopupFocus') && this.hasPopupFocus) {
      this.announce_();
    }

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('isMenuOpen_')) {
      this.classList.toggle('menu-open', this.isMenuOpen_);
    }
  }

  showContextMenu(point: {x: number, y: number} = {x: 0, y: 0}) {
    this.isMenuOpen_ = true;
    this.classList.add('menu-open');
    this.browserProxy_.handler.showContextMenu(point);
  }

  protected onContextMenuEntrypointClick_(
      e: CustomEvent<{x: number, y: number}>) {
    e.preventDefault();
    this.showContextMenu({x: e.detail?.x ?? 0, y: e.detail?.y ?? 0});
  }

  private onShow_() {
    // When the popup is shown, blur the contextual entrypoint button and reset
    // menu state.
    this.isMenuOpen_ = false;
    this.classList.remove('menu-open');
    this.blur();
    this.shadowRoot
        ?.querySelector<ContextualEntrypointButtonElement>(
            'cr-composebox-contextual-entrypoint-button')
        ?.blur();
  }

  private onContextMenuClosed_() {
    this.isMenuOpen_ = false;
    this.classList.remove('menu-open');
  }

  private announce_() {
    const entrypoint =
        this.shadowRoot?.querySelector<ContextualEntrypointButtonElement>(
            'cr-composebox-contextual-entrypoint-button');
    const message = entrypoint?.shadowRoot?.querySelector('#entrypoint')
                        ?.getAttribute('aria-label');
    if (message) {
      if (this.ariaNotify) {
        this.ariaNotify(message);
      } else {
        getA11yAnnouncer(this).announce(message);
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-contextual-entrypoint-button':
        OmniboxContextualEntrypointButtonElement;
  }
}

customElements.define(
    OmniboxContextualEntrypointButtonElement.is,
    OmniboxContextualEntrypointButtonElement);
