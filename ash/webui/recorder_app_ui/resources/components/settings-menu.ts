// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/switch/switch.js';
import 'chrome://resources/mwc/@material/web/progress/circular-progress.js';
import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './settings-row.js';
import './speaker-label-consent-dialog.js';
import './spoken-message.js';
import './transcription-consent-dialog.js';

import {
  Switch as CrosSwitch,
} from 'chrome://resources/cros_components/switch/switch.js';
import {
  createRef,
  css,
  html,
  live,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {usePlatformHandler} from '../core/lit/context.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {signal} from '../core/reactive/signal.js';
import {
  settings,
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
} from '../core/state/settings.js';
import {HELP_URL} from '../core/url_constants.js';
import {
  assertExhaustive,
  assertInstanceof,
  assertNotReached,
} from '../core/utils/assert.js';

import {CraDialog} from './cra/cra-dialog.js';
import {SpeakerLabelConsentDialog} from './speaker-label-consent-dialog.js';
import {TranscriptionConsentDialog} from './transcription-consent-dialog.js';

/**
 * Settings menu for Recording app.
 */
export class SettingsMenu extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
    }

    cra-dialog {
      --md-dialog-container-color: var(--cros-sys-surface3);

      /* 16px margin for each side at minimum size. */
      max-height: calc(100% - 32px);
      max-width: calc(100% - 32px);
      width: 512px;

      @container style(--dark-theme: 1) {
        /*
         * TODO: b/336963138 - This is neutral5 in spec but there's no
         * neutral5 in colors.css.
         */
        --md-dialog-container-color: var(--cros-sys-app_base_shaded);
      }
    }

    div[slot="content"] {
      padding: 0;
    }

    #header {
      padding: 24px;
      position: relative;

      & > h2 {
        color: var(--cros-sys-primary);
        font: var(--cros-title-1-font);
        margin: unset;
      }

      & > cra-icon-button {
        position: absolute;
        right: 16px;
        top: 16px;
      }
    }

    #body {
      background: var(--cros-sys-surface1);
      border-radius: 20px;
      display: flex;
      flex-flow: column;
      gap: 8px;
      padding: 0 16px 16px;

      @container style(--dark-theme: 1) {
        background: var(--cros-sys-app_base);
      }
    }

    .section {
      padding-top: 8px;

      & > .title {
        color: var(--cros-sys-primary);
        font: var(--cros-button-2-font);
        margin: unset;
        padding: 8px;
      }

      & > .body {
        border-radius: 16px;
        display: flex;
        flex-flow: column;
        gap: 1px;

        /* To have the border-radius applied to content. */
        overflow: hidden;
      }
    }

    settings-row cra-button md-circular-progress {
      --md-circular-progress-active-indicator-color: var(--cros-sys-disabled);

      /*
       * This has a lower precedence than the size override in cros-button,
       * but still need to be set to have correct line width.
       */
      --md-circular-progress-size: 24px;

      /*
       * This is to override the size setting for slotted element in
       * cros-button. On figma the circular progress have 2px padding, but
       * md-circular-progres has a non-configurable 4px padding. Setting a
       * negative margin so the extra padding doesn't expand the button size.
       */
      height: 24px;
      margin: -2px;
      width: 24px;
    }
  `;

  private readonly platformHandler = usePlatformHandler();

  private readonly dialog = createRef<CraDialog>();

  private readonly summaryDownloadRequested = signal(false);

  private readonly downloadPerfCollected = signal(false);

  private readonly transcriptionConsentDialog =
    createRef<TranscriptionConsentDialog>();

  private readonly speakerLabelConsentDialog =
    createRef<SpeakerLabelConsentDialog>();

  override updated(): void {
    if (this.summaryDownloadRequested.value &&
      !this.downloadPerfCollected.value &&
      this.platformHandler.summaryModelLoader.state.value.kind === 'installed'
    ) {
      // TODO: b/367263595 - Collect perf in PlatformHandler instead.
      this.platformHandler.perfLogger.finish('summaryModelDownload');
      this.downloadPerfCollected.value = true;
    }
  }

  show(): void {
    this.dialog.value?.show();
  }

  private get summaryEnabled() {
    return settings.value.summaryEnabled === SummaryEnableState.ENABLED;
  }

  private onDownloadSummaryClick() {
    settings.mutate((s) => {
      s.summaryEnabled = SummaryEnableState.ENABLED;
    });
    this.platformHandler.perfLogger.start({kind: 'summaryModelDownload'});
    this.platformHandler.summaryModelLoader.download();
    // The settings download both the model for summary and title suggestion.
    this.platformHandler.titleSuggestionModelLoader.download();
    this.summaryDownloadRequested.value = true;
  }

  private onSummaryToggle(ev: Event) {
    const target = assertInstanceof(ev.target, CrosSwitch);
    settings.mutate((s) => {
      s.summaryEnabled = target.selected ? SummaryEnableState.ENABLED :
                                           SummaryEnableState.DISABLED;
    });
  }

  private renderSummaryModelDescriptionAndAction() {
    const state = this.platformHandler.summaryModelLoader.state.value;
    if (state.kind === 'notInstalled') {
      // Shows the "download" button when the summary model is not installed,
      // even if it's already enabled by user. This shouldn't happen in normal
      // case, but might happen if DLC is cleared manually by any mean.
      return html`
        <span slot="description">
          ${i18n.settingsOptionsSummaryDescription}
          <a href=${HELP_URL} target="_blank">
            ${i18n.settingsOptionsSummaryLearnMoreLink}
          </a>
        </span>
        <cra-button
          slot="action"
          button-style="secondary"
          .label=${i18n.settingsOptionsSummaryDownloadButton}
          @click=${this.onDownloadSummaryClick}
        ></cra-button>
      `;
    }

    const summaryToggle = html`
      <cros-switch
        slot="action"
        .selected=${this.summaryEnabled}
        @change=${this.onSummaryToggle}
        aria-label=${i18n.settingsOptionsSummaryLabel}
      >
      </cros-switch>
    `;
    if (!this.summaryEnabled) {
      return summaryToggle;
    }
    const downloadedStatus =
      html`<spoken-message slot="status" role="status" aria-live="polite">
        ${i18n.summaryDownloadFinishedStatusMessage}
      </spoken-message>`;

    switch (state.kind) {
      case 'unavailable':
        return assertNotReached(
          'Summary model unavailable but the setting is rendered.',
        );
      case 'error':
        // TODO: b/344784638 - Render error state.
        return nothing;
      case 'installing': {
        const progressDescription =
          i18n.settingsOptionsSummaryDownloadingProgressDescription(
            state.progress,
          );
        return html`
          <span slot="description">${progressDescription}</span>
          <cra-button
            slot="action"
            button-style="secondary"
            .label=${i18n.settingsOptionsSummaryDownloadingButton}
            disabled
          >
            <md-circular-progress indeterminate slot="leading-icon">
            </md-circular-progress>
          </cra-button>
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.summaryDownloadStartedStatusMessage}
          </spoken-message>
        `;
      }
      case 'installed':
        return [
          summaryToggle,
          this.summaryDownloadRequested.value ? downloadedStatus : nothing,
        ];
      default:
        assertExhaustive(state.kind);
    }
  }

  private renderSummaryModelSettings() {
    if (this.platformHandler.summaryModelLoader.state.value.kind ===
        'unavailable') {
      return nothing;
    }
    return html`
      <settings-row>
        <span slot="label">${i18n.settingsOptionsSummaryLabel}</span>
        ${this.renderSummaryModelDescriptionAndAction()}
      </settings-row>
    `;
  }

  private onSpeakerLabelToggle() {
    switch (settings.value.speakerLabelEnabled) {
      case SpeakerLabelEnableState.ENABLED:
        settings.mutate((s) => {
          s.speakerLabelEnabled = SpeakerLabelEnableState.DISABLED;
        });
        return;
      case SpeakerLabelEnableState.DISABLED:
        settings.mutate((s) => {
          s.speakerLabelEnabled = SpeakerLabelEnableState.ENABLED;
        });
        return;
      case SpeakerLabelEnableState.UNKNOWN:
      case SpeakerLabelEnableState.DISABLED_FIRST:
        this.speakerLabelConsentDialog.value?.show();
        // This force the switch to be re-rendered so it'll catch the "live"
        // value and set selected back to false.
        this.requestUpdate();
        return;
      default:
        assertExhaustive(settings.value.speakerLabelEnabled);
    }
  }

  private renderSpeakerLabelSettings() {
    if (!this.platformHandler.canUseSpeakerLabel.value) {
      return nothing;
    }

    const speakerLabelEnabled =
      settings.value.speakerLabelEnabled === SpeakerLabelEnableState.ENABLED;
    return html`
      <settings-row>
        <span slot="label">${i18n.settingsOptionsSpeakerLabelLabel}</span>
        <span slot="description">
          ${i18n.settingsOptionsSpeakerLabelDescription}
        </span>
        <cros-switch
          slot="action"
          .selected=${live(speakerLabelEnabled)}
          @change=${this.onSpeakerLabelToggle}
          aria-label=${i18n.settingsOptionsSpeakerLabelLabel}
        ></cros-switch>
      </settings-row>
    `;
  }

  private renderTranscriptionDetailSettings() {
    if (!this.transcriptionEnabled ||
        this.platformHandler.sodaState.value.kind === 'notInstalled') {
      return nothing;
    }
    return [
      this.renderSpeakerLabelSettings(),
      this.renderSummaryModelSettings(),
    ];
  }

  private onCloseClick() {
    this.dialog.value?.close();
    this.summaryDownloadRequested.value = false;
  }

  private onTranscriptionToggle() {
    // TODO(pihsun): This is the same as in toggleTranscriptionEnabled in
    // record-page.ts, consider how to centralize the logic for all
    // transcription enable/available state transitions.
    switch (settings.value.transcriptionEnabled) {
      case TranscriptionEnableState.ENABLED:
        settings.mutate((s) => {
          s.transcriptionEnabled = TranscriptionEnableState.DISABLED;
        });
        return;
      case TranscriptionEnableState.DISABLED:
        settings.mutate((s) => {
          s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
        });
        return;
      case TranscriptionEnableState.UNKNOWN:
      case TranscriptionEnableState.DISABLED_FIRST:
        this.transcriptionConsentDialog.value?.show();
        // This force the switch to be re-rendered so it'll catch the "live"
        // value and set selected back to false.
        this.requestUpdate();
        return;
      default:
        assertExhaustive(settings.value.transcriptionEnabled);
    }
  }

  private onInstallSodaClick() {
    // TODO(pihsun): This is the same as in toggleTranscriptionEnabled in
    // record-page.ts, consider how to centralize the logic for all
    // transcription enable/available state transitions.
    switch (settings.value.transcriptionEnabled) {
      case TranscriptionEnableState.ENABLED:
      case TranscriptionEnableState.DISABLED:
        settings.mutate((s) => {
          s.transcriptionEnabled = TranscriptionEnableState.ENABLED;
        });
        this.platformHandler.installSoda();
        return;
      case TranscriptionEnableState.UNKNOWN:
      case TranscriptionEnableState.DISABLED_FIRST:
        this.transcriptionConsentDialog.value?.show();
        return;
      default:
        assertExhaustive(settings.value.transcriptionEnabled);
    }
  }

  private get transcriptionEnabled() {
    return (
      settings.value.transcriptionEnabled === TranscriptionEnableState.ENABLED
    );
  }

  private renderTranscriptionDescriptionAndAction() {
    const sodaState = this.platformHandler.sodaState.value;
    if (sodaState.kind === 'notInstalled') {
      // Shows the "download" button when SODA is not installed, even if it's
      // already enabled by user. This shouldn't happen in normal case, but
      // might happen if DLC is cleared manually by any mean.
      return html`
        <cra-button
          slot="action"
          button-style="secondary"
          .label=${i18n.settingsOptionsTranscriptionDownloadButton}
          @click=${this.onInstallSodaClick}
        ></cra-button>
      `;
    }

    const transcriptionToggle = html`
      <cros-switch
        slot="action"
        .selected=${live(this.transcriptionEnabled)}
        @change=${this.onTranscriptionToggle}
        aria-label=${i18n.settingsOptionsTranscriptionLabel}
      >
      </cros-switch>
    `;
    if (!this.transcriptionEnabled) {
      return transcriptionToggle;
    }

    switch (sodaState.kind) {
      case 'unavailable':
        return assertNotReached(
          'SODA unavailable but the setting is rendered.',
        );
      case 'error':
        // TODO: b/344784638 - Render error state.
        return nothing;
      case 'installing': {
        const progressDescription =
          i18n.settingsOptionsTranscriptionDownloadingProgressDescription(
            sodaState.progress,
          );
        return html`
          <span slot="description">${progressDescription}</span>
          <cra-button
            slot="action"
            button-style="secondary"
            .label=${i18n.settingsOptionsTranscriptionDownloadingButton}
            disabled
          >
            <md-circular-progress indeterminate slot="leading-icon">
            </md-circular-progress>
          </cra-button>
        `;
      }
      case 'installed':
        return transcriptionToggle;
      default:
        assertExhaustive(sodaState.kind);
    }
  }

  private renderTranscriptionSection() {
    if (this.platformHandler.sodaState.value.kind === 'unavailable') {
      return nothing;
    }
    return html`
      <div class="section">
        <h3 class="title">${i18n.settingsSectionTranscriptionSummaryHeader}</h3>
        <div class="body">
          <settings-row>
            <span slot="label">
              ${i18n.settingsOptionsTranscriptionLabel}
            </span>
            ${this.renderTranscriptionDescriptionAndAction()}
          </settings-row>
          ${this.renderTranscriptionDetailSettings()}
        </div>
      </div>
    `;
  }

  private onDoNotDisturbToggle() {
    this.platformHandler.quietMode.update((s) => !s);
  }

  private renderDoNotDisturbSettingsRow() {
    return html`
      <settings-row>
        <span slot="label">${i18n.settingsOptionsDoNotDisturbLabel}</span>
        <span slot="description">
          ${i18n.settingsOptionsDoNotDisturbDescription}
        </span>
        <cros-switch
          slot="action"
          .selected=${live(this.platformHandler.quietMode.value)}
          @change=${this.onDoNotDisturbToggle}
          aria-label=${i18n.settingsOptionsDoNotDisturbLabel}
        ></cros-switch>
      </settings-row>
    `;
  }

  private onKeepScreenOnToggle() {
    settings.mutate((s) => {
      s.keepScreenOn = !s.keepScreenOn;
    });
  }

  private renderKeepScreenOnSettingsRow() {
    return html`
      <settings-row>
        <span slot="label">${i18n.settingsOptionsKeepScreenOnLabel}</span>
        <cros-switch
          slot="action"
          .selected=${live(settings.value.keepScreenOn)}
          @change=${this.onKeepScreenOnToggle}
          aria-label=${i18n.settingsOptionsKeepScreenOnLabel}
        ></cros-switch>
      </settings-row>
    `;
  }

  override render(): RenderResult {
    // TODO: b/354109582 - Implement actual functionality of keep screen on.
    return html`<cra-dialog
        ${ref(this.dialog)}
        aria-label=${i18n.settingsHeader}
      >
        <div slot="content">
          <div id="header">
            <h2 id="dialog-label">${i18n.settingsHeader}</h2>
            <cra-icon-button
              buttonstyle="floating"
              size="small"
              shape="circle"
              @click=${this.onCloseClick}
              aria-label=${i18n.closeDialogButtonTooltip}
            >
              <cra-icon slot="icon" name="close"></cra-icon>
            </cra-icon-button>
          </div>
          <div id="body">
            <div class="section">
              <h3 class="title">${i18n.settingsSectionGeneralHeader}</h3>
              <div class="body">
                ${this.renderDoNotDisturbSettingsRow()}
                ${this.renderKeepScreenOnSettingsRow()}
              </div>
            </div>
            ${this.renderTranscriptionSection()}
          </div>
        </div>
      </cra-dialog>
      <transcription-consent-dialog ${ref(this.transcriptionConsentDialog)}>
      </transcription-consent-dialog>
      <speaker-label-consent-dialog ${ref(this.speakerLabelConsentDialog)}>
      </speaker-label-consent-dialog>`;
  }
}

window.customElements.define('settings-menu', SettingsMenu);

declare global {
  interface HTMLElementTagNameMap {
    'settings-menu': SettingsMenu;
  }
}
