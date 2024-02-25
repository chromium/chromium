// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An element that lists phones usable as security keys,
    optionally with a drop-down menu for editing or deleting them.
 */
import '../settings_shared.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {AnchorAlignment} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {SecurityKeysPhone} from './security_keys_browser_proxy.js';
import {getTemplate} from './security_keys_phones_list.html.js';

class SecurityKeysPhonesListElement extends PolymerElement {
  static get is() {
    return 'security-keys-phones-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      immutable: {type: Boolean, value: false},
      phones: {type: Array, value: []},
    };
  }

  immutable: boolean;
  phones: SecurityKeysPhone[];
  // Contains the public key of the phone that the action menu was opened for.
  private publicKeyForActionMenu_: string|null;

  private onDotsClick_(e: Event) {
    this.publicKeyForActionMenu_ =
        (e.target as HTMLElement).dataset['phonePublicKey']!;

    this.shadowRoot!.querySelector('cr-action-menu')!.showAt(
        e.target as HTMLElement, {
          anchorAlignmentY: AnchorAlignment.AFTER_END,
        });
  }

  private onEditClick_(e: Event) {
    this.handleClick_(e, 'edit-security-key-phone');
  }

  private onDeleteClick_(e: Event) {
    this.handleClick_(e, 'delete-security-key-phone');
  }

  private handleClick_(
      e: Event,
      eventName: 'edit-security-key-phone'|'delete-security-key-phone') {
    e.stopPropagation();
    this.closePopupMenu_();

    this.dispatchEvent(new CustomEvent(eventName, {
      bubbles: true,
      composed: true,
      detail: this.publicKeyForActionMenu_,
    }));
  }

  private closePopupMenu_() {
    this.shadowRoot!.querySelector('cr-action-menu')!.close();
  }
}

customElements.define(
    SecurityKeysPhonesListElement.is, SecurityKeysPhonesListElement);
