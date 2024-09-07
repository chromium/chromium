// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra-dialog.js';
import './cra-image.js';

import {
  createRef,
  css,
  html,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../../core/reactive/lit.js';

import {CraDialog} from './cra-dialog.js';

/**
 * Dialog with an illustration on top, matching the "feature tour" style of the
 * cros dialog spec.
 */
export class CraFeatureTourDialog extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
    }

    cra-dialog {
      height: inherit;

      /* Want at least 80px left/right margin. */
      max-width: min(512px, 100vw - 160px);

      /* From CrOS dialog style. Min width for Recorder App is 480px. */
      @media (width < 520px) {
        width: 360px;
      }
    }

    #header {
      align-items: stretch;
      display: flex;
      flex-flow: column;
      gap: 0;
      overflow: hidden;
      padding: 0;

      & > span {
        padding: 32px 32px 0;
      }
    }

    #illust {
      align-items: center;
      background: var(--cros-sys-highlight_shape);

      /*
       * Due to how the md-dialog put the box-shadow inside the container
       * element instead of on the container element, having overflow: hidden on
       * the whole dialog will also hide the box shadow, so a separate top
       * rounded border is needed here to hide extra background.
       */
      border-radius: 20px 20px 0 0;
      display: flex;
      height: 236px;
      justify-content: center;
    }
  `;

  static override properties: PropertyDeclarations = {
    illustrationName: {type: String},
    header: {type: String},
  };

  private readonly dialog = createRef<CraDialog>();

  private readonly illustrationName: string|null = null;

  private readonly header = '';

  async show(): Promise<void> {
    await this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  override render(): RenderResult {
    return html`<cra-dialog ${ref(this.dialog)}>
      <div slot="headline" id="header">
        <div id="illust">
          <cra-image .name=${this.illustrationName}></cra-image>
        </div>
        <span>${this.header}</span>
      </div>
      <slot name="content" slot="content"></slot>
      <slot name="actions" slot="actions"> </slot>
    </cra-dialog>`;
  }
}

window.customElements.define('cra-feature-tour-dialog', CraFeatureTourDialog);

declare global {
  interface HTMLElementTagNameMap {
    'cra-feature-tour-dialog': CraFeatureTourDialog;
  }
}
