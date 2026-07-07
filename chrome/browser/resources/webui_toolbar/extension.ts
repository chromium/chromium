// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/shared/icon_from_table.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {ExtensionActionInfo} from '/shared/extensions_bar_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {getHtml} from './extension.html.js';
import {getCss} from './toolbar_button.css.js';
import {getContextMenuSourceType, HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';

const ExtensionElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class ExtensionElement extends ExtensionElementBase {
  static get is() {
    return 'webui-toolbar-extension';
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

  accessor state: ExtensionActionInfo = {
    id: '',
    accessibleName: '',
    tooltip: '',
    isVisible: false,
    icon: {handleId: 0n},
  };

  private browserProxy_ = BrowserProxyImpl.getInstance();
  private registerHelpBubbleController_: AbortController|null = null;

  protected accessor trackedHighlighted: boolean = false;

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.registerHelpBubbleController_) {
      this.registerHelpBubbleController_.abort();
      this.registerHelpBubbleController_ = null;
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      const wasMenuButton = oldState?.id === '';
      const isMenuButton = this.state.id === '';

      if (wasMenuButton !== isMenuButton) {
        if (this.registerHelpBubbleController_) {
          this.registerHelpBubbleController_.abort();
          this.registerHelpBubbleController_ = null;
        }
        if (wasMenuButton) {
          this.unregisterHelpBubble('kExtensionsMenuButtonElementId');
        }
        if (isMenuButton) {
          this.registerHelpBubble_('kExtensionsMenuButtonElementId');
        }
      }
    }
  }

  private async registerHelpBubble_(newId: string) {
    this.registerHelpBubbleController_ = new AbortController();
    const signal = this.registerHelpBubbleController_.signal;

    const animations = this.getAnimations().filter(anim => {
      const timing = anim.effect?.getTiming();
      // Ignore infinite animations (e.g. pulsing for IPH).
      return timing?.iterations !== Infinity && timing?.duration !== Infinity;
    });

    // Wait for any animations to complete, so button is in final location.
    if (animations.length > 0) {
      try {
        await Promise.all(animations.map(a => a.finished));
      } catch (e) {
        // Ignore animation cancellation.
      }
    }

    if (!signal.aborted) {
      this.registerHelpBubble(newId, this, {
        onHighlightChanged: (highlighted: boolean) => {
          this.trackedHighlighted = highlighted;
        },
        onHelpBubbleShown: () => setHasHelpBubble(this, true),
        onHelpBubbleHidden: () => setHasHelpBubble(this, false),
      });
      this.registerHelpBubbleController_ = null;
    }
  }

  protected onClick_() {
    this.browserProxy_.toolbarUIHandler.executeExtensionAction(this.state.id);
  }

  protected onContextmenu_(e: Event) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showExtensionContextMenu(
        this.state.id, getContextMenuSourceType(e));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-toolbar-extension': ExtensionElement;
  }
}

customElements.define(ExtensionElement.is, ExtensionElement);
