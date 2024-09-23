// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './export-dialog-section.js';

import {
  createRef,
  css,
  CSSResultGroup,
  html,
  PropertyDeclarations,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  usePlatformHandler,
  useRecordingDataManager,
} from '../core/lit/context.js';
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
import {assertEnumVariant} from '../core/utils/assert.js';
import {AsyncJobQueue} from '../core/utils/async_job_queue.js';

import {CraDialog} from './cra/cra-dialog.js';

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
        padding-bottom: 12px;
        padding-top: 24px;
      }

      & > [slot="actions"] {
        padding-top: 12px;
      }
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

  private readonly platformHandler = usePlatformHandler();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly transcription = new ScopedAsyncComputed(this, async () => {
    if (this.recordingIdSignal.value === null) {
      return null;
    }
    return this.recordingDataManager.getTranscription(
      this.recordingIdSignal.value,
    );
  });

  private readonly recordingSize = new ScopedAsyncComputed(this, async () => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return null;
    }
    const file = await this.recordingDataManager.getAudioFile(id);
    return file.size;
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
      this.platformHandler.perfLogger.start({
        kind: 'export',
        recordingSize: this.recordingSize.value ?? 0,
      });

      await this.recordingDataManager.exportRecording(
        recordingId,
        exportSettings,
      );

      this.platformHandler.perfLogger.finish('export');
      this.hide();
    });
    this.platformHandler.eventsSender.sendExportEvent({
      exportSettings,
      transcriptionAvailable: this.transcriptionAvailable.value,
    });
  }

  private onExportAudioChange(ev: CustomEvent<boolean>) {
    settings.mutate((s) => {
      s.exportSettings.audio = ev.detail;
    });
  }

  private onExportTranscriptionChange(ev: CustomEvent<boolean>) {
    settings.mutate((s) => {
      s.exportSettings.transcription = ev.detail;
    });
  }

  private onAudioFormatChange(ev: CustomEvent<string>) {
    settings.mutate((s) => {
      s.exportSettings.audioFormat = assertEnumVariant(
        ExportAudioFormat,
        ev.detail,
      );
    });
  }

  private onTranscriptionFormatChange(ev: CustomEvent<string>) {
    settings.mutate((s) => {
      s.exportSettings.transcriptionFormat = assertEnumVariant(
        ExportTranscriptionFormat,
        ev.detail,
      );
    });
  }

  override render(): RenderResult {
    // TODO: b/344784478 - Show estimate file size.
    const audioFormats: Array<DropdownOption<ExportAudioFormat>> = [
      {
        headline: i18n.exportDialogAudioFormatWebmOption,
        value: ExportAudioFormat.WEBM_ORIGINAL,
      },
    ];
    const transcriptionFormats:
      Array<DropdownOption<ExportTranscriptionFormat>> = [
        {
          headline: i18n.exportDialogTranscriptionFormatTxtOption,
          value: ExportTranscriptionFormat.TXT,
        },
      ];

    return html`<cra-dialog ${ref(this.dialog)}>
      <div slot="headline">${i18n.exportDialogHeader}</div>
      <div slot="content">
        <export-dialog-section
          .checked=${this.exportSettings.value.audio}
          .options=${audioFormats}
          .value=${this.exportSettings.value.audioFormat}
          @check-changed=${this.onExportAudioChange}
          @value-changed=${this.onAudioFormatChange}
        >
          <span slot="header">${i18n.exportDialogAudioHeader}</span>
        </export-dialog-section>
        <export-dialog-section
          .checked=${this.exportSettings.value.transcription}
          .disabled=${!this.transcriptionAvailable.value}
          .options=${transcriptionFormats}
          .value=${this.exportSettings.value.transcriptionFormat}
          @check-changed=${this.onExportTranscriptionChange}
          @value-changed=${this.onTranscriptionFormatChange}
        >
          <span slot="header">${i18n.exportDialogTranscriptionHeader}</span>
        </export-dialog-section>
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
