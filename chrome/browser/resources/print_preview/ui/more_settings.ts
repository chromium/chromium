// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import '../strings.m.js';

import type {CrExpandButtonElement} from 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsContext, PrintSettingsUiBucket} from '../metrics.js';

import {getTemplate} from './more_settings.html.js';

interface PrintPreviewMoreSettingsElement {
  $: {
    label: HTMLElement,
  };
}


class PrintPreviewMoreSettingsElement extends PolymerElement {
  static get is() {
    return 'print-preview-more-settings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      settingsExpandedByUser: {
        type: Boolean,
        notify: true,
      },

      disabled: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  settingsExpandedByUser: boolean;
  disabled: boolean;
  private metrics_: MetricsContext = MetricsContext.printSettingsUi();

  /**
   * Toggles the expand button within the element being listened to.
   */
  private toggleExpandButton_(e: Event) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if ((e.target as HTMLElement).tagName === expandButtonTag) {
      return;
    }

    if (!(e.currentTarget as HTMLElement).hasAttribute('actionable')) {
      return;
    }

    const expandButton: CrExpandButtonElement =
        (e.currentTarget as HTMLElement).querySelector(expandButtonTag)!;
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
    this.metrics_.record(
        this.settingsExpandedByUser ?
            PrintSettingsUiBucket.MORE_SETTINGS_CLICKED :
            PrintSettingsUiBucket.LESS_SETTINGS_CLICKED);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-more-settings': PrintPreviewMoreSettingsElement;
  }
}

customElements.define(
    PrintPreviewMoreSettingsElement.is, PrintPreviewMoreSettingsElement);
