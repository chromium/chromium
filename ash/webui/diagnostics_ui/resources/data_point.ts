// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome://resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';
import './diagnostics_shared.css.js';
import './icons.html.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './data_point.html.js';

/**
 * @fileoverview
 * 'data-point' shows a single piece of information related to a component. It
 *  consists of a header, value, and tooltip that provides context about the
 *  item.
 */

export class DataPointElement extends PolymerElement {
  static get is(): string {
    return 'data-point';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      header: {
        type: String,
      },

      value: {
        type: String,
        value: '',
      },

      tooltipText: {
        type: String,
        value: '',
      },

      warningState: {
        type: Boolean,
        value: false,
      },

      /**
       * The alignment of the data point on the screen (vertical or horizontal).
       */
      orientation: {
        type: String,
        value: 'vertical',
        reflectToAttribute: true,
      },

    };
  }

  header: string;
  value: string;
  tooltipText: string;
  warningState: boolean;
  orientation: string;

  protected getValueClass(): string {
    return this.warningState ? 'value text-red' : 'value';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'data-point': DataPointElement;
  }
}

customElements.define(DataPointElement.is, DataPointElement);
