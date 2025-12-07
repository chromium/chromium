// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Button} from 'chrome://resources/cros_components/button/button.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

export class CraButton extends Button {
  static override styles = [
    Button.styles,
    // TODO: b/338544996 - This currently only allows customization if the
    // underlying is md-filled-button.
    // TODO: b/338544996 - This currently only allows customization if the
    // button-style is "primary" (the default value).
    // TODO: b/338544996 - Overriding things this way is pretty unclean since
    // we can't properly fallback to the parent value, so we have to hard-coded
    // the default value from cros-button here. We should submit these
    // customization options to upstream instead.
    css`
      md-filled-button {
        --md-filled-button-container-height: var(
          --cra-button-container-height,
          36px
        );
        --md-filled-button-label-text-font-family: var(
          --cra-button-label-text-font-family,
          var(--cros-button-2-font-family)
        );
        --md-filled-button-label-text-size: var(
          --cra-button-label-text-size,
          var(--cros-button-2-font-size)
        );
        --md-filled-button-label-text-line-height: var(
          --cra-button-label-text-line-height,
          var(--cros-button-2-line-height)
        );
        --md-filled-button-label-text-weight: var(
          --cra-button-label-text-weight,
          var(--cros-button-2-font-weight)
        );
        --md-filled-button-leading-space: var(--cra-button-leading-space, 16px);
        --md-filled-button-trailing-space: var(
          --cra-button-trailing-space,
          16px
        );
      }

      md-filled-button:has(.content-container.has-leading-icon) {
        --md-filled-button-leading-space: var(--cra-button-leading-space, 12px);
      }

      md-filled-button:has(.content-container.has-trailing-icon) {
        --md-filled-button-trailing-space: var(
          --cra-button-trailing-space,
          12px
        );
      }

      :host([button-style="primary"]) md-filled-button {
        --md-filled-button-hover-state-layer-color: var(
          --cra-button-hover-state-layer-color,
          var(--cros-sys-hover_on_prominent)
        );
        --md-filled-button-pressed-state-layer-color: var(
          --cra-button-pressed-state-layer-color,
          var(--cros-sys-ripple_primary)
        );
      }

      .content-container {
        gap: var(--cra-button-icon-gap, 8px);
      }
    `,
  ];

  get buttonElement(): HTMLElement|null {
    return (
      this.shadowRoot?.querySelector('md-text-button') ??
      this.shadowRoot?.querySelector('md-filled-button') ?? null
    );
  }

  override focus(): void {
    // Need to manually delegate the focus() call to the inner button,
    // otherwise the :focus-visible state won't be correct and the focus ring
    // would always be shown.
    this.buttonElement?.focus();
  }

  override blur(): void {
    this.buttonElement?.blur();
  }
}

window.customElements.define('cra-button', CraButton);

declare global {
  interface HTMLElementTagNameMap {
    'cra-button': CraButton;
  }
}
