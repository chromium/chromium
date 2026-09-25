// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {GlicButtonState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './glic_button.css.js';
import {getHtml} from './glic_button.html.js';
import {getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin, setHasHelpBubble} from './toolbar_button.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface GlicButtonElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

const GlicButtonElementBase = HelpBubbleAnchorMixin(CrLitElement);

export class GlicButtonElement extends GlicButtonElementBase {
  static get is() {
    return 'glic-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      enabled: {type: Boolean},
      state: {type: Object},
      label: {type: String},
    };
  }

  accessor enabled: boolean = true;
  accessor state: GlicButtonState = {
    open: false,
    shouldShow: false,
    isContextMenuVisible: false,
    nudgeLabel: null,
  };
  accessor label: string = loadTimeData.getString('glicButtonLabel');

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override connectedCallback() {
    super.connectedCallback();
    this.registerHelpBubble('kGlicButtonElementId', this.$.button, {
      onHighlightChanged: (highlighted: boolean) => {
        this.classList.toggle('anchor-highlight', highlighted);
      },
      onHelpBubbleShown: () => setHasHelpBubble(this, true),
      onHelpBubbleHidden: () => setHasHelpBubble(this, false),
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.unregisterHelpBubble('kGlicButtonElementId');
  }

  override focus() {
    this.$.button.focus();
  }

  protected getLabel_(): string {
    return this.state.nudgeLabel || this.label;
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(loadTimeData.getString(
        this.state.open ? 'glicButtonTooltipClose' : 'glicButtonTooltip'));
  }

  protected getAriaLabel_(): string {
    return loadTimeData.getString('glicButtonAccName');
  }

  protected onClick_() {
    this.browserProxy_.toolbarUIHandler.onGlicButtonClicked();
  }

  protected onContextmenu_(e: MouseEvent) {
    e.preventDefault();
    this.browserProxy_.toolbarUIHandler.showContextMenu(
        ContextMenuType.kGlic, getContextMenuPosition(this),
        getContextMenuSourceType(e), /*showMenuToken=*/ null);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'glic-button': GlicButtonElement;
  }
}

customElements.define(GlicButtonElement.is, GlicButtonElement);
