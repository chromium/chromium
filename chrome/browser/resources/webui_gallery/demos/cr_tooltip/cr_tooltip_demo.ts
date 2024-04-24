// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_input/cr_input.js';
import '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import '//resources/cr_elements/md_select.css.js';
import '../demo.css.js';

import {getTemplate} from './cr_tooltip_demo.html.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrInputElement} from '//resources/cr_elements/cr_input/cr_input.js';
import type {CrTooltipElement} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';
import {TooltipPosition} from '//resources/cr_elements/cr_tooltip/cr_tooltip.js';

interface CrTooltipDemoElement {
  $: {
    manualTooltip: CrTooltipElement,
    target1: HTMLElement,
    target2: HTMLElement,
  };
}

class CrTooltipDemoElement extends PolymerElement {
  static get is() {
    return 'cr-tooltip-demo';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      tooltipPosition_: String,
      tooltipOffset_: Number,
    };
  }

  // Default values.
  private tooltipPosition_: TooltipPosition = TooltipPosition.BOTTOM;
  private tooltipOffset_: number = 14;

  private onTooltipPositionChange_(e: Event) {
    const position = (e.target as HTMLSelectElement).value;
    this.tooltipPosition_ = position as TooltipPosition;
    this.shadowRoot!.querySelectorAll('cr-tooltip').forEach(
        tooltip => tooltip.updatePosition());
  }

  private onTooltipOffsetInput_(e: Event) {
    const offset = Number((e.target as CrInputElement).value);
    this.tooltipOffset_ = offset;
    this.shadowRoot!.querySelectorAll('cr-tooltip').forEach(
        tooltip => tooltip.updatePosition());
  }

  private hide_() {
    this.$.manualTooltip.hide();
  }

  private showAtTarget1_() {
    this.$.manualTooltip.target = this.$.target1;
    this.$.manualTooltip.updatePosition();
    this.$.manualTooltip.show();
  }

  private showAtTarget2_() {
    this.$.manualTooltip.target = this.$.target2;
    this.$.manualTooltip.updatePosition();
    this.$.manualTooltip.show();
  }
}

export const tagName = CrTooltipDemoElement.is;

customElements.define(
    CrTooltipDemoElement.is, CrTooltipDemoElement);
