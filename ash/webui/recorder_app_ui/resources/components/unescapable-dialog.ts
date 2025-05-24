// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-image.js';

import {
  createRef,
  css,
  html,
  nothing,
  PropertyDeclarations,
  PropertyValues,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';

/**
 * A dialog that cannot be closed by pressing ESC or losing focus.
 */
export class UnescapableDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
      height: fit-content;
      max-height: 600px;
      max-width: 512px;
      min-width: 296px;
      width: fit-content;

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        /* Want at least 32px left/right margin. */
        max-width: min(512px, 100vw - 64px);
      }
    }

    #dialog {
      background: var(--cros-sys-dialog_container);
      border: none;
      border-radius: 20px;
      box-shadow: var(--cros-sys-app_elevation3);
      color: var(--cros-sys-on_surface);
      display: flex;
      flex-flow: column;
      height: inherit;
      max-height: inherit;
      max-width: inherit;
      min-width: inherit;
      padding: 0;
      width: inherit;

      &::backdrop {
        background: var(--cros-sys-scrim);
        pointer-events: none;
      }

      /**
       * Turn off display when dialog is not open, this ensures dialog is
       * invisible regardless of its stacked order. (crbug.com/391018463)
       */
      &:not(:popover-open) {
        display: none;
      }
    }

    #illust {
      align-items: center;
      background: var(--cros-sys-highlight_shape);
      display: flex;
      height: 236px;
      justify-content: center;
      overflow: hidden;
      width: 100%;
    }

    #content {
      display: flex;
      flex: 1;
      flex-flow: column;
      min-height: 0;
      padding: 32px 32px 28px;
    }

    #header {
      font: var(--cros-display-7-font);
      margin: 0 0 16px;
    }

    slot[name="description"]::slotted(*) {
      color: var(--cros-sys-on_surface_variant);
      font: var(--cros-body-1-font);
      flex: 1;
      margin-bottom: 32px;
      overflow-y: auto;
    }

    slot[name="actions"]::slotted(*) {
      display: flex;
      flex-flow: row;
      gap: 8px;
      justify-content: right;
    }
  `;

  static override properties: PropertyDeclarations = {
    illustrationName: {type: String},
    header: {type: String},
    open: {type: Boolean},
  };

  private readonly dialog = createRef<HTMLDivElement>();

  private readonly illustrationName: string|null = null;

  private readonly header = '';

  /**
   * Whether the dialog is opened.
   */
  open = false;

  show(): void {
    this.dialog.value?.showPopover();
  }

  hide(): void {
    this.dialog.value?.hidePopover();
  }

  private renderIllustration() {
    if (this.illustrationName === null) {
      return nothing;
    }
    return html`
      <div id="illust">
        <cra-image .name=${this.illustrationName}></cra-image>
      </div>
    `;
  }

  override updated(changedProperties: PropertyValues<this>): void {
    if (changedProperties.has('open')) {
      if (this.open) {
        this.show();
      } else {
        this.hide();
      }
    }
  }

  override render(): RenderResult {
    // We can't use <dialog> / <md-dialog> / <cra-dialog> here since the dialog
    // is always cancelable by pressing ESC, and the onboarding flow should not
    // be cancelable.
    // See https://issues.chromium.org/issues/346597066.
    return html`
      <div
        id="dialog"
        popover="manual"
        ?inert=${!this.open}
        aria-labelledby="header"
        role="dialog"
        ${ref(this.dialog)}
      >
        ${this.renderIllustration()}
        <div id="content">
          <h2 id="header">${this.header}</h2>
          <slot name="description"></slot>
          <slot name="actions"></slot>
        </div>
      </div>
    `;
  }
}

window.customElements.define('unescapable-dialog', UnescapableDialog);

declare global {
  interface HTMLElementTagNameMap {
    'unescapable-dialog': UnescapableDialog;
  }
}
