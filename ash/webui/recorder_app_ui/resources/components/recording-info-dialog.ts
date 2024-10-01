// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';

import {
  createRef,
  css,
  CSSResultGroup,
  html,
  nothing,
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
import {assert} from '../core/utils/assert.js';
import {formatDuration, formatFullDatetime} from '../core/utils/datetime.js';

import {CraDialog} from './cra/cra-dialog.js';

export class RecordingInfoDialog extends ReactiveLitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: contents;
    }

    cra-dialog {
      width: 420px;

      & > [slot="headline"] > cra-icon-button {
        position: absolute;
        right: 12px;
        top: 12px;
      }

      & > [slot="content"] {
        padding: 20px 32px 32px;
      }
    }

    .row {
      align-items: flex-start;
      display: flex;
      flex-flow: row;
      gap: 16px;
      padding: 8px 8px 8px 16px;

      & > cra-icon {
        height: 20px;
        width: 20px;
      }

      & > .content {
        color: var(--cros-sys-on_surface);
        display: flex;
        flex: 1;
        flex-flow: column;
        font: var(--cros-body-2-font);
        overflow-wrap: anywhere;
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingId: {type: String},
  };

  recordingId: string|null = null;

  private readonly recordingIdSignal = this.propSignal('recordingId');

  private readonly dialog = createRef<CraDialog>();

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly platformHandler = usePlatformHandler();

  private readonly recordingMetadata = computed(() => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return null;
    }
    return this.recordingDataManager.getMetadata(id).value;
  });

  private readonly recordingSize = new ScopedAsyncComputed(this, async () => {
    const id = this.recordingIdSignal.value;
    if (id === null) {
      return null;
    }
    const file = await this.recordingDataManager.getAudioFile(id);
    return file.size;
  });

  show(): void {
    // There's no user waiting for the dialog open animation to be done.
    void this.dialog.value?.show();
  }

  hide(): void {
    this.dialog.value?.close();
  }

  private formatFileSize(size: number|null): string {
    if (size === null) {
      return '';
    }

    const buckets = [
      {maxVal: 2 ** 10, unit: 'byte'},
      {maxVal: 2 ** 20, unit: 'kilobyte'},
      {maxVal: 2 ** 30, unit: 'megabyte'},
      {maxVal: 2 ** 40, unit: 'gigabyte'},
      {maxVal: 2 ** 50, unit: 'terabyte'},
      {maxVal: Infinity, unit: 'petabyte'},
    ] as const;

    const bucket = buckets.find((i) => size < i.maxVal);
    assert(bucket !== undefined);

    const displayValue = (size / bucket.maxVal) * 2 ** 10;
    const formatOptions: Intl.NumberFormatOptions = {
      style: 'unit',
      unit: bucket.unit,
    };
    if (displayValue < 1000) {
      // Only show 3 maximum significant digits when the value is less than
      // 1000.
      formatOptions.maximumSignificantDigits = 3;
    } else {
      // Don't show fractions but show full integer part when value is not less
      // than 1000.
      formatOptions.maximumFractionDigits = 0;
    }
    const formatter = new Intl.NumberFormat(
      this.platformHandler.getLocale(),
      formatOptions,
    );
    return formatter.format(displayValue);
  }

  private renderRow(icon: string, label: string, value: string) {
    return html`<div class="row">
      <cra-icon .name=${icon}></cra-icon>
      <div class="content">
        <div>${label}</div>
        <div>${value}</div>
      </div>
    </div>`;
  }

  private renderContent() {
    const meta = this.recordingMetadata.value;
    if (meta === null) {
      return nothing;
    }
    return [
      this.renderRow(
        'duration',
        i18n.recordInfoDialogDurationLabel,
        formatDuration({
          milliseconds: meta.durationMs,
        }),
      ),
      this.renderRow(
        'time_created',
        i18n.recordInfoDialogDateLabel,
        formatFullDatetime(this.platformHandler.getLocale(), meta.recordedAt),
      ),
      this.renderRow('page', i18n.recordInfoDialogTitleLabel, meta.title),
      this.renderRow(
        'music_note',
        i18n.recordInfoDialogSizeLabel,
        this.formatFileSize(this.recordingSize.value),
      ),
    ];
  }

  override render(): RenderResult {
    // Sets aria-label explicitly since the default aria-labelledby takes the
    // whole headline, which includes the "close" button.
    return html`<cra-dialog
      ${ref(this.dialog)}
      aria-label=${i18n.recordInfoDialogHeader}
    >
      <div slot="headline">
        ${i18n.recordInfoDialogHeader}
        <cra-icon-button
          buttonstyle="floating"
          size="small"
          shape="circle"
          @click=${this.hide}
          aria-label=${i18n.closeDialogButtonTooltip}
        >
          <cra-icon slot="icon" name="close"></cra-icon>
        </cra-icon-button>
      </div>
      <div slot="content">${this.renderContent()}</div>
    </cra-dialog>`;
  }
}

window.customElements.define('recording-info-dialog', RecordingInfoDialog);

declare global {
  interface HTMLElementTagNameMap {
    'recording-info-dialog': RecordingInfoDialog;
  }
}
