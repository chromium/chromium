// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/switch/switch.js';
import 'chrome://resources/mwc/@material/web/progress/circular-progress.js';
import './cra/cra-button.js';
import './cra/cra-dialog.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './language-picker.js';
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
import {LanguageCode} from '../core/soda/language_info.js';
import {
  settings,
  SpeakerLabelEnableState,
  SummaryEnableState,
  TranscriptionEnableState,
} from '../core/state/settings.js';
import {
  enableTranscription,
  setTranscriptionLanguage,
  toggleTranscriptionEnabled,
} from '../core/state/transcription.js';
import {HELP_URL} from '../core/url_constants.js';
import {
  assert,
  assertExhaustive,
  assertExists,
  assertInstanceof,
  assertNotReached,
} from '../core/utils/assert.js';
import {stopPropagation} from '../core/utils/event_handler.js';

import {CraDialog} from './cra/cra-dialog.js';
import {CraIconButton} from './cra/cra-icon-button.js';
import {withTooltip} from './directives/with-tooltip.js';
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

    language-picker {
      background: var(--cros-sys-surface1);

      @container style(--dark-theme: 1) {
        background: var(--cros-sys-app_base);
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

  private readonly subpageButton = createRef<CraIconButton>();

  private readonly summaryDownloadRequested = signal(false);

  private readonly shouldShowLanguagePicker =
    this.platformHandler.isMultipleLanguageAvailable();

  private readonly transcriptionLanguageExpanded = signal(false);

  private readonly transcriptionConsentDialog =
    createRef<TranscriptionConsentDialog>();

  private readonly speakerLabelConsentDialog =
    createRef<SpeakerLabelConsentDialog>();

  show(): void {
    this.dialog.value?.show();
    this.transcriptionLanguageExpanded.value = false;
  }

  private get summaryEnabled() {
    return settings.value.summaryEnabled === SummaryEnableState.ENABLED;
  }

  private onDownloadSummaryClick() {
    settings.mutate((s) => {
      s.summaryEnabled = SummaryEnableState.ENABLED;
    });
    this.platformHandler.perfLogger.start({kind: 'summaryModelDownload'});
    this.platformHandler.downloadGenAiModel();
    this.summaryDownloadRequested.value = true;
  }

  private onSummaryToggle(ev: Event) {
    const target = assertInstanceof(ev.target, CrosSwitch);
    settings.mutate((s) => {
      s.summaryEnabled = target.selected ? SummaryEnableState.ENABLED :
                                           SummaryEnableState.DISABLED;
    });
  }

  private renderSummaryModelDownloadStatus() {
    const state = this.platformHandler.getGenAiModelState().kind;
    switch (state) {
      case 'unavailable':
        return assertNotReached(
          'Summary model unavailable but the setting is rendered.',
        );
      case 'notInstalled':
        return nothing;
      case 'needsReboot':
        return html`
          <spoken-message
            slot="status"
            role="status"
            aria-live="polite"
          >
            ${i18n.genAiNeedsRebootStatusMessage}
          </spoken-message>
        `;
      case 'error':
        return html`
          <spoken-message
            slot="status"
            role="status"
            aria-live="polite"
          >
            ${i18n.genAiDownloadErrorStatusMessage}
          </spoken-message>
        `;
      case 'installed':
        if (!this.summaryDownloadRequested.value) {
          return nothing;
        }
        return html`
          <spoken-message
            slot="status"
            role="status"
            aria-live="polite"
          >
            ${i18n.genAiDownloadFinishedStatusMessage}
          </spoken-message>
        `;
      case 'installing':
        return html`
          <spoken-message slot="status" role="status" aria-live="polite">
            ${i18n.genAiDownloadStartedStatusMessage}
          </spoken-message>
        `;
      default:
        return assertExhaustive(state);
    }
  }

  private renderSummaryModelDescriptionAndAction() {
    const state = this.platformHandler.getGenAiModelState();
    const downloadButton = html`
      <cra-button
        slot="action"
        button-style="secondary"
        .label=${i18n.settingsOptionsGenAiDownloadButton}
        @click=${this.onDownloadSummaryClick}
        aria-label=${i18n.settingsOptionsGenAiDownloadButtonAriaLabel}
      ></cra-button>
    `;
    if (state.kind === 'notInstalled') {
      // Shows the "download" button when the summary model is not installed,
      // even if it's already enabled by user. This shouldn't happen in normal
      // case, but might happen if DLC is cleared manually by any mean.
      return html`
        <span slot="description">
          ${i18n.settingsOptionsGenAiDescription}
          <a
            href=${HELP_URL}
            target="_blank"
            @click=${stopPropagation}
            aria-label=${i18n.settingsOptionsGenAiLearnMoreLinkAriaLabel}
          >
            ${i18n.settingsOptionsGenAiLearnMoreLink}
          </a>
        </span>
        ${downloadButton}
      `;
    }

    if (state.kind === 'error') {
      // Shows the "download" button when summary model fails to download so
      // that users can try download again later.
      return html`
        <span slot="description" class="error">
          ${i18n.settingsOptionsGenAiErrorDescription}
        </span>
        ${downloadButton}
      `;
    }

    const summaryToggle = html`
      <cros-switch
        slot="action"
        .selected=${this.summaryEnabled}
        @change=${this.onSummaryToggle}
        aria-label=${i18n.settingsOptionsGenAiLabel}
      >
      </cros-switch>
    `;
    if (!this.summaryEnabled) {
      return summaryToggle;
    }

    switch (state.kind) {
      case 'unavailable':
        return assertNotReached(
          'Summary model unavailable but the setting is rendered.',
        );
      case 'needsReboot':
        return html`
          <span slot="description" class="error">
            ${i18n.settingsOptionsGenAiNeedsRebootDescription}
          </span>
          ${downloadButton}
        `;
      case 'installing': {
        const progressDescription =
          i18n.settingsOptionsGenAiDownloadingProgressDescription(
            state.progress,
          );
        return html`
          <span slot="description">${progressDescription}</span>
          <cra-button
            slot="action"
            button-style="secondary"
            .label=${i18n.settingsOptionsGenAiDownloadingButton}
            disabled
          >
            <md-circular-progress indeterminate slot="leading-icon">
            </md-circular-progress>
          </cra-button>
        `;
      }
      case 'installed':
        return summaryToggle;
      default:
        assertExhaustive(state.kind);
    }
  }

  private renderSummaryModelSettings() {
    if (this.platformHandler.getGenAiModelState().kind === 'unavailable') {
      return nothing;
    }
    return html`
      <settings-row>
        <span slot="label">${i18n.settingsOptionsGenAiLabel}</span>
        ${this.renderSummaryModelDescriptionAndAction()}
        ${this.renderSummaryModelDownloadStatus()}}
      </settings-row>
    `;
  }

  private onLanguagePickerExpand() {
    assert(!this.transcriptionLanguageExpanded.value);
    this.transcriptionLanguageExpanded.value = true;
  }

  private renderTranscriptLanguageSettings() {
    if (!this.shouldShowLanguagePicker) {
      return nothing;
    }
    let description = i18n.settingsOptionsTranscriptionLanguageDescription;
    const selectedLanguage = this.platformHandler.getSelectedLanguage();
    if (selectedLanguage !== null) {
      const langPackInfo =
        this.platformHandler.getLangPackInfo(selectedLanguage);
      // Shows selected language regardless of its state. The state will be
      // shown in the subpage or in the transcript view when recording.
      description = langPackInfo.displayName;
    }
    return html`
      <settings-row>
        <span slot="label">
          ${i18n.settingsOptionsTranscriptionLanguageLabel}
        </span>
        <span slot="description">${description}</span>
        <cra-icon-button
          buttonstyle="floating"
          size="small"
          slot="action"
          shape="circle"
          aria-label=${i18n.settingsOptionsLanguageSubpageButtonAriaLabel}
          @click=${this.onLanguagePickerExpand}
          ${ref(this.subpageButton)}
        >
          <cra-icon slot="icon" name="chevron_right"></cra-icon>
        </cra-icon-button>
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
    if (!this.transcriptionEnabled) {
      return nothing;
    }

    if (!this.shouldShowLanguagePicker) {
      const defaultLang = LanguageCode.EN_US;
      const sodaState = this.platformHandler.getSodaState(defaultLang).value;
      if (sodaState.kind !== 'installed' && sodaState.kind !== 'installing') {
        return nothing;
      }
    }
    return [
      this.renderSpeakerLabelSettings(),
      this.renderTranscriptLanguageSettings(),
      this.renderSummaryModelSettings(),
    ];
  }

  private onCloseClick() {
    this.dialog.value?.close();
    this.summaryDownloadRequested.value = false;
  }

  private onTranscriptionToggle() {
    if (!toggleTranscriptionEnabled()) {
      this.transcriptionConsentDialog.value?.show();
      // This force the switch to be re-rendered so it'll catch the "live"
      // value and set selected back to false.
      this.requestUpdate();
    }
  }

  private renderTranscriptionToggle() {
    return html`
      <cros-switch
        slot="action"
        .selected=${live(this.transcriptionEnabled)}
        @change=${this.onTranscriptionToggle}
        aria-label=${i18n.settingsOptionsTranscriptionLabel}
      >
      </cros-switch>
    `;
  }

  private get transcriptionEnabled() {
    return (
      settings.value.transcriptionEnabled === TranscriptionEnableState.ENABLED
    );
  }

  private renderTranscriptionDescriptionAndAction() {
    const defaultLang = LanguageCode.EN_US;
    const sodaState = this.platformHandler.getSodaState(defaultLang).value;
    const onInstallSodaClick = () => {
      if (!enableTranscription()) {
        this.transcriptionConsentDialog.value?.show();
        return;
      }
      setTranscriptionLanguage(defaultLang);
    };
    const downloadButton = html`
      <cra-button
        slot="action"
        button-style="secondary"
        .label=${i18n.settingsOptionsTranscriptionDownloadButton}
        @click=${onInstallSodaClick}
        aria-label=${i18n.settingsOptionsTranscriptionDownloadButtonAriaLabel}
      ></cra-button>
    `;
    if (sodaState.kind === 'notInstalled') {
      // Shows the "download" button when SODA is not installed, even if it's
      // already enabled by user. This shouldn't happen in normal case, but
      // might happen if DLC is cleared manually by any mean.
      return downloadButton;
    }

    if (sodaState.kind === 'error') {
      // Shows the "download" button when SODA fails to install so that users
      // try download again later.
      return html`
        <span slot="description" class="error">
          ${i18n.settingsOptionsTranscriptionErrorDescription}
        </span>
        ${downloadButton}
      `;
    }

    const transcriptionToggle = this.renderTranscriptionToggle();
    if (!this.transcriptionEnabled) {
      return transcriptionToggle;
    }

    switch (sodaState.kind) {
      case 'unavailable':
        return assertNotReached(
          'SODA unavailable but the setting is rendered.',
        );
      case 'needsReboot':
        return html`
          <span slot="description" class="error">
            ${i18n.settingsOptionsTranscriptionNeedsRebootDescription}
          </span>
          ${downloadButton}
        `;
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
    if (!this.platformHandler.isSodaAvailable()) {
      return nothing;
    }
    const renderTranscriptionRow = this.shouldShowLanguagePicker ?
      this.renderTranscriptionToggle() :
      this.renderTranscriptionDescriptionAndAction();
    return html`
      <div class="section">
        <h3 class="title">${i18n.settingsSectionTranscriptionSummaryHeader}</h3>
        <div class="body">
          <settings-row>
            <span slot="label">
              ${i18n.settingsOptionsTranscriptionLabel}
            </span>
            ${renderTranscriptionRow}
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

  private onSubpageCloseClick() {
    assert(this.transcriptionLanguageExpanded.value);
    this.transcriptionLanguageExpanded.value = false;
    this.updateComplete.then(() => {
      const subpageButton = assertExists(this.subpageButton.value);
      subpageButton.updateComplete.then(() => {
        subpageButton.focus();
      });
    });
  }

  private renderSettingsBody(): RenderResult {
    if (this.transcriptionLanguageExpanded.value) {
      return html`
        <language-picker @close=${this.onSubpageCloseClick}></language-picker>
      `;
    }
    return html`
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
    `;
  }

  override render(): RenderResult {
    // TODO: b/354109582 - Implement actual functionality of keep screen on.
    return html`<cra-dialog
        ${ref(this.dialog)}
        aria-label=${i18n.settingsHeader}
      >
        <div id="header" slot="headline">
          <h2 id="dialog-label">${i18n.settingsHeader}</h2>
          <cra-icon-button
            buttonstyle="floating"
            size="small"
            shape="circle"
            @click=${this.onCloseClick}
            aria-label=${i18n.closeDialogButtonTooltip}
            ${withTooltip()}
          >
            <cra-icon slot="icon" name="close"></cra-icon>
          </cra-icon-button>
        </div>
        <div slot="content">${this.renderSettingsBody()}</div>
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
