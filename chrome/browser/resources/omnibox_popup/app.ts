// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {AutocompleteResult, OmniboxPopupSelection, PageCallbackRouter} from '//resources/cr_components/searchbox/searchbox.mojom-webui.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import {assert} from '//resources/js/assert.js';
import {MetricsReporterImpl} from '//resources/js/metrics_reporter/metrics_reporter.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';

// 675px ~= 449px (--cr-realbox-primary-side-min-width) * 1.5 + some margin.
const canShowSecondarySideMediaQueryList =
    window.matchMedia('(min-width: 675px)');

export interface OmniboxPopupAppElement {
  $: {
    matches: SearchboxDropdownElement,
  };
}

// Displays the autocomplete matches in the autocomplete result.
export class OmniboxPopupAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-popup-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * Whether the secondary side can be shown based on the feature state and
       * the width available to the dropdown.
       */
      canShowSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      /*
       * Whether the secondary side is currently available to be shown.
       */
      hasSecondarySide: {
        type: Boolean,
        reflect: true,
      },

      result_: {type: Object},
    };
  }

  accessor canShowSecondarySide: boolean =
      canShowSecondarySideMediaQueryList.matches;
  accessor hasSecondarySide: boolean = false;
  protected accessor result_: AutocompleteResult|null = null;

  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;
  private selectionChangedListenerId_: number|null = null;

  constructor() {
    super();
    this.callbackRouter_ = SearchboxBrowserProxy.getInstance().callbackRouter;
    ColorChangeUpdater.forDocument().start();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged_.bind(this));
    this.selectionChangedListenerId_ =
        this.callbackRouter_.updateSelection.addListener(
            this.onUpdateSelection_.bind(this));
    canShowSecondarySideMediaQueryList.addEventListener(
        'change', this.onCanShowSecondarySideChanged_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    assert(this.autocompleteResultChangedListenerId_);
    this.callbackRouter_.removeListener(
        this.autocompleteResultChangedListenerId_);
    assert(this.selectionChangedListenerId_);
    this.callbackRouter_.removeListener(this.selectionChangedListenerId_);
    canShowSecondarySideMediaQueryList.removeEventListener(
        'change', this.onCanShowSecondarySideChanged_.bind(this));
  }

  private onCanShowSecondarySideChanged_(e: MediaQueryListEvent) {
    this.canShowSecondarySide = e.matches;
  }

  private onAutocompleteResultChanged_(result: AutocompleteResult) {
    this.result_ = result;

    if (result.matches[0]?.allowedToBeDefaultMatch) {
      this.$.matches.selectFirst();
    } else if (this.$.matches.selectedMatchIndex >= result.matches.length) {
      this.$.matches.unselect();
    }
  }

  protected onResultRepaint_() {
    const metricsReporter = MetricsReporterImpl.getInstance();
    metricsReporter.measure('ResultChanged')
        .then(
            duration => metricsReporter.umaReportTime(
                'WebUIOmnibox.ResultChangedToRepaintLatency.ToPaint', duration))
        .then(() => metricsReporter.clearMark('ResultChanged'))
        // Ignore silently if mark 'ResultChanged' is missing.
        .catch(() => {});
  }

  private onUpdateSelection_(
      oldSelection: OmniboxPopupSelection, selection: OmniboxPopupSelection) {
    this.$.matches.updateSelection(oldSelection, selection);
  }

  protected onHasSecondarySideChanged_(e: CustomEvent<{value: boolean}>) {
    this.hasSecondarySide = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-popup-app': OmniboxPopupAppElement;
  }
}

customElements.define(OmniboxPopupAppElement.is, OmniboxPopupAppElement);
