// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-sandbox-interest-item' is the custom element to show a topics or
 * fledge interest in the privacy sandbox.
 */
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import '../settings.js';

import {assert} from 'chrome://resources/js/assert.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData, PrivacySandboxInterest} from '../settings.js';

import {getTemplate} from './interest_item.html.js';

export class PrivacySandboxInterestItemElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-interest-item';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      model: Object,
    };
  }

  model: PrivacySandboxInterest;

  private getDisplayString_(): string {
    if (this.model.topic !== undefined) {
      assert(!this.model.site);
      return this.model.topic.displayString;
    } else {
      assert(!this.model.topic);
      return this.model.site!;
    }
  }

  private getButtonLabel_(): string {
    return loadTimeData.getString(this.model.removed ? 'add' : 'remove');
  }

  private onInterestChanged_(e: Event) {
    e.stopPropagation();
    this.dispatchEvent(new CustomEvent(
        'interest-changed',
        {bubbles: true, composed: true, detail: this.model}));
  }
}

customElements.define(
    PrivacySandboxInterestItemElement.is, PrivacySandboxInterestItemElement);
