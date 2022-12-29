// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/omnibox/realbox_dropdown.js';
import './strings.m.js';

import {PageCallbackRouter, RealboxBrowserProxy} from '//resources/cr_components/omnibox/realbox_browser_proxy.js';
import {assert} from '//resources/js/assert_ts.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './app.html.js';
import {AutocompleteResult} from './omnibox.mojom-webui.js';

// Displays the autocomplete matches in the autocomplete result.
export class OmniboxPopupAppElement extends PolymerElement {
  static get is() {
    return 'omnibox-popup-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      result_: Object,
    };
  }

  private callbackRouter_: PageCallbackRouter;
  private omniboxAutocompleteResultChangedListenerId_: number|null = null;
  private result_: AutocompleteResult;

  constructor() {
    super();
    this.callbackRouter_ = RealboxBrowserProxy.getInstance().callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.omniboxAutocompleteResultChangedListenerId_ =
        this.callbackRouter_.omniboxAutocompleteResultChanged.addListener(
            this.onOmniboxAutocompleteResultChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.omniboxAutocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.omniboxAutocompleteResultChangedListenerId_);
  }

  private onOmniboxAutocompleteResultChanged_(result: AutocompleteResult) {
    this.result_ = result;
  }

  private onResultRepaint_() {
    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('ResultChanged')
        .then(
            duration => metricsReporter.umaReportTime(
                'WebUIOmnibox.ResultChangedToRepaintLatency.ToPaint', duration))
        .then(() => metricsReporter.clearMark('ResultChanged'))
        // Ignore silently if mark 'ResultChanged' is missing.
        .catch(() => {});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
