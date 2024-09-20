// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/menu/menu_separator.js';
import './cra/cra-icon.js';
import './cra/cra-icon-dropdown.js';
import './cra/cra-icon-dropdown-option.js';

import {
  css,
  html,
  live,
  map,
  nothing,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  useMicrophoneManager,
  usePlatformHandler,
} from '../core/lit/context.js';
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

  private readonly platformHandler = usePlatformHandler();

  toggleSystemAudio(): void {
    settings.mutate((d) => {
      d.includeSystemAudio = !d.includeSystemAudio;
    });
  }

  private checkSystemAudioConsent(): void {
    const triggerSystemAudioConsent = !settings.value.includeSystemAudio &&
      !settings.value.systemAudioConsentDone;
    if (!triggerSystemAudioConsent) {
      this.toggleSystemAudio();
    } else {
      // TODO(hsuanling): The selection menu will hide when the consent dialog
      // shows up. Modify the selection menu to make it stay open.
      this.dispatchEvent(new CustomEvent('trigger-system-audio-consent'));
      // requestUpdate so the includeSystemAudio switch state will be synced
      // back to off.
      this.requestUpdate();
    }
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
      <cra-icon-dropdown-option
        headline=${mic.label}
        itemStart="icon"
        ?checked=${isSelectedMic}
        @cros-icon-dropdown-option-triggered=${onSelectMic}
        data-role="menuitemcheckbox"
      >
        <cra-icon slot="start" name=${micIcon}></cra-icon>
      </cra-icon-dropdown-option>
    `;
  }

  private onSwitchChange() {
    /*
     * (Note that since cros-icon-dropdown-option is just a subclass of
     * cros-menu-item, we use cros-menu-item in the explanation below.)
     *
     * There's an issue with only listening to
     * cros-menu-item-triggered when clicking on the cros-switch directly.
     *
     * Since @cros-menu-item-triggered event is triggered inside
     * cros-menu-item @click handler, the following events happens:
     * * cros-menu-item @click handler toggles the .selected value of the
     *   underlying cros-switch, (which also sets the .selected on md-switch on
     *   next render).
     * * @cros-menu-item-triggered is triggered.
     * * checkSystemAudioConsent is called and the consent dialog is triggered
     *   without the includeSystemAudio value being changed, and a rerender of
     *   this component is requested.
     * * On update of this component, Lit sets the switchSelected of the
     *   cros-icon-dropdown-option to false, which sets the underlying
     *   cros-switch / md-switch selected property to false. But since the
     *   underlying <input type="checkbox"> hasn't fired the @input/@change
     *   event yet, md-switch sets the <input type="checkbox"> attribute to not
     *   be checked, but the .checked is still true and the @change event would
     *   still be fired.
     * * The underlying <input> fires @input event, md-switch picks up the
     *   event and set the .selected to true (= <input>.checked) again.
     * * The underlying <input> fires @change event, md-switch redispatch the
     *   event, and cros-switch picks up the event and set the .selected to true
     *   (= <md-switch>.selected) again.
     * * cros-switch redispatch the event but cros-menu-item ignores it.
     * * Since there's no further rerender calls for this component, the
     *   switch stays on and the `.switchSelected=${live(...)}` doesn't help.
     *
     * To solve this, we propagates the @change event from cros-switch back out
     * in cros-menu-item, and do another requestUpdate() here after the change
     * event, to ensure that the .switchSelected is set to correct value.
     *
     * TODO(pihsun): File a bug upstream about this.
     * TODO(pihsun): Transcription setting in record page likely have the same
     * issue for this, but since the page is updated much more frequently it's
     * likely that another unrelated change triggers update and `live()`
     * updates the value to correct value.
     */
    this.requestUpdate();
  }

  private renderSystemAudioSwitch(): RenderResult {
    if (!this.platformHandler.canCaptureSystemAudioWithLoopback.value) {
      return nothing;
    }

    const {includeSystemAudio} = settings.value;
    return html`
      <cros-menu-separator></cros-menu-separator>
      <cra-icon-dropdown-option
        headline=${i18n.micSelectionMenuChromebookAudioOption}
        itemStart="icon"
        itemEnd="switch"
        .switchSelected=${live(includeSystemAudio)}
        @cros-menu-item-triggered=${this.checkSystemAudioConsent}
        @change=${this.onSwitchChange}
      >
        <cra-icon slot="start" name="laptop_chromebook"></cra-icon>
      </cra-icon-dropdown-option>
    `;
  }

  override render(): RenderResult {
    const microphones = this.microphoneManager.getMicrophoneList().value;
    const selectedMic = this.microphoneManager.getSelectedMicId().value;

    return html`
      <cra-icon-dropdown
        id="mic-selection-button"
        shape="circle"
        anchor-corner="start-start"
        menu-corner="end-start"
        menu-type="menu"
        aria-label=${i18n.micSelectionMenuButtonTooltip}
      >
        <cra-icon slot="button-icon" name="mic"></cra-icon>
        ${map(microphones, (mic) => this.renderMicrophone(mic, selectedMic))}
        ${this.renderSystemAudioSwitch()}
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
