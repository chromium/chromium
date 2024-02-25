// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A link row is a UI element similar to a button, though usually wider than a
 * button (taking up the whole 'row'). The name link comes from the intended use
 * of this element to take the user to another page in the app or to an external
 * page (somewhat like an HTML link).
 * Forked from
 * ui/webui/resources/cr_elements/cr_link_row/cr_link_row.ts
 */
import '../cr_actionable_row_style.css.js';
import '../cr_icon_button/cr_icon_button.js';
import '../cr_hidden_style.css.js';
import '../icons.html.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrIconButtonElement} from '../cr_icon_button/cr_icon_button.js';

import {getTemplate} from './cr_link_row.html.js';

export interface CrLinkRowElement {
  $: {
    icon: CrIconButtonElement,
    buttonAriaDescription: HTMLElement,
  };
}

export class CrLinkRowElement extends PolymerElement {
  static get is() {
    return 'cr-link-row';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      ariaShowLabel: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      ariaShowSublabel: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      startIcon: {
        type: String,
        value: '',
      },

      label: {
        type: String,
        value: '',
      },

      subLabel: {
        type: String,
        /* Value used for noSubLabel attribute. */
        value: '',
      },

      disabled: {
        type: Boolean,
        reflectToAttribute: true,
      },

      external: {
        type: Boolean,
        value: false,
      },

      usingSlottedLabel: {
        type: Boolean,
        value: false,
      },

      roleDescription: String,

      buttonAriaDescription: String,

      hideLabelWrapper_: {
        type: Boolean,
        computed: 'computeHideLabelWrapper_(label, usingSlottedLabel)',
      },
    };
  }

  ariaShowLabel: boolean;
  ariaShowSublabel: boolean;
  startIcon: string;
  label: string;
  subLabel: string;
  disabled: boolean;
  external: boolean;
  usingSlottedLabel: boolean;
  roleDescription: string;
  buttonAriaDescription: string;
  private hideLabelWrapper_: boolean;

  override focus() {
    this.$.icon.focus();
  }

  private computeHideLabelWrapper_(): boolean {
    return !(this.label || this.usingSlottedLabel);
  }

  private getIcon_(): string {
    return this.external ? 'cr:open-in-new' : 'cr:arrow-right';
  }

  private computeButtonAriaDescription_(
      external: boolean, buttonAriaDescription?: string): string {
    return buttonAriaDescription ??
        (external ? loadTimeData.getString('opensInNewTab') : '');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-link-row': CrLinkRowElement;
  }
}

customElements.define(CrLinkRowElement.is, CrLinkRowElement);
