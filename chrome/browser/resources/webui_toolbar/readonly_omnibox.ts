// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {type Range as MojomRange} from '//resources/mojo/ui/gfx/range/mojom/range.mojom-webui.js';

import type {OmniboxViewState} from './browser_proxy.js';
import {BrowserProxyImpl} from './browser_proxy.js';
import type {BrowserProxy} from './browser_proxy.js';
import {getCss} from './readonly_omnibox.css.js';
import {getHtml} from './readonly_omnibox.html.js';
import type {OmniboxTextPortion} from './toolbar_ui_api_data_model.mojom-webui.js';
import {OmniboxActionName, OmniboxTextColor} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface ReadonlyOmniboxElement {
  $: {
    textContainer: HTMLElement,
    textContainerWrap: HTMLElement,
    textInput: HTMLInputElement,
  };
}

// TODO(crbug.com/500653057): Rename since it's no longer readonly.
export class ReadonlyOmniboxElement extends CrLitElement {
  static get is() {
    return 'readonly-omnibox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxViewState: {type: Object},
    };
  }

  accessor omniboxViewState: OmniboxViewState = {
    textPieces: [],
    selection: null,
    textIsUrl: false,
  };

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    this.$.textContainerWrap.addEventListener(
        'focus', this.onWrapFocus.bind(this));
    const textInput = this.$.textInput;
    textInput.addEventListener('focus', this.onInputFocus.bind(this));
    textInput.addEventListener('blur', this.onInputBlur.bind(this));
    textInput.addEventListener('input', this.onInputInput.bind(this));
    textInput.addEventListener('keydown', this.onInputKeyDown.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('omniboxViewState')) {
      this.$.textContainer.classList.toggle(
          'force-ltr', this.omniboxViewState.textIsUrl);

      const allText = this.$.textContainer.textContent;
      if (this.$.textInput.value !== allText) {
        this.$.textInput.value = allText;
      }

      if (this.omniboxViewState.selection) {
        let selection = this.omniboxViewState.selection;
        let selectionDirection: SelectionDirection = 'forward';
        if (selection.start > selection.end) {
          selection = {start: selection.end, end: selection.start};
          selectionDirection = 'backward';
        }

        this.$.textInput.setSelectionRange(
            selection.start, selection.end, selectionDirection);
      }

      if (!this.hasFocus()) {
        // Make sure we make the beginning of the line visible when we're not
        // focused.
        this.$.textContainer.scrollLeft = 0;
      }
    }
  }

  private hasFocus(): boolean {
    return document.hasFocus() &&
        this.shadowRoot.activeElement === this.$.textInput;
  }

  private onInputBlur(): void {
    // Make the read-only view visible.
    this.$.textContainer.style.zIndex = '1';

    // TODO(crbug.com/500653057): sync up the readonly view to avoid roundtrip/
    // flicker.
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      name: OmniboxActionName.kBlur,
      text: '',
      selection: this.getSelection(),
    });
  }

  private onInputFocus(): void {
    // Make the <input> visible.
    this.$.textContainer.style.zIndex = '-1';

    // TODO(crbug.com/500653057): The Views behavior is pretty subtle here: it
    // mostly selects all but you can drag-select to get that specific
    // selection. Focus restore only seems to happen on keyboard focus.
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      name: OmniboxActionName.kFocus,
      text: '',
      selection: this.getSelection(),
    });
  }

  private onInputInput(): void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      name: OmniboxActionName.kTextInput,
      text: this.$.textInput.value,
      selection: this.getSelection(),
    });
  }

  private onInputKeyDown(event: KeyboardEvent): void {
    // TODO(crbug.com/500653057): shouldn't do this if shift is down.
    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      event.preventDefault();
    }
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      name: OmniboxActionName.kKeyDown,
      text: event.key,
      selection: this.getSelection(),
    });
  }

  private getSelection(): MojomRange {
    // selectionStart/End should work since <input> is of appropriate type
    // for them.
    let selection: MojomRange = {
      start: this.$.textInput.selectionStart || 0,
      end: this.$.textInput.selectionEnd || 0,
    };

    if (this.$.textInput.selectionDirection === 'backward') {
      selection = {
        end: selection.start,
        start: selection.end,
      };
    }

    return selection;
  }

  private onWrapFocus(): void {
    // We forward focus requests from the entirety of textContainerWrap to
    // textInput.
    this.$.textInput.focus();
  }

  // Returns the CSS classes for rendering the given text piece.
  static getTextPieceClasses(piece: OmniboxTextPortion): string {
    const classes = [];
    switch (piece.color) {
      case OmniboxTextColor.kOmniboxTextDimmed:
        classes.push('color-dim');
        break;
      case OmniboxTextColor.kOmniboxForegroundDisabled:
        classes.push('color-foreground-disabled');
        break;
      case OmniboxTextColor.kOmniboxSecurityChipDangerous:
        classes.push('color-danger');
        break;
      case OmniboxTextColor.kUnspecified:
        console.error('Unexected kUnspecified for text color');
        break;
      case OmniboxTextColor.kOmniboxText:
        // The default is fine.
        break;
      default:
        assertNotReachedCase(piece.color);
    }
    if (piece.strikethrough) {
      classes.push('strikethrough');
    }
    return classes.join(' ');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'readonly-omnibox': ReadonlyOmniboxElement;
  }
}

customElements.define(ReadonlyOmniboxElement.is, ReadonlyOmniboxElement);
