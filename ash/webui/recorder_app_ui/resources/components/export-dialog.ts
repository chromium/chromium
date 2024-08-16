// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/dropdown/dropdown_option.js';
import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './cra/cra-dropdown.js';
import './expandable-card.js';

import {
  createRef,
  css,
  CSSResultGroup,
  html,
  map,
  nothing,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {useRecordingDataManager} from '../core/lit/context.js';
import {
  ReactiveLitElement,
  ScopedAsyncComputed,
} from '../core/reactive/lit.js';
import {computed} from '../core/reactive/signal.js';
import {
  ExportAudioFormat,
  ExportTranscriptionFormat,
  settings,
} from '../core/state/settings.js';
import {
  assertEnumVariant,
  assertInstanceof,
  assertString,
} from '../core/utils/assert.js';
import {AsyncJobQueue} from '../core/utils/async_job_queue.js';

import {CraDialog} from './cra/cra-dialog.js';
import {CraDropdown} from './cra/cra-dropdown.js';

interface DropdownOption<T extends string> {
  headline: string;
  value: T;
}

export class ExportDialog extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: contents;
    }

    cra-dialog {
      width: 440px;

      & > [slot="content"] {
        display: flex;
        flex-flow: column;
        gap: 16px;
        padding-top: 24px;
      }

      & > [slot="actions"] {
        padding-top: 24px;
      }
    }

    .header {
      color: var(--cros-sys-on_surface);
      font: var(--cros-headline-1-font);
    }

    cros-dropdown-option cra-icon {
      color: var(--cros-sys-primary);
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingId: {type: String},
  };

  recordingId: string|null = null;

  private readonly recordingIdSignal = this.propSignal('recordingId');

  private readonly dialog = createRef<CraDialog>();

  private readonly exportSettings = computed(
    () => settings.value.exportSettings,
  );

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly transcription = new ScopedAsyncComputed(this, async () => {
    if (this.recordingIdSignal.value === null) {
      return null;
    }
    return this.recordingDataManager.getTranscription(
      this.recordingIdSignal.value,
    );
  });

  private readonly transcriptionAvailable = computed(
    () => !(this.transcription.value?.isEmpty() ?? true),
  );

  private readonly exportQueue = new AsyncJobQueue('drop');

  private get saveEnabled() {
    return (
      this.exportSettings.value.audio ||
      (this.transcriptionAvailable.value &&
       this.exportSettings.value.transcription)
    );
  }

  show(): void {
    // There's no user waiting for the dialog open animation to be done.
    void this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  private save() {
    const recordingId = this.recordingId;
    const exportSettings = this.exportSettings.value;
    if (!this.saveEnabled || recordingId === null) {
      return;
    }
    // TODO(pihsun): Loading state for export recording.
    // TODO(pihsun): Handle failure.
    this.exportQueue.push(async () => {
      await this.recordingDataManager.exportRecording(
        recordingId,
        exportSettings,
      );
      this.hide();
    });
  }

  private toggleExportAudio() {
    settings.mutate((s) => {
      s.exportSettings.audio = !s.exportSettings.audio;
    });
  }

  private toggleExportTranscription() {
    settings.mutate((s) => {
      s.exportSettings.transcription = !s.exportSettings.transcription;
    });
  }

  private onAudioFormatChange(ev: Event) {
    settings.mutate((s) => {
      s.exportSettings.audioFormat = assertEnumVariant(
        ExportAudioFormat,
        assertInstanceof(ev.target, CraDropdown).value,
      );
    });
  }

  private onTranscriptionFormatChange(ev: Event) {
    settings.mutate((s) => {
      s.exportSettings.transcriptionFormat = assertEnumVariant(
        ExportTranscriptionFormat,
        assertInstanceof(ev.target, CraDropdown).value,
      );
    });
  }

  private renderDropdownOptions<T extends string>(
    options: Array<DropdownOption<T>>,
    selected: T,
  ): RenderResult {
    return map(options, ({headline, value}) => {
      const icon = value === selected ?
        html`<cra-icon name="checked" slot="end"></cra-icon>` :
        nothing;
      // lit-analyzer somehow doesn't think "T extends string" is assignable to
      // "string", so we have to add `assertString`...
      return html`
        <cros-dropdown-option
          .headline=${headline}
          .value=${assertString(value)}
        >
          ${icon}
        </cros-dropdown-option>
      `;
    });
  }

  override render(): RenderResult {
    // TODO(pihsun): Investigate why the cros-dropdown can't be closed by
    // clicking on the select again...
    // TODO: b/344784478 - Show estimate file size.
    const audioOptions = this.renderDropdownOptions(
      [
        {
          headline: i18n.exportDialogAudioFormatWebmOption,
          value: ExportAudioFormat.WEBM_ORIGINAL,
        },
      ],
      this.exportSettings.value.audioFormat,
    );

    const transcriptionOptions = this.renderDropdownOptions(
      [
        {
          headline: i18n.exportDialogTranscriptionFormatTxtOption,
          value: ExportTranscriptionFormat.TXT,
        },
      ],
      this.exportSettings.value.transcriptionFormat,
    );

    return html`<cra-dialog ${ref(this.dialog)}>
      <div slot="headline">${i18n.exportDialogHeader}</div>
      <div slot="content">
        <expandable-card
          ?expanded=${this.exportSettings.value.audio}
          @toggle-expand=${this.toggleExportAudio}
        >
          <span slot="header" class="header">
            ${i18n.exportDialogAudioHeader}
          </span>
          <cra-dropdown
            .value=${this.exportSettings.value.audioFormat}
            slot="content"
            @change=${this.onAudioFormatChange}
          >
            ${audioOptions}
          </cra-dropdown>
        </expandable-card>
        <expandable-card
          ?expanded=${this.exportSettings.value.transcription}
          @toggle-expand=${this.toggleExportTranscription}
          ?disabled=${!this.transcriptionAvailable.value}
        >
          <span slot="header" class="header">
            ${i18n.exportDialogTranscriptionHeader}
          </span>
          <cra-dropdown
            slot="content"
            .value=${this.exportSettings.value.transcriptionFormat}
            @change=${this.onTranscriptionFormatChange}
          >
            ${transcriptionOptions}
          </cra-dropdown>
        </expandable-card>
      </div>
      <div slot="actions">
        <cra-button
          .label=${i18n.exportDialogCancelButton}
          button-style="secondary"
          @click=${this.hide}
        ></cra-button>
        <cra-button
          .label=${i18n.exportDialogSaveButton}
          @click=${this.save}
          ?disabled=${!this.saveEnabled}
        ></cra-button>
      </div>
    </cra-dialog>`;
  }
}

window.customElements.define('export-dialog', ExportDialog);

declare global {
  interface HTMLElementTagNameMap {
    'export-dialog': ExportDialog;
  }
}
