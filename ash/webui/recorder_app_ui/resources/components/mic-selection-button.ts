// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import
  'chrome://resources/cros_components/icon_dropdown/icon-dropdown-option.js';
import 'chrome://resources/cros_components/menu/menu_separator.js';
import './cra/cra-icon.js';
import './cra/cra-icon-dropdown.js';

import {css, html, map} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {useMicrophoneManager} from '../core/lit/context.js';
import {MicrophoneInfo} from '../core/microphone_manager.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings} from '../core/state/settings.js';

/**
 * A button that allows the user to select the input mic of the app.
 */
export class MicSelectionButton extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: contents;
    }

    cra-icon-dropdown::part(menu) {
      --cros-menu-width: 320px;
    }
  `;

  private readonly microphoneManager = useMicrophoneManager();

  private toggleSystemAudio(): void {
    settings.mutate((d) => {
      d.includeSystemAudio = !d.includeSystemAudio;
    });
  }

  private renderMicrophone(
    mic: MicrophoneInfo,
    selectedMic: string|null,
  ): RenderResult {
    const micIcon = mic.isInternal ? 'mic' : 'mic_external_on';
    const isSelectedMic = mic.deviceId === selectedMic;
    const onSelectMic = () => {
      this.microphoneManager.setSelectedMicId(mic.deviceId);
    };
    // TODO(kamchonlathorn): Get status and render noise cancellation warning.
    return html`
      <cros-icon-dropdown-option
        headline=${mic.label}
        itemStart="icon"
        ?checked=${isSelectedMic}
        @cros-icon-dropdown-option-triggered=${onSelectMic}
      >
        <cra-icon slot="start" name=${micIcon}></cra-icon>
      </cros-icon-dropdown-option>
    `;
  }

  override render(): RenderResult {
    const {includeSystemAudio} = settings.value;
    const microphones = this.microphoneManager.getMicrophoneList().value;
    const selectedMic = this.microphoneManager.getSelectedMicId().value;

    return html`
      <cra-icon-dropdown
        id="mic-selection-button"
        shape="circle"
        anchor-corner="start-start"
        menu-corner="end-start"
      >
        <cra-icon slot="button-icon" name="mic"></cra-icon>
        ${map(microphones, (mic) => this.renderMicrophone(mic, selectedMic))}
        <cros-menu-separator></cros-menu-separator>
        <cros-icon-dropdown-option
          headline=${i18n.micSelectionMenuChromebookAudioOption}
          itemStart="icon"
          itemEnd="switch"
          .switchSelected=${includeSystemAudio}
          @cros-menu-item-triggered=${this.toggleSystemAudio}
        >
          <cra-icon slot="start" name="laptop_chromebook"></cra-icon>
        </cros-icon-dropdown-option>
      </cra-icon-dropdown>
    `;
  }
}

window.customElements.define('mic-selection-button', MicSelectionButton);

declare global {
  interface HTMLElementTagNameMap {
    'mic-selection-button': MicSelectionButton;
  }
}
