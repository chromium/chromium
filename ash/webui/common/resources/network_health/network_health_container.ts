// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/ash/common/cr_elements/icons.html.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './network_health_container.html.js';

/**
 * @fileoverview Polymer element for a container used in displaying network
 * health info.
 */

export class NetworkHealthContainerElement extends PolymerElement {
  static get is() {
    return 'network-health-container' as const;
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Boolean flag if the container is expanded.
       */
      expanded: {
        type: Boolean,
        value: false,
      },

      /**
       * Container label.
       */
      label: {
        type: String,
        value: '',
      },
    };
  }

  expanded: boolean;
  label: string;

  /**
   * Returns the correct arrow icon depending on if the container is expanded.
   */
  private getArrowIcon_(): string {
    return this.expanded ? 'cr:expand-less' : 'cr:expand-more';
  }

  /**
   * Helper function to fire the toggle event when clicked.
   */
  private onClick_(): void {
    this.dispatchEvent(new CustomEvent('toggle-expanded', {
      bubbles: true,
      composed: true,
    }));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NetworkHealthContainerElement.is]: NetworkHealthContainerElement;
  }
}

customElements.define(
    NetworkHealthContainerElement.is, NetworkHealthContainerElement);
