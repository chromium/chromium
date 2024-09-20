// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/textfield/textfield.js';
import './cra/cra-icon.js';
import './cra/cra-icon-button.js';

import {
  Textfield,
} from 'chrome://resources/cros_components/textfield/textfield.js';
import {
  classMap,
  createRef,
  css,
  html,
  nothing,
  ref,
} from 'chrome://resources/mwc/lit/index.js';

import {i18n} from '../core/i18n.js';
import {ReactiveLitElement} from '../core/reactive/lit.js';
import {computed, signal} from '../core/reactive/signal.js';
import {assertExists} from '../core/utils/assert.js';
import {AsyncJobQueue} from '../core/utils/async_job_queue.js';

import {CraIconButton} from './cra/cra-icon-button.js';

export class RecordingSearchBox extends ReactiveLitElement {
  static override styles = css`
    :host {
      --icon-button-width: 48px;
      --textfield-width: 224px;
    }

    #container {
      align-items: center;
      display: flex;

      & > cra-icon-button {
        transition:
          opacity 150ms 50ms var(--cros-ref-motion-easing-standard-decelerate),
          transform 200ms var(--cros-ref-motion-easing-standard);
      }

      &.opened > cra-icon-button {
        opacity: 0;
        transform: translateX(
          calc(-1 * (var(--textfield-width) - var(--icon-button-width)))
        );
        transition:
          opacity 50ms var(--cros-ref-motion-easing-standard-accelerate),
          transform 200ms var(--cros-ref-motion-easing-standard);
      }
    }

    cros-textfield {
      /*
       * The margin and width are chosen so it perfectly align with the icon on
       * the start of transition.
       */
      margin-right: -48px;

      /*
       * The transition property is not visibility but opacity here because
       * visibility mess up with focus and somehow we can't auto-focus on the
       * textfield afterward.
       * The text field is disabled when it's not shown, so it won't capture
       * focus even with opacity: 0.
       */
      opacity: 0;
      transition:
        opacity 50ms var(--cros-ref-motion-easing-standard-accelerate),
        width 200ms var(--cros-ref-motion-easing-standard);
      width: 48px;

      #container.opened & {
        opacity: 1;
        transition:
          opacity 150ms 50ms var(--cros-ref-motion-easing-standard-decelerate),
          width 200ms var(--cros-ref-motion-easing-standard);
        width: 224px;
      }
    }
  `;

  private readonly query = signal('');

  private readonly opened = signal(false);

  private readonly hasQuery = computed(
    () => this.opened.value && this.query.value.trim().length !== 0,
  );

  private readonly textfieldRef = createRef<Textfield>();

  private readonly searchButtonRef = createRef<CraIconButton>();

  private readonly autofocusQueue = new AsyncJobQueue('keepLatest');

  private openSearchBox() {
    this.opened.value = true;
    const textfield = assertExists(this.textfieldRef.value);
    this.autofocusQueue.push(async () => {
      // Wait for self update so the disabled state is updated on the textfield,
      // and the textfield to update so the underlying textfield is focusable.
      await this.updateComplete;
      await textfield.updateComplete;
      textfield.focusTextfield();
    });
  }

  private updateQuery(newQuery: string) {
    this.query.value = newQuery;
    this.dispatchEvent(new CustomEvent('query-changed', {detail: newQuery}));
  }

  private clearSearchBox() {
    this.updateQuery('');
    if (this.textfieldRef.value !== undefined) {
      this.textfieldRef.value.value = '';
    }
  }

  private closeSearchBox() {
    this.opened.value = false;
    this.clearSearchBox();
    const searchButton = assertExists(this.searchButtonRef.value);
    this.autofocusQueue.push(async () => {
      // Wait for self update so the disabled state is updated on the search
      // button, and the button to update so it's focusable.
      await this.updateComplete;
      await searchButton.updateComplete;
      searchButton.focus();
    });
  }

  private onFocusOut() {
    if (!this.hasQuery.value) {
      this.closeSearchBox();
    }
  }

  private onKeyDown(e: KeyboardEvent) {
    const key = e.key;
    if (key === 'Escape') {
      if (this.hasQuery.value) {
        this.clearSearchBox();
      } else {
        this.closeSearchBox();
      }
    }
  }

  private onInputUpdated() {
    this.updateQuery(assertExists(this.textfieldRef.value).value);
  }

  override render(): RenderResult {
    const searchButton = html`<cra-icon-button
      buttonstyle="floating"
      @click=${this.openSearchBox}
      ?disabled=${this.opened.value}
      aria-hidden=${this.opened.value}
      aria-label=${i18n.recordingListSearchButtonTooltip}
      ${ref(this.searchButtonRef)}
    >
      <cra-icon slot="icon" name="search"></cra-icon>
    </cra-icon-button>`;

    const cancelButton = !this.hasQuery.value ? nothing : html`<cra-icon-button
          buttonstyle="floating"
          size="small"
          slot="trailing"
          shape="circle"
          @click=${this.closeSearchBox}
          aria-label=${i18n.recordingListSearchBoxClearButtonAriaLabel}
        >
          <cra-icon slot="icon" name="remove_fill"></cra-icon>
        </cra-icon-button>`;

    // TODO(pihsun): The textfield is disabled a bit too early and can be seen
    // "fade out" on the slowed down animation. It'd be nicer if we can
    // transition "visibility"/"display" instead of "opacity" so we don't
    // need to disable the search box here, or disable the search box only
    // after the transition is completed.
    const searchBox = html`<cros-textfield
      ?disabled=${!this.opened.value}
      aria-hidden=${!this.opened.value}
      placeholder=${i18n.recordingListSearchBoxPlaceholder}
      shaded
      type="text"
      @focusout=${this.onFocusOut}
      @keydown=${this.onKeyDown}
      @input=${this.onInputUpdated}
      ${ref(this.textfieldRef)}
    >
      <cra-icon-button
        ?disabled=${!this.opened.value}
        buttonstyle="floating"
        size="small"
        slot="leading"
        @click=${this.closeSearchBox}
        aria-label=${i18n.recordingListSearchBoxCloseButtonAriaLabel}
      >
        <cra-icon slot="icon" name="search"></cra-icon>
      </cra-icon-button>
      ${cancelButton}
    </cros-textfield>`;

    const classes = {
      opened: this.opened.value,
    };

    return html`<div id="container" class=${classMap(classes)}>
      ${searchBox}${searchButton}
    </div>`;
  }
}

window.customElements.define('recording-search-box', RecordingSearchBox);

declare global {
  interface HTMLElementTagNameMap {
    'recording-search-box': RecordingSearchBox;
  }
}
