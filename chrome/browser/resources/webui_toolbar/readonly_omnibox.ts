// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {type Range as MojomRange} from '//resources/mojo/ui/gfx/range/mojom/range.mojom-webui.js';

import type {OmniboxViewState} from './browser_proxy.js';
import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, FocusRequestHandle} from './browser_proxy.js';
import {getCss} from './readonly_omnibox.css.js';
import {getHtml} from './readonly_omnibox.html.js';
import {getEventDispositionFlags} from './toolbar_button.js';
import type {OmniboxTextPortion} from './toolbar_ui_api_data_model.mojom-webui.js';
import {FocusRequestTarget, OmniboxTextColor} from './toolbar_ui_api_data_model.mojom-webui.js';

export interface ReadonlyOmniboxElement {
  $: {
    additionalText: HTMLElement,
    inlineAutocomplete: HTMLElement,
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
      // State pushed by browser.
      browserOmniboxState: {type: Object},

      // Current state on this side.
      omniboxViewState: {type: Object},
    };
  }

  accessor browserOmniboxState: OmniboxViewState = {
    browserVersion: 0,
    uiVersion: 0,
    textPieces: [],
    inlineAutocompletion: '',
    additionalText: '',
    selection: null,
    textIsUrl: false,
  };

  accessor omniboxViewState: OmniboxViewState =
      Object.assign(this.browserOmniboxState);

  private focusRequestHandle_: FocusRequestHandle =
      INVALID_FOCUS_REQUEST_HANDLE;

  // The portion of the text that the user entered or accepted (rather than
  // what's being merely suggested by inline autocompletion).
  private userText: string = '';

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  // Keys that may need to be forwarded to the browser.
  private maybeForwardKeys: Set<string>;

  constructor() {
    super();
    this.maybeForwardKeys = new Set([
      'Control',
      'Enter',
      'Escape',
      'ArrowUp',
      'ArrowDown',
      ' ',
      'Backspace',
    ]);
  }

  override connectedCallback() {
    super.connectedCallback();
    this.focusRequestHandle_ = this.browserProxy_.addFocusRequestListener(
        this.onFocusRequest.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    this.browserProxy_.removeFocusRequestListener(this.focusRequestHandle_);
  }

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);
    if (changedProperties.has('browserOmniboxState')) {
      // Updates are accepted either if browser version changes, or if the
      // ui version matches.
      if ((this.browserOmniboxState.browserVersion ===
           this.omniboxViewState.browserVersion) &&
          (this.browserOmniboxState.uiVersion !==
           this.omniboxViewState.uiVersion)) {
        return;
      }

      this.omniboxViewState = Object.assign(this.browserOmniboxState);
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    this.$.textContainerWrap.addEventListener(
        'focus', this.onWrapFocus.bind(this));
    const textInput = this.$.textInput;
    textInput.addEventListener('focus', this.onInputFocus.bind(this));
    textInput.addEventListener('blur', this.onInputBlur.bind(this));
    textInput.addEventListener('input', this.onInputInput.bind(this));
    textInput.addEventListener('keydown', this.onInputKeyDown.bind(this));
    textInput.addEventListener('keyup', this.onInputKeyUp.bind(this));
  }

  override updated(changedProperties: PropertyValues<this>): void {
    super.updated(changedProperties);
    if (changedProperties.has('omniboxViewState')) {
      this.$.textContainer.classList.toggle(
          'force-ltr', this.omniboxViewState.textIsUrl);

      this.userText = this.$.textContainer.textContent;
      let selection = this.omniboxViewState.selection;

      // If there is an inline autocompletion, render it as selected text
      // after the input.
      // TODO(crbug.com/500653057): We will likely need to do something
      // different when IME is popped up.
      if (this.omniboxViewState.inlineAutocompletion.length > 0) {
        selection = {
          start: this.userText.length,
          end: this.userText.length +
              this.omniboxViewState.inlineAutocompletion.length,
        };
      }

      const allText =
          this.userText + this.omniboxViewState.inlineAutocompletion;
      if (this.$.textInput.value !== allText) {
        this.$.textInput.value = allText;
      }

      if (selection) {
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

  private onFocusRequest(target: FocusRequestTarget): void {
    // TODO(crbug.com/503784990): Proper implementation.
    switch (target) {
      case FocusRequestTarget.kLocationBar:
      case FocusRequestTarget.kLocationBarUserInitiated:
      case FocusRequestTarget.kSearch:
        this.$.textInput.focus();
        break;

      default:
        // Not relevant here.
        return;
    }
  }

  private onInputBlur(): void {
    // Make the read-only view visible.
    this.$.textContainer.style.zIndex = '1';

    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      focusChange: {
        hasFocus: false,
        selection: this.getSelection(),
      },
    });
  }

  private onInputFocus(): void {
    // Make the <input> visible.
    this.$.textContainer.style.zIndex = '-1';

    // TODO(crbug.com/500653057): The Views behavior is pretty subtle here: it
    // mostly selects all but you can drag-select to get that specific
    // selection. Focus restore only seems to happen on keyboard focus.
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      focusChange: {
        hasFocus: true,
        selection: this.getSelection(),
      },
    });
  }

  // Sync ups the textPieces to be an unhighlighted version of `userText`.
  private updateTextPiecesFromUserText() {
    this.omniboxViewState.textPieces = [{
      text: this.userText,
      strikethrough: false,
      color: OmniboxTextColor.kOmniboxText,
    }];
    this.requestUpdate();  // Since our changes were deep.
  }

  private onInputInput(): void {
    // If we got here (rather than blocking things in onInputKeyDown),
    // there is no longer any inline completion.
    this.userText = this.$.textInput.value;

    // Sync up the read-only view to have the right text.
    ++this.omniboxViewState.uiVersion;
    this.omniboxViewState.inlineAutocompletion = '';
    this.updateTextPiecesFromUserText();

    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      textInput: {
        uiVersion: this.omniboxViewState.uiVersion,
        browserVersion: this.omniboxViewState.browserVersion,
        text: this.$.textInput.value,
        inlineAutocompletion: '',
        selection: this.getSelection(),
      },
    });
  }

  private onInputKeyDown(event: KeyboardEvent): void {
    const inlineAutocompletion = this.omniboxViewState.inlineAutocompletion;
    if (inlineAutocompletion.length > 0) {
      // If the current input state (its value and selection) matches its last
      // state (text and inline autocompletion) and the user types the next
      // character in the inline autocompletion, stop the keydown event. Just
      // move the selection. This is needed to avoid flicker. (Shamelessly
      // adapted from searchbox_input.ts).
      const inputValue = this.$.textInput.value;
      let textPortionLength = this.$.textInput.selectionStart!;
      const inputSelection = inputValue.substring(
          textPortionLength, this.$.textInput.selectionEnd!);
      if (inlineAutocompletion[0]!.toLocaleLowerCase() ===
              event.key.toLocaleLowerCase() &&
          inputSelection === inlineAutocompletion &&
          inputValue === (this.userText + inlineAutocompletion)) {
        ++textPortionLength;
        this.$.textInput.selectionStart = textPortionLength;
        this.userText = inputValue.substr(0, textPortionLength);
        this.omniboxViewState.inlineAutocompletion =
            inlineAutocompletion.substr(1);
        ++this.omniboxViewState.uiVersion;
        this.updateTextPiecesFromUserText();

        this.browserProxy_.toolbarUIHandler.onOmniboxAction({
          textInput: {
            uiVersion: this.omniboxViewState.uiVersion,
            browserVersion: this.omniboxViewState.browserVersion,
            text: this.userText,
            inlineAutocompletion: this.omniboxViewState.inlineAutocompletion,
            selection: {
              start: textPortionLength,
              end: textPortionLength,
            },
          },
        });

        event.preventDefault();
        return;
      }
    }

    if (this.maybeForwardKeys.has(event.key)) {
      // TODO(crbug.com/503785596): shouldn't do this if shift is down.
      if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
        event.preventDefault();
      }

      // Backspace is only relevant to the other end if we're at the very
      // beginning (where it deletes the search keyword rather than a
      // character).
      if (event.key === 'Backspace' &&
          (this.$.textInput.selectionStart! !== 0 ||
           this.$.textInput.selectionEnd! !== 0)) {
        return;
      }

      this.browserProxy_.toolbarUIHandler.onOmniboxAction({
        key: {
          key: event.key,
          isKeyDown: true,
          selection: this.getSelection(),
          modifiers: getEventDispositionFlags(event),
        },
      });
    }
  }

  private onInputKeyUp(event: KeyboardEvent): void {
    // OmniboxEditModel keeps track of state of control key separately, and
    // needs to be notified of its releases. Everything else is handled on
    // keydown.
    if (event.key === 'Control') {
      this.browserProxy_.toolbarUIHandler.onOmniboxAction({
        key: {
          key: event.key,
          isKeyDown: false,
          selection: this.getSelection(),
          modifiers: getEventDispositionFlags(event),
        },
      });
    }
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
