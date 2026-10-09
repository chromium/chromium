// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './toolbar_chip_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import './icons.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {getContextMenuPosition, getContextMenuSourceType, HelpBubbleAnchorMixin, HighlightTracker} from '/shared/toolbar_button.js';
import type {GlicButtonState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl, ContextMenuType} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {CollapsibleLabelButtonMixin} from './collapsible_label_button.js';
import {getCss} from './glic_button.css.js';
import {getHtml} from './glic_button.html.js';
import type {ToolbarChipButtonElement} from './toolbar_chip_button.js';

export interface GlicButtonElement {
  $: {
    button: ToolbarChipButtonElement,
  };
}

const GlicButtonElementBase =
    HelpBubbleAnchorMixin(CollapsibleLabelButtonMixin(CrLitElement));

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
      ...super.properties,
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

  highlightTracker: HighlightTracker = new HighlightTracker();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    const oldState = changedProperties.get('state');
    const stateNeedsLayout = changedProperties.has('state') &&
        (!oldState || oldState.shouldShow !== this.state.shouldShow ||
         oldState.nudgeLabel !== this.state.nudgeLabel);
    if (stateNeedsLayout || changedProperties.has('label')) {
      this.fire('request-layout');
    }
  }

  override focus() {
    this.$.button.focus();
  }

  override shouldBeShown(): boolean {
    return this.state.shouldShow;
  }

  protected override hasLabel_(): boolean {
    return !!this.getLabel_();
  }

  protected getLabel_(): string {
    return this.state.nudgeLabel || this.label;
  }

  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(loadTimeData.getString(
        this.state.open ? 'glicButtonTooltipClose' : 'glicButtonTooltip'));
  }

  protected getAriaLabel_(): string {
    if (this.state.open) {
      return loadTimeData.getString('glicButtonTooltipClose');
    }
    return this.getLabel_() || loadTimeData.getString('glicButtonAccName');
  }

  protected onClick_(e: PointerEvent) {
    if (!this.highlightTracker.shouldSkipClick(e)) {
      TrackedElementManager.getInstance().notifyElementActivated(this);
      this.browserProxy_.toolbarUIHandler.onGlicButtonClicked();
    }
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
