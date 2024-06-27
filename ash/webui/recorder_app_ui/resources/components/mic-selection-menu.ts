// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mwc/@material/web/dialog/dialog.js';
import 'chrome://resources/mwc/@material/web/icon/icon.js';
import 'chrome://resources/mwc/@material/web/iconbutton/filled-icon-button.js';
import 'chrome://resources/mwc/@material/web/iconbutton/icon-button.js';
import 'chrome://resources/mwc/@material/web/radio/radio.js';
import './cra/cra-icon.js';

import {MdDialog} from 'chrome://resources/mwc/@material/web/dialog/dialog.js';
import {css, html} from 'chrome://resources/mwc/lit/index.js';

import {ReactiveLitElement} from '../core/reactive/lit.js';
import {AudioSource} from '../core/recording_session.js';
import {settings} from '../core/state/settings.js';

/**
 * A menu that allows the user to select the input mic of the app.
 */
export class MicSelectionMenu extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    .title {
      flex: 1;
      font: var(--cros-display-5-font);
    }

    .column {
      display: flex;
      flex-direction: column;
      gap: 0.5em;
    }

    .section-title {
      color: var(--cros-sys-secondary);
      font: var(--cros-title-2-font);
    }

    .radio-label {
      align-items: center;
      display: flex;
      font: var(--cros-body-0-font);
      gap: 1em;
    }
  `;

  private get dialog(): MdDialog|null {
    return this.shadowRoot?.querySelector('md-dialog') ?? null;
  }

  show(): void {
    this.dialog?.show();
  }

  private onAudioSourceChange(newSource: AudioSource): void {
    settings.mutate((d) => {
      d.audioSource = newSource;
    });
  }

  override render(): RenderResult {
    const {audioSource} = settings.value;
    const selectUserMedia = () => {
      this.onAudioSourceChange(AudioSource.USER_MEDIA);
    };
    const selectDisplayMedia = () => {
      this.onAudioSourceChange(AudioSource.DISPLAY_MEDIA);
    };
    // TODO(shik): Side sheet seems to be better than dialog, but Material Web
    // has no side sheet yet.
    // TODO: b/336963138 - Implement mic selection as in spec.
    return html`<md-dialog>
      <div slot="headline">
        <span class="title">Mic Selection</span>
        <md-icon-button form="form">
          <cra-icon name="close"></cra-icon>
        </md-icon-button>
      </div>
      <form id="form" slot="content" method="dialog">
        <div class="column">
          <span class="section-title">Audio Source</span>
          <div class="radio-label">
            <md-radio
              id="mic-radio"
              name="source"
              ?checked=${audioSource === AudioSource.USER_MEDIA}
              @change=${selectUserMedia}
            >
            </md-radio>
            <label for="mic-radio">Microphone</label>
          </div>

          <div class="radio-label">
            <md-radio
              id="tab-radio"
              name="source"
              ?checked=${audioSource === AudioSource.DISPLAY_MEDIA}
              @change=${selectDisplayMedia}
            >
            </md-radio>
            <label for="tab-radio">Tab or Window</label>
          </div>
        </div>
      </form>
    </md-dialog>`;
  }
}

window.customElements.define('mic-selection-menu', MicSelectionMenu);

declare global {
  interface HTMLElementTagNameMap {
    'mic-selection-menu': MicSelectionMenu;
  }
}
