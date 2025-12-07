// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '//resources/cr_elements/md_select.css.js';

import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrTooltipElement} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {TooltipPosition} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_tooltip_demo.css.js';
import {getHtml} from './cr_tooltip_demo.html.js';

export interface CrTooltipDemoElement {
  $: {
    manualTooltip: CrTooltipElement,
    target1: HTMLElement,
    target2: HTMLElement,
  };
}

export class CrTooltipDemoElement extends CrLitElement {
  static get is() {
    return 'cr-tooltip-demo';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      tooltipPosition_: {type: String},
      tooltipOffset_: {type: Number},
    };
  }

  // Default values.
  protected accessor tooltipPosition_: TooltipPosition = TooltipPosition.BOTTOM;
  protected accessor tooltipOffset_: number = 14;

  protected onTooltipPositionChange_(e: Event) {
    const position = (e.target as HTMLSelectElement).value;
    this.tooltipPosition_ = position as TooltipPosition;
    this.shadowRoot.querySelectorAll('cr-tooltip')
        .forEach(tooltip => tooltip.updatePosition());
  }

  protected onTooltipOffsetInput_(e: Event) {
    const offset = Number((e.target as CrInputElement).value);
    this.tooltipOffset_ = offset;
    this.shadowRoot.querySelectorAll('cr-tooltip')
        .forEach(tooltip => tooltip.updatePosition());
  }

  protected hide_() {
    this.$.manualTooltip.hide();
  }

  protected showAtTarget1_() {
    this.$.manualTooltip.target = this.$.target1;
    this.$.manualTooltip.updatePosition();
    this.$.manualTooltip.show();
  }

  protected showAtTarget2_() {
    this.$.manualTooltip.target = this.$.target2;
    this.$.manualTooltip.updatePosition();
    this.$.manualTooltip.show();
  }
}

export const tagName = CrTooltipDemoElement.is;

customElements.define(
    CrTooltipDemoElement.is, CrTooltipDemoElement);
