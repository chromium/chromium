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

      :host([size="default"]) md-filled-icon-button {
        --md-filled-icon-button-container-width: var(
          --cra-icon-button-container-width,
          40px
        );
        --md-filled-icon-button-container-height: var(
          --cra-icon-button-container-height,
          40px
        );
      }

      :host([buttonStyle="toggle"].with-filled-style) md-filled-icon-button {
        --md-filled-icon-button-unselected-container-color: var(
          --cra-icon-button-container-color,
          var(--cros-sys-surface_variant)
        );
        --md-filled-icon-button-selected-container-color: var(
          --cra-icon-button-container-color,
          var(--cros-sys-surface_variant)
        );
      }

      :host([buttonStyle="toggle"].with-floating-style) md-filled-icon-button {
        --md-filled-icon-button-selected-container-color: #0000;
      }

      :host([buttonStyle="toggle"].with-filled-style) md-filled-icon-button,
      :host([buttonStyle="toggle"].with-floating-style) md-filled-icon-button {
        --md-filled-icon-button-toggle-selected-focus-icon-color: var(
          --cros-sys-on_surface
        );
        --md-filled-icon-button-toggle-selected-hover-icon-color: var(
          --cros-sys-on_surface
        );
        --md-filled-icon-button-toggle-selected-hover-state-layer-color: var(
          --cros-sys-hover_on_subtle
        );
        --md-filled-icon-button-toggle-selected-icon-color: var(
          --cros-sys-on_surface
        );
        --md-filled-icon-button-toggle-selected-pressed-icon-color: var(
          --cros-sys-on_surface
        );
        --md-filled-icon-button-toggle-selected-pressed-state-layer-color: var(
          --cros-sys-ripple_neutral_on_subtle
        );
        --md-filled-icon-button-toggle-pressed-state-layer-color: var(
          --cros-sys-ripple_neutral_on_subtle
        );
      }

      :host([buttonstyle="filled"].with-toggle-style)
        md-filled-tonal-icon-button {
        --md-filled-tonal-icon-button-container-color: #0000;
        --md-filled-tonal-icon-button-pressed-state-layer-color: var(
          --cros-sys-ripple_primary
        );
      }

      :host([buttonstyle="filled"].selected.with-toggle-style)
        md-filled-tonal-icon-button {
        --md-filled-tonal-icon-button-container-color: var(
          --cros-sys-primary_container
        );
        --md-filled-tonal-icon-button-focus-icon-color: var(
          --cros-sys-on_primary_container
        );
        --md-filled-tonal-icon-button-hover-icon-color: var(
          --cros-sys-on_primary_container
        );
        --md-filled-tonal-icon-button-icon-color: var(
          --cros-sys-on_primary_container
        );
        --md-filled-tonal-icon-button-pressed-state-layer-color: var(
          --cros-sys-ripple_primary
        );
      }
    `,
  ];

  get buttonElement(): HTMLElement|null {
    return (
      this.shadowRoot?.querySelector('md-icon-button') ??
      this.shadowRoot?.querySelector('md-filled-icon-button') ??
      this.shadowRoot?.querySelector('md-filled-tonal-icon-button') ?? null
    );
  }

  get innerButtonElement(): HTMLElement|null {
    // Since the md-icon-button doesn't call focus to underlying button (unlike
    // md-button), we need to do that by ourselves.
    return this.buttonElement?.shadowRoot?.getElementById('button') ?? null;
  }

  override focus(): void {
    // Need to manually delegate the focus() call to the inner button,
    // otherwise the :focus-visible state won't be correct and the focus ring
    // would always be shown.
    this.innerButtonElement?.focus();
  }

  override blur(): void {
    this.innerButtonElement?.blur();
  }
}

window.customElements.define('cra-icon-button', CraIconButton);

declare global {
  interface HTMLElementTagNameMap {
    'cra-icon-button': CraIconButton;
  }
}
