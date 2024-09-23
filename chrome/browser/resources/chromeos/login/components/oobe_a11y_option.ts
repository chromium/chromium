// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/ash/common/cr_elements/cros_color_overrides.css.js';
import '//resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/ash/common/cr_elements/cr_shared_vars.css.js';
import './common_styles/oobe_common_styles.css.js';

import {assert} from '//resources/js/assert.js';
import {PolymerElementProperties} from '//resources/polymer/v3_0/polymer/interfaces.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './oobe_a11y_option.html.js';

export class OobeA11yOption extends PolymerElement {
  static get is() {
    return 'oobe-a11y-option' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  static get properties(): PolymerElementProperties {
    return {
      /**
       * If cr-toggle is checked.
       */
      checked: {
        type: Boolean,
      },

      /**
       * Chrome message handling this option.
       */
      chromeMessage: {
        type: String,
      },

      /**
       * ARIA-label for the button.
       *
       * Note that we are not using "aria-label" property here, because
       * we want to pass the label value but not actually declare it as an
       * ARIA property anywhere but the actual target element.
       */
      labelForAria: {
        type: String,
      },
    };
  }

  checked: boolean;
  private labelForAria: string;
  chromeMessage: string;

  override focus(): void {
    const button = this.shadowRoot?.querySelector('#button');
    assert(button instanceof HTMLElement);
    button.focus();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [OobeA11yOption.is]: OobeA11yOption;
  }
}

customElements.define(OobeA11yOption.is, OobeA11yOption);
