// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A badge component that is used to denote additional information
 * or an updated state and is displayed inside of a cr-url-list-item.
 */

import '//resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './sp_list_item_badge.html.js';

export class SpListItemBadge extends PolymerElement {
  static get is() {
    return 'sp-list-item-badge';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      actionable: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
        observer: 'onActionableChanged_',
      },
      updated: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },
    };
  }

  actionable: boolean;
  updated: boolean;

  private onActionableChanged_() {
    if (this.actionable) {
      this.setAttribute('tabindex', '1');
      this.setAttribute('role', 'button');
    } else {
      this.removeAttribute('tabindex');
      this.removeAttribute('role');
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'sp-list-item-badge': SpListItemBadge;
  }
}

customElements.define(SpListItemBadge.is, SpListItemBadge);
