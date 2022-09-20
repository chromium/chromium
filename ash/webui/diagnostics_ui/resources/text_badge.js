// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './diagnostics_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * Badge style class type.
 * @enum {string}
 */
export const BadgeType = {
  ERROR: 'error',
  QUEUED: 'queued',
  RUNNING: 'running',
  STOPPED: 'stopped',
  SUCCESS: 'success',
  SKIPPED: 'skipped',
  WARNING: 'warning',
};

/**
 * @fileoverview
 * 'text-badge' displays a text-based rounded badge.
 */

/** @polymer */
export class TextBadgeElement extends PolymerElement {
  static get is() {
    return 'text-badge';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {!BadgeType} */
      badgeType: {
        type: String,
        value: BadgeType.QUEUED,
      },

      /** @type {string} */
      value: {
        type: String,
        value: '',
      },

      /** @type {boolean} */
      hidden: {
        type: Boolean,
        value: false,
      },

    };
  }
}

customElements.define(TextBadgeElement.is, TextBadgeElement);
