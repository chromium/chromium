// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  css,
  html,
  LitElement,
} from 'chrome://resources/mwc/lit/index.js';

import {I18nString} from '../../i18n_string.js';
import {getI18nMessage} from '../../models/load_time_data.js';
import {withTooltip} from '../directives/with_tooltip.js';
import {DEFAULT_STYLE} from '../styles.js';

export class SwitchDeviceButton extends LitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles = [
    DEFAULT_STYLE,
    css`
      button {
        align-items: center;
        border-radius: 50%;
        display: flex;
        height: var(--big-icon);
        justify-content: center;
        outline-offset: 8px;
        width: var(--big-icon);
      }

      button:enabled:active {
        transform: scale(1.07);
      }

      :host(.animate) button {
        animation: rotate180 350ms ease-out;
      }

      @keyframes rotate180 {
        0% {
          transform: rotate(0deg);
        }
        100% {
          transform: rotate(-180deg);
        }
      }
    `,
  ];

  override render(): RenderResult {
    return html`
      <button
          aria-label=${getI18nMessage(I18nString.SWITCH_CAMERA_BUTTON)}
          ${withTooltip()}>
        <svg-wrapper name="camera_button_switch_device.svg"></svg-wrapper>
      </button>
    `;
  }
}

window.customElements.define('switch-device-button', SwitchDeviceButton);

declare global {
  interface HTMLElementTagNameMap {
    'switch-device-button': SwitchDeviceButton;
  }
}
