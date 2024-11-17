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
import {State} from '../../state.js';
import {StateObserverController} from '../state_observer_controller.js';

export class RecordTimeChip extends LitElement {
  static override styles = css`
    :host {
      align-items: center;
      background-color: var(--cros-sys-base_elevated);
      border-radius: var(--border-radius-rounded-with-short-side);
      box-sizing: border-box;
      color: var(--cros-sys-on_surface);
      display: flex;
      font-variant-numeric: tabular-nums;
      height: 32px;
      justify-content: flex-start;
      padding: 6px 12px;
    }

    #icon {
      background-color: var(--cros-ref-error40);
      border-radius: 50%;
      flex-shrink: 0;
      height: 6px;
      width: 6px;
    }

    #paused-msg {
      font: var(--cros-button-2-font);
      flex-shrink: 0;
    }

    #time-msg {
      font: var(--cros-body-2-font);
      flex-shrink: 0;
      margin-inline-start: 8px;
    }
  `;

  // TODO(pihsun): Ideally this should be passed from parent so the whole
  // component is controlled.
  private readonly recordingUiPausedState =
      new StateObserverController(this, State.RECORDING_UI_PAUSED);

  override render(): RenderResult {
    const recordingIcon = html`<div id="icon"></div>`;
    const pausedText = html`<div id="paused-msg">${
        getI18nMessage(I18nString.RECORD_VIDEO_PAUSED_MSG)}</div>`;

    return html`
    ${this.recordingUiPausedState.value ? pausedText : recordingIcon}
    <div id="time-msg"><slot></slot></div>
    `;
  }
}

window.customElements.define('record-time-chip', RecordTimeChip);

declare global {
  interface HTMLElementTagNameMap {
    'record-time-chip': RecordTimeChip;
  }
}
