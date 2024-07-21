// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/menu/menu_separator.js';
import './cra/cra-icon.js';
import './cra/cra-menu.js';
import './cra/cra-menu-item.js';

import {Menu} from 'chrome://resources/cros_components/menu/menu.js';
import {
  createRef,
  css,
  html,
  map,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {useMicrophoneManager} from '../core/lit/context.js';
import {MicrophoneInfo} from '../core/microphone_manager.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {settings} from '../core/state/settings.js';

/**
 * A menu that allows the user to select the input mic of the app.
 */
export class MicSelectionMenu extends ReactiveLitElement {
  static override styles = css`
    cra-menu {
      --cros-menu-width: 320px;
    }
  `;

  private readonly microphoneManager = useMicrophoneManager();

  private readonly menuRef = createRef<Menu>();

  show(anchorElement: HTMLElement): void {
    if (this.menuRef.value !== undefined) {
      const menu = this.menuRef.value;
      menu.anchorElement = anchorElement;
      menu.show();
    }
  }

  private toggleSystemAudio(): void {
    settings.mutate((d) => {
      d.includeSystemAudio = !d.includeSystemAudio;
    });
  }

  private renderMicrophone(mic: MicrophoneInfo, selectedMic: string|null):
    RenderResult {
    const micIcon = mic.isInternal ? 'mic' : 'mic_external_on';
    const isSelectedMic = mic.deviceId === selectedMic;
    // TODO(kamchonlathorn): Get status and render noise cancellation warning.
    return html`
      <cra-menu-item
        headline=${mic.label}
        itemStart="icon"
        ?checked=${isSelectedMic}
        @cros-menu-item-triggered=${() => {
      this.microphoneManager.setSelectedMicId(mic.deviceId);
    }}
      >
        <cra-icon slot="start" name=${micIcon}></cra-icon>
      </cra-menu-item>
    `;
  }

  override render(): RenderResult {
    const {includeSystemAudio} = settings.value;
    const microphones = this.microphoneManager.getMicrophoneList().value;
    const selectedMic = this.microphoneManager.getSelectedMicId().value;

    return html`<cra-menu ${ref(this.menuRef)}>
      ${map(microphones, (mic) => this.renderMicrophone(mic, selectedMic))}
      </cra-menu-item>
      <cros-menu-separator></cros-menu-separator>
      <cra-menu-item
          headline=${i18n.micSelectionMenuChromebookAudioOption}
          itemStart="icon"
          itemEnd="switch"
          .switchSelected=${includeSystemAudio}
          @cros-menu-item-triggered=${this.toggleSystemAudio}
        >
          <cra-icon slot="start" name="laptop_chromebook"></cra-icon>
        </cra-menu-item>
    </cra-menu>`;
  }
}

window.customElements.define('mic-selection-menu', MicSelectionMenu);

declare global {
  interface HTMLElementTagNameMap {
    'mic-selection-menu': MicSelectionMenu;
  }
}
