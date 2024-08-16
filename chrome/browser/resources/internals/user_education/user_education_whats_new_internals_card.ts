// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';

import {assert} from '//resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {WhatsNewEditionDemoPageInfo, WhatsNewModuleDemoPageInfo} from './user_education_internals.mojom-webui.js';
import {getCss} from './user_education_whats_new_internals_card.css.js';
import {getHtml} from './user_education_whats_new_internals_card.html.js';

const CLEAR_WHATS_NEW_DATA_EVENT = 'clear-whats-new-data';

export class UserEducationWhatsNewInternalsCardElement extends CrLitElement {
  static get is() {
    return 'user-education-whats-new-internals-card';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      item: {type: Object},
      type: {type: String},

      /**
       * Indicates if the list of promo data is expanded or collapsed.
       */
      dataExpanded_: {
        type: Boolean,
        value: false,
      },
    };
  }

  item: WhatsNewModuleDemoPageInfo|WhatsNewEditionDemoPageInfo|null = null;
  type: 'module'|'edition'|null = null;
  protected dataExpanded_: boolean = false;

  protected clearData_() {
    if (confirm(
            'Clear all What\'s New data?\n' +
            'This affects module order and edition use data.')) {
      this.dispatchEvent(new CustomEvent(
          CLEAR_WHATS_NEW_DATA_EVENT, {bubbles: true, composed: true}));
    }
  }

  protected isModule_() {
    return this.type === 'module';
  }

  protected isEdition_() {
    return this.type === 'edition';
  }

  protected hasBeenUsed_() {
    assert(this.item);
    const hasBeenUsed = (this.item as WhatsNewEditionDemoPageInfo).hasBeenUsed;
    return hasBeenUsed;
  }

  protected formatName_() {
    assert(this.item);
    if (this.isModule_()) {
      return (this.item as WhatsNewModuleDemoPageInfo).moduleName;
    } else {
      return (this.item as WhatsNewEditionDemoPageInfo).editionName;
    }
  }

  protected formatHasBrowserCommand_() {
    assert(this.item);
    const hasBrowserCommand =
        (this.item as WhatsNewModuleDemoPageInfo).hasBrowserCommand;
    return hasBrowserCommand ? 'yes' : 'no';
  }

  protected formatIsFeatureEnabled_() {
    assert(this.item);
    return (this.item as
            (WhatsNewModuleDemoPageInfo | WhatsNewEditionDemoPageInfo))
               .isFeatureEnabled ?
        'yes' :
        'no';
  }

  protected formatQueuePosition_() {
    assert(this.item);
    const queuePosition =
        (this.item as WhatsNewModuleDemoPageInfo).queuePosition;
    return queuePosition === -1 ? 'Not in queue' : queuePosition;
  }

  protected formatHasBeenUsed_() {
    assert(this.item);
    const hasBeenUsed = (this.item as WhatsNewEditionDemoPageInfo).hasBeenUsed;
    return hasBeenUsed ? 'yes' : 'no';
  }

  protected formatVersionUsed_() {
    assert(this.item);
    const versionUsed = (this.item as WhatsNewEditionDemoPageInfo).versionUsed;
    return versionUsed;
  }

  protected onDataExpandedChanged_(e: CustomEvent<{value: boolean}>) {
    this.dataExpanded_ = e.detail.value;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'user-education-whats-new-internals-card':
        UserEducationWhatsNewInternalsCardElement;
  }
}

customElements.define(
    UserEducationWhatsNewInternalsCardElement.is,
    UserEducationWhatsNewInternalsCardElement);
