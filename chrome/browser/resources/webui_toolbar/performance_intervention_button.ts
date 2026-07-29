// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PerformanceInterventionControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './performance_intervention_button.css.js';
import {getHtml} from './performance_intervention_button.html.js';
import {HelpBubbleAnchorMixin} from './toolbar_button.js';

const PerformanceInterventionButtonElementBase =
    HelpBubbleAnchorMixin(CrLitElement);

export class PerformanceInterventionButtonElement extends
    PerformanceInterventionButtonElementBase {
  static get is() {
    return 'performance-intervention-button';
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
    };
  }

  accessor state: PerformanceInterventionControlState = {
    shouldBeShown: false,
    isActive: true,
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  protected getLabel_(): string {
    return loadTimeData.getString('performanceInterventionButtonAccName');
  }
  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        loadTimeData.getString('performanceInterventionButtonTooltip'));
  }

  protected onClick_(e: PointerEvent) {
    this.browserProxy_.toolbarUIHandler.onPerformanceInterventionButtonClicked(
        e.pointerType !== '');
  }

  protected onPointerdown_(e: PointerEvent) {
    if (e.button === 0) {
      this.browserProxy_.toolbarUIHandler
          .onPerformanceInterventionButtonMousePressed();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'performance-intervention-button': PerformanceInterventionButtonElement;
  }
}

customElements.define(
    PerformanceInterventionButtonElement.is,
    PerformanceInterventionButtonElement);
