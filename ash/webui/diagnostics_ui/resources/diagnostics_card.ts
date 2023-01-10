// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_card_frame.js';
import './diagnostics_shared.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './diagnostics_card.html.js';

/**
 * @fileoverview
 * 'diagnostics-card' is a styling wrapper for each component's diagnostic
 * card.
 */

export class DiagnosticsCardElement extends PolymerElement {
  static get is(): string {
    return 'diagnostics-card';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      hideDataPoints: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      isNetworkingCard: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

    };
  }

  hideDataPoints: boolean;
  isNetworkingCard: boolean;

  protected getTopSectionClassName(): string {
    return `top-section${this.isNetworkingCard ? '-networking' : ''}`;
  }

  protected getBodyClassName(): string {
    return `data-points${this.isNetworkingCard ? '-column' : ''}`;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'diagnostics-card': DiagnosticsCardElement;
  }
}

customElements.define(DiagnosticsCardElement.is, DiagnosticsCardElement);
