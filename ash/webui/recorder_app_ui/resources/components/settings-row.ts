// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  Switch as CrosSwitch,
} from 'chrome://resources/cros_components/switch/switch.js';
import {classMap, css, html} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {stopPropagation} from '../core/utils/event_handler.js';

/**
 * A row in settings menu for Recording app.
 */
export class SettingsRow extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    #root {
      --background-color: var(--cros-sys-app_base);

      align-items: center;
      background: var(--background-color);
      box-sizing: border-box;
      display: flex;
      flex-flow: row;
      height: 64px;
      padding: 12px 20px;

      @container style(--dark-theme: 1) {
        --background-color: var(--cros-sys-surface1);
      }

      &.interactive {
        cursor: pointer;

        &:hover {
          /* Use linear-gradient to apply multiple background colors. */
          background: linear-gradient(
              0deg,
              var(--cros-sys-hover_on_subtle),
              var(--cros-sys-hover_on_subtle)
            ),
            var(--background-color);
        }

        /* TODO: b/336963138 - Should we have a ripple on the whole row? */
        &:active {
          /* Use linear-gradient to apply multiple background colors. */
          background: linear-gradient(
              0deg,
              var(--cros-sys-ripple_neutral_on_subtle),
              var(--cros-sys-ripple_neutral_on_subtle)
            ),
            var(--background-color);
        }
      }
    }

    #left {
      align-items: flex-start;
      display: flex;
      flex: 1;
      flex-flow: column;
      font: var(--cros-body-2-font);

      & > slot[name="label"] {
        color: var(--cros-sys-on_surface);
      }

      & > slot[name="description"] {
        color: var(--cros-sys-on_surface_variant);
      }
    }
  `;

  private readonly interactive = signal(false);

  private getSlottedSwitch(): CrosSwitch|null {
    // We're getting the slotted item via querySelector in the light DOM
    // instead of the shadow DOM, so no .shadowRoot is needed here.
    return this.querySelector('cros-switch[slot="action"]');
  }

  private onClick() {
    const crosSwitch = this.getSlottedSwitch();
    crosSwitch?.dispatchEvent(new Event('click'));
  }

  private onSlotChange() {
    this.interactive.value = this.getSlottedSwitch() !== null;
  }

  override render(): RenderResult {
    const classes = {
      interactive: this.interactive.value,
    };
    return html`<div
      id="root"
      class=${classMap(classes)}
      @click=${this.onClick}
    >
      <div id="left">
        <slot name="label"></slot>
        <slot name="description"></slot>
      </div>
      <slot
        name="action"
        @click=${stopPropagation}
        @slotchange=${this.onSlotChange}
      ></slot>
      <slot name="status"></slot>
    </div>`;
  }
}

window.customElements.define('settings-row', SettingsRow);

declare global {
  interface HTMLElementTagNameMap {
    'settings-row': SettingsRow;
  }
}
