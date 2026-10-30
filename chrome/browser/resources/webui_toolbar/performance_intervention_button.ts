// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {TrackedElementManager} from '//resources/js/tracked_element/tracked_element_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {HelpBubbleAnchorMixin, HighlightTracker} from '/shared/toolbar_button.js';
import type {PerformanceInterventionControlState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './performance_intervention_button.css.js';
import {getHtml} from './performance_intervention_button.html.js';

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
      isActive_: {
        type: Boolean,
        reflect: true,
        attribute: 'is-active',
      },
    };
  }

  accessor state: PerformanceInterventionControlState = {
    shouldBeShown: false,
  };

  // Whether the button is in its active (highlighted icon color) state. Set
  // when the button transitions to shown and cleared when clicked.
  protected accessor isActive_: boolean = false;

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  highlightTracker: HighlightTracker = new HighlightTracker();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('state')) {
      const oldState = changedProperties.get('state');
      if (this.state.shouldBeShown && !oldState?.shouldBeShown) {
        this.isActive_ = true;
      }
    }
  }

  protected getLabel_(): string {
    return loadTimeData.getString('performanceInterventionButtonAccName');
  }
  protected getTooltip_(): string {
    return this.adjustTooltipForHelpBubble(
        loadTimeData.getString('performanceInterventionButtonTooltip'));
  }

  protected onClick_(e: PointerEvent) {
    this.isActive_ = false;
    if (!this.highlightTracker.shouldSkipClick(e)) {
      TrackedElementManager.getInstance().notifyElementActivated(this);
      this.browserProxy_.toolbarUIHandler
          .onPerformanceInterventionButtonClicked();
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
