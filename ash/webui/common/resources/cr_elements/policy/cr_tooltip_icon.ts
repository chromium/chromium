// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Forked from ui/webui/resources/cr_elements/policy/cr_tooltip_icon.ts

import '../icons.html.js';
import '../cr_shared_style.css.js';
import '../cr_shared_vars.css.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_tooltip_icon.html.js';

export interface CrTooltipIconElement {
  $: {
    indicator: HTMLElement,
  };
}

export class CrTooltipIconElement extends PolymerElement {
  static get is() {
    return 'cr-tooltip-icon';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      iconAriaLabel: String,
      iconClass: String,
      tooltipText: String,

      /** Position of tooltip popup related to the icon. */
      tooltipPosition: {
        type: String,
        value: 'top',
      },
    };
  }

  iconAriaLabel: string;
  iconClass: string;
  tooltipText: string;
  tooltipPosition: string;

  getFocusableElement(): HTMLElement {
    return this.$.indicator;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-tooltip-icon': CrTooltipIconElement;
  }
}

customElements.define(CrTooltipIconElement.is, CrTooltipIconElement);
