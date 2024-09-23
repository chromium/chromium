// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 *   @fileoverview
 *   Material design button that shows Android phone icon and displays text to
 *   use quick start.
 *
 *   Example:
 *     <quick-start-entry-point
 *       quickStartTextkey="welcomeScreenQuickStart">
 *     </quick-start-entry-point>
 *
 *   Attributes:
 *     'quickStartTextkey' - ID of localized string to be used as button text.
 */

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {OobeI18nMixin} from './mixins/oobe_i18n_mixin.js';

import {getTemplate} from './quick_start_entry_point.html.js';

const QuickStartEntryPointBase = OobeI18nMixin(PolymerElement);

export class QuickStartEntryPoint extends QuickStartEntryPointBase {
  static get is() {
    return 'quick-start-entry-point' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      quickStartTextKey: {
        type: String,
        value: '',
      },
    };
  }

  private quickStartTextKey: string;

  quickStartButtonClicked(): void {
    this.dispatchEvent(new CustomEvent('activate-quick-start', {
      bubbles: true,
      composed: true,
      detail: {enableBluetooth: false},
    }));
  }

}

declare global {
  interface HTMLElementTagNameMap {
    [QuickStartEntryPoint.is]: QuickStartEntryPoint;
  }
}

customElements.define(QuickStartEntryPoint.is, QuickStartEntryPoint);
