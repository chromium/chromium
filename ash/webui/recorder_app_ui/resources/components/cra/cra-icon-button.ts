// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  IconButton,
} from 'chrome://resources/cros_components/icon_button/icon-button.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraIconButton extends IconButton {
  static override styles = [
    IconButton.styles,
    // TODO: b/338544996 - This currently only allows customization if the
    // underlying is md-filled-tonal-icon-button.
    // TODO: b/338544996 - Overriding things this way is pretty unclean since
    // we can't properly fallback to the parent value, so we have to hard-coded
    // the default value from cros-icon-button here. We should submit these
    // customization options to upstream instead.
    css`
      :host {
        /*
         * A margin is added since in the original md-button, the touch region
         * has a minimal size of 48x48, but the element size is still 40x40.
         * In cros-button the design is changed to having touch region always
         * being 4px outward of the button. But in both case the touch region
         * isn't included in the element size.
         * Since the button size used in the UI spec is almost always the
         * whole button size, it's easier to implement the spec with the 4px
         * margin added.
         *
         * TODO(pihsun): Button with [size="small"] has size of 32x32, so we'll
         * need a larger margin to achieve the same effect. Since it's usual to
         * have the touch target overlaps a bit with other things in small icon
         * button, this might not be as useful. Revisit this when we have more
         * usage.
         */
        margin: 4px;
      }

      md-filled-tonal-icon-button {
        --md-filled-tonal-icon-button-hover-state-layer-color: var(
          --cra-icon-button-hover-state-layer-color,
          var(--cros-sys-hover_on_subtle)
        );
        --md-filled-tonal-icon-button-pressed-state-layer-color: var(
          --cra-icon-button-pressed-state-layer-color,
          var(--cros-sys-ripple_neutral_on_subtle)
        );
      }

      :host([surface="base"]) md-filled-tonal-icon-button {
        --md-filled-tonal-icon-button-container-color: var(
          --cra-icon-button-container-color,
          var(--cros-sys-surface_variant)
        );
      }

      :host([size="default"]) md-filled-tonal-icon-button {
        --md-filled-tonal-icon-button-container-width: var(
          --cra-icon-button-container-width,
          40px
        );
        --md-filled-tonal-icon-button-container-height: var(
          --cra-icon-button-container-height,
          40px
        );
      }
    `,
  ];
}

window.customElements.define('cra-icon-button', CraIconButton);

declare global {
  interface HTMLElementTagNameMap {
    'cra-icon-button': CraIconButton;
  }
}
