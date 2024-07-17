// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/textfield/textfield.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';
import './cra/cra-tooltip.js';
import './recording-title-suggestion.js';

import {
  Textfield,
} from 'chrome://resources/cros_components/textfield/textfield.js';
import {
  css,
  html,
  nothing,
  PropertyDeclarations,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {
  usePlatformHandler,
  useRecordingDataManager,
} from '../core/lit/context.js';
import {ModelId} from '../core/on_device_model/types.js';
import {
  ReactiveLitElement,
  ScopedAsyncComputed,
} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {RecordingMetadata} from '../core/recording_data_manager.js';
import {concatTextTokens} from '../core/soda/soda.js';
import {settings, SummaryEnableState} from '../core/state/settings.js';
import {assertInstanceof} from '../core/utils/assert.js';

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
      inset-area: bottom span-right;
      margin-top: 4.5px;
      max-width: 402px;
      min-width: 360px;
    }

    #title {
      anchor-name: --title;
      border-radius: 12px;
      box-sizing: border-box;
      font: var(--cros-button-1-font);
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

  private readonly textTokens = new ScopedAsyncComputed(this, async () => {
    if (this.recordingMetadataSignal.value === null) {
      return null;
    }
    const {textTokens} = await this.recordingDataManager.getTranscription(
      this.recordingMetadataSignal.value.id,
    );
    return textTokens;
  });

  private readonly shouldShowTitleSuggestion = computed(() => {
    const modelState = this.platformHandler.getModelState(
      ModelId.GEMINI_XXS_IT_BASE,
    );
    return (
      modelState.value.kind === 'installed' &&
      settings.value.summaryEnabled === SummaryEnableState.ENABLED &&
      this.textTokens.value !== null && this.textTokens.value.length > 0
    );
  });

  private readonly suggestedTitles = new ScopedAsyncComputed(this, async () => {
    // TODO(pihsun): Cache title suggestion between hide/show the suggestion
    // dialog?
    if (!this.suggestionShown.value || this.recordingMetadata === null ||
        !this.shouldShowTitleSuggestion.value) {
      return null;
    }
    if (this.textTokens.value === null) {
      return null;
    }
    const text = concatTextTokens(this.textTokens.value);
    const model = await this.platformHandler.loadModel(
      ModelId.GEMINI_XXS_IT_BASE,
    );
    try {
      return await model.suggestTitles(text);
      // TODO(pihsun): Handle error.
    } finally {
      model.close();
    }
  });

  private get editTextfield(): Textfield|null {
    return this.shadowRoot?.querySelector('cros-textfield') ?? null;
  }

  private get titleSuggestionDialog(): RecordingTitleSuggestion|null {
    return this.shadowRoot?.querySelector('recording-title-suggestion') ?? null;
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
    const newTarget = ev.relatedTarget;
    if (newTarget !== null && newTarget instanceof Node &&
        (this.editTextfield?.contains(newTarget) ||
         this.titleSuggestionDialog?.contains(newTarget))) {
      // New target is a child of the textfield or title suggestion, don't stop
      // editing.
      return;
    }
    this.editing.value = false;
    this.suggestionShown.value = false;
    // TODO(pihsun): The focusout/blur event got triggered synchronously on
    // render when an element is removed, which breaks the assumption in the
    // reactive/lit.ts implementation of ReactiveLitElement, so setting values
    // above won't trigger rerender. Call requestUpdate() by ourselves to
    // workaround this.
    this.requestUpdate();
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
  }

  private onChangeTitle(ev: Event) {
    const target = assertInstanceof(ev.target, Textfield);
    this.setTitle(target.value);
  }

  private onSuggestTitle(ev: CustomEvent<string>) {
    this.setTitle(ev.detail);
  }

  private openSuggestionDialog() {
    // We focus on the text field before showing the suggestion, so the
    // focusout event from the icon button won't cause edit to be exited.
    // TODO(pihsun): Check a11y on where we should focus in this case.
    this.editTextfield?.focusTextfield();
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
      @focusout=${this.onFocusout}
      @close=${this.closeSuggestionDialog}
      @change=${this.onSuggestTitle}
      .suggestedTitles=${this.suggestedTitles}
    ></recording-title-suggestion>`;
  }

  override render(): RenderResult {
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
            >
              <cra-icon slot="icon" name="pen_spark"></cra-icon>
            </cra-icon-button>`;
      // TODO(pihsun): Handle keyboard event like "enter".
      return html`<cros-textfield
          type="text"
          .value=${this.recordingMetadata?.title ?? ''}
          @change=${this.onChangeTitle}
          @focusout=${this.onFocusout}
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
      >
        ${this.recordingMetadata?.title ?? ''}
        <cra-tooltip>${i18n.titleRenameTooltip}</cra-tooltip>
      </div>
    `;
  }
}

window.customElements.define('recording-title', RecordingTitle);

declare global {
  interface HTMLElementTagNameMap {
    'recording-title': RecordingTitle;
  }
}
