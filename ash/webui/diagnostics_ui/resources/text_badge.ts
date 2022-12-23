// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared.css.js';

import {PolymerElementProperties} from 'chrome://resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './text_badge.html.js';

/**
 * Badge style class type.
 * @enum {string}
 */
export enum BadgeType {
  ERROR = 'error',
  QUEUED = 'queued',
  RUNNING = 'running',
  STOPPED = 'stopped',
  SUCCESS = 'success',
  SKIPPED = 'skipped',
  WARNING = 'warning',
}

/**
 * @fileoverview
 * 'text-badge' displays a text-based rounded badge.
 */

export class TextBadgeElement extends PolymerElement {
  static get is(): string {
    return 'text-badge';
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      badgeType: {
        type: String,
        value: BadgeType.QUEUED,
      },

      value: {
        type: String,
        value: '',
      },

      hidden: {
        type: Boolean,
        value: false,
      },
    };
  }

  badgeType: BadgeType;
  value: string;
  override hidden: boolean;
}

declare global {
  interface HTMLElementTagNameMap {
    'text-badge': TextBadgeElement;
  }
}


customElements.define(TextBadgeElement.is, TextBadgeElement);
