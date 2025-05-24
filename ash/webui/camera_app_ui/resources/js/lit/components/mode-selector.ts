// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  classMap,
  css,
  html,
  LitElement,
  PropertyDeclarations,
  repeat,
} from 'chrome://resources/mwc/lit/index.js';

import {
  assertExists,
  checkEnumVariant,
  checkInstanceof,
} from '../../assert.js';
import {I18nString} from '../../i18n_string.js';
import {getI18nMessage} from '../../models/load_time_data.js';
import {Mode} from '../../type.js';
import {getKeyboardShortcut} from '../../util.js';
import {DEFAULT_STYLE} from '../styles.js';

const MODE_LABELS = {
  [Mode.VIDEO]: {
    ariaLabel: I18nString.SWITCH_RECORD_VIDEO_BUTTON,
    text: I18nString.LABEL_SWITCH_RECORD_VIDEO_BUTTON,
  },
  [Mode.PHOTO]: {
    ariaLabel: I18nString.SWITCH_TAKE_PHOTO_BUTTON,
    text: I18nString.LABEL_SWITCH_TAKE_PHOTO_BUTTON,
  },
  [Mode.SCAN]: {
    ariaLabel: I18nString.SWITCH_SCAN_MODE_BUTTON,
    text: I18nString.LABEL_SWITCH_SCAN_MODE_BUTTON,
  },
  [Mode.PORTRAIT]: {
    ariaLabel: I18nString.SWITCH_TAKE_PORTRAIT_BOKEH_PHOTO_BUTTON,
    text: I18nString.LABEL_SWITCH_TAKE_PORTRAIT_BOKEH_PHOTO_BUTTON,
  },
};

export class ModeSelector extends LitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  static override styles = [
    DEFAULT_STYLE,
    css`
      :host {
        --fade-padding: 24px;
        --scrollbar-height: 4px;
      }

      :host::before,
      :host::after {
        /* This is for "fading" effect when window is narrow and the mode
         * selector overflows, so we use the same color as the background
         * color here. */
        background: linear-gradient(to right, var(--cros-sys-app_base),
                                    transparent);
        content: '';
        display: block;
        height: calc(100% - var(--scrollbar-height));
        pointer-events: none;
        position: absolute;
        top: 0;
        width: var(--fade-padding);
        z-index: 2;
      }

      :host::before {
        left: 0;
      }

      :host::after {
        right: 0;
        transform: scaleX(-1);
      }

      #modes-group {
        font-size: 0; /* Remove space between inline-block. */
        overflow: auto;
        padding: 10px 0;
        text-align: center;
        user-select: none;
        white-space: nowrap;

        &::-webkit-scrollbar-thumb {
          background: var(--cros-sys-scrollbar);
          border-radius: 2px;
          height: auto;
          width: auto;
        }

        &::-webkit-scrollbar {
          height: var(--scrollbar-height);
          width: auto;
        }
      }

      .mode-item {
        display: inline-block;
        margin: 0 8px;
        position: relative;

        &:first-child {
          margin-inline-start: var(--fade-padding);
        }

        &:last-child {
          margin-inline-end: var(--fade-padding);
        }

        &.disabled {
          opacity: 0.38;
        }
      }

      input {
        border-radius: 16px/50%;
        height: 100%;
        outline-offset: 7px;
        position: absolute;
        width: 100%;
        z-index: 1;
      }

      span {
        border-radius: 16px/50%;
        color: var(--cros-sys-on_surface);
        display: inline-block;
        font: var(--cros-button-1-font);
        padding: 8px 12px;
        position: relative;
        z-index: 0;

        input:checked + & {
          background: var(--cros-sys-primary);
          color: var(--cros-sys-inverse_on_surface);
        }
      }
    `,
  ];

  static override properties: PropertyDeclarations = {
    disabled: {type: Boolean},
    selectedMode: {type: Mode},
    supportedModes: {attribute: false},
  };

  /**
   * Whether the mode selector is temporarily disabled.
   */
  disabled = false;

  /**
   * List of modes that should be rendered.
   */
  supportedModes: Mode[] = [];

  /**
   * The current selected mode.
   */
  selectedMode: Mode|null = null;

  private getModeItem(mode: Mode) {
    return checkInstanceof(
        this.renderRoot.querySelector(`input[data-mode=${mode}]`),
        HTMLInputElement);
  }

  private scrollToMode(mode: Mode): void {
    this.getModeItem(mode)?.scrollIntoView({behavior: 'smooth'});
  }

  private handleClick(e: Event) {
    if (this.disabled) {
      e.preventDefault();
      e.stopPropagation();
    }
  }

  private handleChange(e: Event) {
    const target = checkInstanceof(e.target, HTMLInputElement);
    if (target === null) {
      return;
    }
    const mode = checkEnumVariant(Mode, target.dataset['mode']);
    if (mode === null) {
      return;
    }
    this.selectedMode = mode;
    this.dispatchEvent(new CustomEvent('mode-change', {detail: mode}));
  }

  // TODO(pihsun): This corresponds to setupToggles in main.ts. Consider how to
  // extract this so it works easier for all <input> element in lit components.
  private handleKeypress(e: KeyboardEvent) {
    if (getKeyboardShortcut(e) === 'Enter') {
      checkInstanceof(e.target, HTMLElement)?.click();
    }
  }

  private renderModeItem(mode: Mode) {
    const modeItemClass = {
      disabled: this.disabled,
    };
    // Use an additional radio input with same name, so we can utilize the
    // default keyboard navigation behavior of the browser.
    return html`
      <div class="mode-item ${classMap(modeItemClass)}">
        <input type="radio" name="mode"
            @keypress=${this.handleKeypress}
            data-mode=${mode}
            .checked=${this.selectedMode === mode}
            aria-label=${getI18nMessage(MODE_LABELS[mode].ariaLabel)}>
        <span aria-hidden="true">
          ${getI18nMessage(MODE_LABELS[mode].text)}
        </span>
      </div>
    `;
  }

  changeModeForTesting(mode: Mode): void {
    assertExists(this.getModeItem(mode)).click();
  }

  override render(): RenderResult {
    const shownModes = [
      Mode.VIDEO,
      Mode.PHOTO,
      Mode.SCAN,
      Mode.PORTRAIT,
    ].filter((mode) => this.supportedModes.includes(mode));
    const modeItems =
        repeat(shownModes, (mode) => mode, (mode) => this.renderModeItem(mode));
    return html`
      <div id="modes-group"
          role="radiogroup"
          @click=${this.handleClick}
          @change=${this.handleChange}
          aria-label=${getI18nMessage(I18nString.ARIA_CAMERA_MODE_GROUP)}>
        ${modeItems}
      </div>
    `;
  }

  override updated(changedProperties: Map<string, unknown>): void {
    if (this.selectedMode !== null && changedProperties.has('selectedMode')) {
      this.scrollToMode(this.selectedMode);
    }
  }
}

window.customElements.define('mode-selector', ModeSelector);

declare global {
  interface HTMLElementTagNameMap {
    'mode-selector': ModeSelector;
  }
}
