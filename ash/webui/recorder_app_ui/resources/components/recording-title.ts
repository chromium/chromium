// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/textfield/textfield.js';
import 'chrome://resources/cros_components/snackbar/snackbar.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './cra/cra-tooltip.js';
import './recording-title-suggestion.js';

import {
  Snackbar,
} from 'chrome://resources/cros_components/snackbar/snackbar.js';
import {
  Textfield,
} from 'chrome://resources/cros_components/textfield/textfield.js';
import {
  createRef,
  css,
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
import {computed, signal} from '../core/reactive/signal.js';
import {RecordingMetadata} from '../core/recording_data_manager.js';
import {settings, SummaryEnableState} from '../core/state/settings.js';
import {assertExists, assertInstanceof} from '../core/utils/assert.js';
import {CraIconButton} from './cra/cra-icon-button.js';
import {RecordingTitleSuggestion} from './recording-title-suggestion.js';

/**
 * The title of the recording in playback page of Recorder App.
 */
export class RecordingTitle extends ReactiveLitElement {
  static override styles = css`
    :host {
      display: block;
      min-width: 0;
    }

    cros-textfield {
      anchor-name: --title-textfield;
      width: 283px;
    }

    recording-title-suggestion {
      position: absolute;
      position-anchor: --title-textfield;
      position-area: bottom span-right;
      margin-top: 4.5px;
      max-width: 402px;
      min-width: 360px;
    }

    #title {
      anchor-name: --title;
      border-radius: 12px;
      box-sizing: border-box;
      font: var(--cros-headline-1-font);
      overflow: hidden;
      padding: 8px 16px;
      text-overflow: ellipsis;
      white-space: nowrap;

      & > cra-tooltip {
        position-anchor: --title;
        display: none;
      }

      &:hover {
        background-color: var(--cros-sys-hover_on_subtle);

        & > cra-tooltip {
          display: block;
        }
      }
    }
  `;

  static override properties: PropertyDeclarations = {
    recordingMetadata: {attribute: false},
  };

  recordingMetadata: RecordingMetadata|null = null;

  private readonly recordingMetadataSignal =
    this.propSignal('recordingMetadata');

  private readonly editing = signal(false);

  private readonly suggestionShown = signal(false);

  private readonly recordingDataManager = useRecordingDataManager();

  private readonly platformHandler = usePlatformHandler();

  private readonly renameContainer = createRef<HTMLDivElement>();

  private readonly snackBar = createRef<Snackbar>();

  private readonly suggestTitleButton = createRef<CraIconButton>();

  private readonly recordingTitleSuggestion =
    createRef<RecordingTitleSuggestion>();

  get renameContainerForTest(): HTMLDivElement {
    return assertExists(this.renameContainer.value);
  }

  get suggestTitleButtonForTest(): CraIconButton {
    return assertExists(this.suggestTitleButton.value);
  }

  get titleSuggestionForTest(): RecordingTitleSuggestion {
    return assertExists(this.recordingTitleSuggestion.value);
  }

  private readonly recordingId = computed(() => {
    return this.recordingMetadataSignal.value?.id ?? null;
  });

  private readonly transcription = new ScopedAsyncComputed(this, async () => {
    if (this.recordingId.value === null) {
      return null;
    }
    return this.recordingDataManager.getTranscription(this.recordingId.value);
  });

  private readonly shouldShowTitleSuggestion = computed(() => {
    const modelState = this.platformHandler.titleSuggestionModelLoader.state;
    return (
      modelState.value.kind === 'installed' &&
      settings.value.summaryEnabled === SummaryEnableState.ENABLED &&
      this.transcription.value !== null && !this.transcription.value.isEmpty()
    );
  });

  private readonly suggestedTitles = new ScopedAsyncComputed(this, async () => {
    // TODO(pihsun): Cache title suggestion between hide/show the suggestion
    // dialog?
    if (!this.suggestionShown.value || !this.shouldShowTitleSuggestion.value) {
      return null;
    }
    if (this.transcription.value === null) {
      return null;
    }

    // TODO(pihsun): Have a specific format for transcription to be used as
    // model input.
    const text = this.transcription.value.toPlainText();

    this.platformHandler.perfLogger.start({
      kind: 'titleSuggestion',
      wordCount: this.transcription.value.wordCount,
    });

    const {titleSuggestionModelLoader} = this.platformHandler;
    const suggestions = await titleSuggestionModelLoader.loadAndExecute(text);
    this.platformHandler.perfLogger.finish('titleSuggestion');

    return suggestions;
  });

  private get editTextfield(): Textfield|null {
    return this.shadowRoot?.querySelector('cros-textfield') ?? null;
  }

  private async startEditTitle() {
    this.editing.value = true;
    // TODO(pihsun): This somehow requires three updates before the
    // .focusTextfield() work, investigate why. Might be related to the
    // additional update pass caused by signal integration.
    await this.updateComplete;
    await this.updateComplete;
    await this.updateComplete;

    this.editTextfield?.focusTextfield();
  }

  private onFocusout(ev: FocusEvent) {
    if (this.suggestionShown.value) {
      // Suggestion dialog is shown, don't auto-close everything on focus out.
      return;
    }
    const newTarget = ev.relatedTarget;
    if (newTarget instanceof Node && this.editTextfield?.contains(newTarget)) {
      // New target is a child of the textfield or title suggestion, don't stop
      // editing.
      return;
    }
    this.editing.value = false;
    // TODO(pihsun): The focusout/blur event got triggered synchronously on
    // render when an element is removed, which breaks the assumption in the
    // reactive/lit.ts implementation of ReactiveLitElement, so setting values
    // above won't trigger rerender. Call requestUpdate() by ourselves to
    // workaround this.
    this.requestUpdate();
  }

  private onTextfieldKeyDown(ev: KeyboardEvent) {
    const target = assertInstanceof(ev.target, Textfield);
    if (ev.key === 'Escape') {
      // Revert back to the old name and exit textfield.
      target.value = this.recordingMetadata?.title ?? '';
      target.blurTextfield();
    } else if (ev.key === 'Enter') {
      // Exit text field.
      target.blurTextfield();
    }
  }

  private onSuggestTitleButtonKeyDown(ev: KeyboardEvent) {
    ev.stopPropagation();
    if (ev.key === 'Escape') {
      // Turn focus back to text field.
      this.editTextfield?.focusTextfield();
    }
  }

  private setTitle(title: string) {
    const meta = this.recordingMetadata;
    if (meta === null) {
      return;
    }
    this.recordingDataManager.setMetadata(meta.id, {
      ...meta,
      title,
    });
    const snackBar = assertExists(this.snackBar.value);
    snackBar.showPopover();
  }

  private onChangeTitle(ev: Event) {
    const target = assertInstanceof(ev.target, Textfield);
    // Change title when the text field is not empty.
    if (target.value !== '') {
      this.setTitle(target.value);
    }
  }

  private onSuggestTitle(ev: CustomEvent<string>) {
    this.setTitle(ev.detail);
  }

  private openSuggestionDialog() {
    this.suggestionShown.value = true;
  }

  private closeSuggestionDialog() {
    // We focus on the text field before closing the suggestion, so the
    // focusout event from the suggestion dialog won't cause edit to be exited.
    this.editTextfield?.focusTextfield();
    this.suggestionShown.value = false;
  }

  private renderSuggestionDialog() {
    if (!this.suggestionShown.value) {
      return nothing;
    }

    return html`<recording-title-suggestion
      @close=${this.closeSuggestionDialog}
      @change=${this.onSuggestTitle}
      .suggestedTitles=${this.suggestedTitles}
      .wordCount=${this.transcription.value?.wordCount ?? 0}
      ${ref(this.recordingTitleSuggestion)}
    ></recording-title-suggestion>`;
  }

  private renderTitle(): RenderResult {
    if (this.editing.value) {
      const suggestionIconButton =
        this.suggestionShown.value || !this.shouldShowTitleSuggestion.value ?
        nothing :
        html`<cra-icon-button
              buttonstyle="floating"
              size="small"
              slot="trailing"
              shape="circle"
              @click=${this.openSuggestionDialog}
              @keydown=${this.onSuggestTitleButtonKeyDown}
              ${ref(this.suggestTitleButton)}
              aria-label=${i18n.titleSuggestionButtonTooltip}
            >
              <cra-icon slot="icon" name="pen_spark"></cra-icon>
            </cra-icon-button>`;
      return html`<cros-textfield
          type="text"
          .value=${this.recordingMetadata?.title ?? ''}
          @change=${this.onChangeTitle}
          @focusout=${this.onFocusout}
          @keydown=${this.onTextfieldKeyDown}
          aria-label=${i18n.titleTextfieldAriaLabel}
        >
          ${suggestionIconButton}
        </cros-textfield>
        ${this.renderSuggestionDialog()}`;
    }
    // TODO(pihsun): Have a directive for tooltip instead of having user to
    // manually add <cra-tooltip> and CSS styles.
    return html`
      <div
        id="title"
        tabindex="0"
        @focus=${this.startEditTitle}
        @click=${this.startEditTitle}
        ${ref(this.renameContainer)}
      >
        ${this.recordingMetadata?.title ?? ''}
        <cra-tooltip>${i18n.titleRenameTooltip}</cra-tooltip>
      </div>
    `;
  }

  override render(): RenderResult {
    return html`
      <cros-snackbar
        message=${i18n.titleRenameSnackbarMessage}
        timeoutMs="4000"
        ${ref(this.snackBar)}
      ></cros-snackbar>
      ${this.renderTitle()}
    `;
  }
}

window.customElements.define('recording-title', RecordingTitle);

declare global {
  interface HTMLElementTagNameMap {
    'recording-title': RecordingTitle;
  }
}
