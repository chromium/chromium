// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MdDialog} from 'chrome://resources/mwc/@material/web/dialog/dialog.js';
import {
  DialogAnimation,
} from 'chrome://resources/mwc/@material/web/dialog/internal/animations.js';
import {css} from 'chrome://resources/mwc/lit/index.js';

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
        --md-dialog-content-font: var(--cros-body-1-font-family);
        --md-dialog-content-line-height: var(--cros-body-1-line-height);
        --md-dialog-content-size: var(--cros-body-1-font-size);
        --md-dialog-content-weight: var(--cros-body-1-font-weight);
        --md-dialog-headline-font: var(--cros-display-7-font-family);
        --md-dialog-headline-line-height: var(--cros-display-7-line-height);
        --md-dialog-headline-size: var(--cros-display-7-font-size);
        --md-dialog-headline-weight: var(--cros-display-7-font-weight);
      }

      .scrim {
        background: var(--cros-sys-scrim);

        /* The opacity is already included in --cros-sys-scrim */
        opacity: 1;
      }

      /* This is the element that gets animated. */
      .container::before {
        box-shadow: var(--cros-sys-app_elevation3);
      }

      .container {
        /* To not hide the box-shadow. */
        overflow: initial;
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

      slot[name="actions"]::slotted(*) {
        padding: 32px 32px 28px;
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
}

window.customElements.define('cra-dialog', CraDialog);

declare global {
  interface HTMLElementTagNameMap {
    'cra-dialog': CraDialog;
  }
}
