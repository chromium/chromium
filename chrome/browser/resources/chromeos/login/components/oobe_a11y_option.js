// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/chromeos/cros_color_overrides.css.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import './common_styles/oobe_common_styles.css.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
export class OobeA11yOption extends PolymerElement {
  static get is() {
    return 'oobe-a11y-option';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
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

  focus() {
    this.$.button.focus();
  }
}

customElements.define(OobeA11yOption.is, OobeA11yOption);
