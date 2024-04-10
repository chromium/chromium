// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/js/action_link.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './oobe_icons.html.js';
import './common_styles/oobe_common_styles.css.js';

import {GaiaButton} from './gaia_button.js';

import {assert} from '//resources/js/assert.js';

import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './notification_card.html.js';

class NotificationCard extends PolymerElement {
  static get is() {
    return 'notification-card' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      buttonLabel: {
        type: String,
        value: '',
      },
      linkLabel: {
        type: String,
        value: '',
      },
    };
  }

  private buttonLabel: string;
  private linkLabel: string;

  private buttonClicked(): void {
    this.dispatchEvent(new CustomEvent('buttonclick',
        { bubbles: true, composed: true }));
  }

  private linkClicked(e: MouseEvent): void {
    this.dispatchEvent(new CustomEvent('linkclick',
        { bubbles: true, composed: true }));
    e.preventDefault();
  }

  get submitButton(): GaiaButton {
    const button = this.shadowRoot?.querySelector('#submitButton');
    assert(button instanceof GaiaButton);
    return button;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [NotificationCard.is]: NotificationCard;
  }
}

customElements.define(NotificationCard.is, NotificationCard);
