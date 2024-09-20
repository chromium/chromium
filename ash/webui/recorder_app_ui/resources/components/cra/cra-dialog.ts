// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MdDialog} from 'chrome://resources/mwc/@material/web/dialog/dialog.js';
import {
  DialogAnimation,
} from 'chrome://resources/mwc/@material/web/dialog/internal/animations.js';
import {css, PropertyValues} from 'chrome://resources/mwc/lit/index.js';

/**
 * A dialog with ChromeOS specific style.
 */
export class CraDialog extends MdDialog {
  static override styles = [
    ...MdDialog.styles,
    css`
      :host {
        --md-dialog-container-shape: 20px;
        --md-dialog-container-color: var(--cros-sys-dialog_container);
        --md-dialog-headline-color: var(--cros-sys-on_surface);
        --md-dialog-headline-font: var(--cros-display-7-font-family);
        --md-dialog-headline-line-height: var(--cros-display-7-line-height);
        --md-dialog-headline-size: var(--cros-display-7-font-size);
        --md-dialog-headline-weight: var(--cros-display-7-font-weight);
        --md-dialog-supporting-text-color: var(--cros-sys-on_surface_variant);
        --md-dialog-supporting-text-font: var(--cros-body-1-font-family);
        --md-dialog-supporting-text-line-height: var(--cros-body-1-line-height);
        --md-dialog-supporting-text-size: var(--cros-body-1-font-size);
        --md-dialog-supporting-text-weight: var(--cros-body-1-font-weight);

        /* All dialog are "fixed" width in Recorder app. */
        max-width: none;
      }

      .scrim {
        background: var(--cros-sys-scrim);

        /* The opacity is already included in --cros-sys-scrim */
        opacity: 1;

        /*
         * The default z-index is 1, which makes it hard for anything below to
         * be ordered by z-index and not accidentally being "over" the scrim.
         * Change it to a higher value to make styling easier.
         */
        z-index: 100;
      }

      dialog {
        box-shadow: var(--cros-sys-app_elevation3);
      }

      slot[name="headline"]::slotted(*) {
        padding: 32px 32px 0;
      }

      slot[name="content"]::slotted(*) {
        padding: 16px 32px 0;
      }

      .has-actions slot[name="content"]::slotted(*) {
        padding-bottom: 0;
      }

      .scrollable.has-headline slot[name="content"]::slotted(*) {
        padding-top: 16px;
      }

      slot[name="actions"]::slotted(*) {
        padding: 32px 32px 28px;
      }

      /* CrOS dialog spec doesn't have the divider when the content scrolls. */
      md-divider {
        display: none !important;
      }
    `,
  ];

  constructor() {
    super();
    const oldOpenAnimation = this.getOpenAnimation();
    this.getOpenAnimation = (): DialogAnimation => ({
      ...oldOpenAnimation,
      scrim: [
        [
          // Scrim fade in, we have opacity in the background color directly.
          [{opacity: 0}, {opacity: 1}],
          {duration: 500, easing: 'linear'},
        ],
      ],
      container: [
        [
          // Container fade in
          [{opacity: 0}, {opacity: 1}],
          {duration: 50, easing: 'linear', pseudoElement: '::before'},
        ],
        // TODO: b/336963138 - The background grow animation is removed since
        // it doesn't look good when we have other color background.
      ],
    });
    const oldCloseAnimation = this.getCloseAnimation();
    this.getCloseAnimation = (): DialogAnimation => ({
      ...oldCloseAnimation,
      scrim: [
        [
          // Scrim fade out, we have opacity in the background color directly.
          [{opacity: 1}, {opacity: 0}],
          {duration: 150, easing: 'linear'},
        ],
      ],
      container: [
        // TODO: b/336963138 - The background shrink animation is removed since
        // it doesn't look good when we have other color background.
        [
          // Container fade out
          [{opacity: '1'}, {opacity: '0'}],
          {
            delay: 100,
            duration: 50,
            easing: 'linear',
            pseudoElement: '::before',
          },
        ],
      ],
    });
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);

    if (this.ariaLabel !== null) {
      // If aria-label is explicitly set, remove the aria-labelledby since that
      // takes precedence over aria-label.
      // TODO: b/338544996 - File a bug to md-dialog for this.
      const dialog = this.shadowRoot?.querySelector('dialog') ?? null;
      dialog?.removeAttribute('aria-labelledby');
    }
  }
}

window.customElements.define('cra-dialog', CraDialog);

declare global {
  interface HTMLElementTagNameMap {
    'cra-dialog': CraDialog;
  }
}
