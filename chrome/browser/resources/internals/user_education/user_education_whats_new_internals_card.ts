// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import type {WhatsNewEditionDemoPageInfo, WhatsNewModuleDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {getTemplate} from './user_education_whats_new_internals_card.html.js';

const CLEAR_WHATS_NEW_DATA_EVENT = 'clear-whats-new-data';

class UserEducationWhatsNewInternalsCardElement extends PolymerElement {
  static get is() {
    return 'user-education-whats-new-internals-card';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      item: Object,

      type: {
        type: String,
      },

      /**
       * Indicates if the list of promo data is expanded or collapsed.
       */
      dataExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  item: WhatsNewModuleDemoPageInfo|WhatsNewEditionDemoPageInfo;
  type: 'module'|'edition';
  private dataExpanded_: boolean;

  private clearData_() {
    if (confirm(
            'Clear all What\'s New data?\n' +
            'This affects module order and edition use data.')) {
      this.dispatchEvent(new CustomEvent(
          CLEAR_WHATS_NEW_DATA_EVENT, {bubbles: true, composed: true}));
    }
  }

  private isModule_() {
    return this.type === 'module';
  }

  private isEdition_() {
    return this.type === 'edition';
  }

  private formatHasBrowserCommand_() {
    const hasBrowserCommand =
        (this.item as WhatsNewModuleDemoPageInfo).hasBrowserCommand;
    return hasBrowserCommand ? 'yes' : 'no';
  }

  private formatIsFeatureEnabled_() {
    return this.item.isFeatureEnabled ? 'yes' : 'no';
  }

  private formatQueuePosition_() {
    const queuePosition =
        (this.item as WhatsNewModuleDemoPageInfo).queuePosition;
    return queuePosition === -1 ? 'Not in queue' : queuePosition;
  }

  private formatHasBeenUsed_() {
    const hasBeenUsed = (this.item as WhatsNewEditionDemoPageInfo).hasBeenUsed;
    return hasBeenUsed ? 'yes' : 'no';
  }
}

customElements.define(
    UserEducationWhatsNewInternalsCardElement.is,
    UserEducationWhatsNewInternalsCardElement);
