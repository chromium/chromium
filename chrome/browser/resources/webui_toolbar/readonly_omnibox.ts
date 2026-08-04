// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeTextForPaste} from '//resources/cr_components/searchbox/utils.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {type Range as MojomRange} from '//resources/mojo/ui/gfx/range/mojom/range.mojom-webui.js';
import type {AdjustOmniboxTextForCopyResult} from '/shared/toolbar_ui_api.mojom-webui.js';
import type {OmniboxActionDropFile, OmniboxActionDropText, OmniboxActionFocusChange, OmniboxActionPointer, OmniboxActionTextInput, OmniboxTextPortion, OmniboxViewState} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {FocusRequestTarget, OmniboxTextColor} from '/shared/toolbar_ui_api_data_model.mojom-webui.js';
import {getFaviconUrl} from 'chrome://resources/js/icon.js';

import {BrowserProxyImpl, INVALID_FOCUS_REQUEST_HANDLE} from './browser_proxy.js';
import type {BrowserProxy, FocusRequestHandle} from './browser_proxy.js';
import {getCss} from './readonly_omnibox.css.js';
import {getHtml} from './readonly_omnibox.html.js';
import {getEventDispositionFlags} from './toolbar_button.js';

export interface ReadonlyOmniboxElement {
  $: {
    additionalText: HTMLElement,
    dragTemplate: HTMLElement,
    inlineAutocomplete: HTMLElement,
    textContainer: HTMLElement,
    textContainerWrap: HTMLElement,
    textInput: HTMLInputElement,
  };
}

interface OmniboxInputDelegate {
  handleFocusChange(
      element: ReadonlyOmniboxElement, focusOp: OmniboxActionFocusChange): void;
  handleTextInput(
      element: ReadonlyOmniboxElement, textInput: OmniboxActionTextInput): void;
  handleKey(
      element: ReadonlyOmniboxElement, isKeyUp: boolean,
      event: KeyboardEvent): void;
  handlePointer(element: ReadonlyOmniboxElement, pointer: OmniboxActionPointer):
      void;
  handleDropText(
      element: ReadonlyOmniboxElement, dragText: OmniboxActionDropText): void;
  handleDropFile(
      element: ReadonlyOmniboxElement, dragFile: OmniboxActionDropFile): void;
}

// Implementation of input handling that works by forwarding the relevant
// events to the browser via Mojo.
class MojoOmniboxInputDelegate implements OmniboxInputDelegate {
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();
  // Keys that may need to be forwarded to the browser.
  private maybeForwardKeys_: Set<string> = new Set([
    'Control',
    'Enter',
    'Escape',
    'ArrowUp',
    'ArrowDown',
    ' ',
    'Backspace',
    'Delete',
    'PageUp',
    'PageDown',
    'Tab',
  ]);

  handleFocusChange(
      _: ReadonlyOmniboxElement, focusChange: OmniboxActionFocusChange): void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      focusChange,
    });
  }

  handleTextInput(
      _element: ReadonlyOmniboxElement,
      textInput: OmniboxActionTextInput): void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      textInput,
    });
  }

  handleKey(
      element: ReadonlyOmniboxElement, isKeyUp: boolean,
      event: KeyboardEvent): void {
    if (!this.maybeForwardKeys_.has(event.key)) {
      return;
    }

    // OmniboxEditModel keeps track of state of control key separately, and
    // needs to be notified of its releases. Everything else is handled on
    // keydown.
    if (isKeyUp && event.key !== 'Control') {
      return;
    }

    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      // Shift+Down/Up does selection, plain Down/Up navigates suggestions.
      if (!event.shiftKey) {
        event.preventDefault();
      } else {
        return;
      }
    }

    // Backspace is only relevant to the other end if we're at the very
    // beginning (where it deletes the search keyword rather than a
    // character).
    if (event.key === 'Backspace' && !element.isCaretAtStart()) {
      return;
    }

    // Shift-Delete can delete suggestion entries.
    if (event.key === 'Delete') {
      if (event.shiftKey && element.isPopupOpen) {
        event.preventDefault();
      } else {
        return;
      }
    }

    // Page keys navigate selection unless modifiers are pressed.
    if (event.key === 'PageUp' || event.key === 'PageDown') {
      if (!event.ctrlKey && !event.altKey && !event.shiftKey) {
        event.preventDefault();
      } else {
        return;
      }
    }

    if (event.key === 'Tab') {
      // See FocusManager::IsTabTraversalKeyEvent
      if (!event.ctrlKey && !event.altKey && element.isPopupOpen) {
        event.preventDefault();
      } else {
        return;
      }
    }

    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      key: {
        key: event.key,
        isKeyDown: !isKeyUp,
        selection: element.getMojoSelection(),
        modifiers: getEventDispositionFlags(event),
      },
    });
  }

  handlePointer(_: ReadonlyOmniboxElement, pointer: OmniboxActionPointer):
      void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      pointer,
    });
  }

  handleDropText(_: ReadonlyOmniboxElement, dropText: OmniboxActionDropText):
      void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      dropText,
    });
  }

  handleDropFile(_: ReadonlyOmniboxElement, dropFile: OmniboxActionDropFile):
      void {
    this.browserProxy_.toolbarUIHandler.onOmniboxAction({
      dropFile,
    });
  }
}

enum UnelisionGesture {
  HOME_KEY_PRESSED,
  MOUSE_RELEASE,
  DOUBLE_CLICK,
  OTHER,
}

function isOnlyLeftButton(event: MouseEvent): boolean {
  // Left button has button # 0, and mask 1. We allow it to be both on
  // and off in buttons to handle both mousedown and mouseup.
  return event.button === 0 && (event.buttons === 0 || event.buttons === 1);
}

function copyMaybeSelection(selection: MojomRange|null): MojomRange|null {
  if (!selection) {
    return null;
  } else {
    return Object.assign(selection);
  }
}

// Movement threshold (in pixels) for touch and pen pointer events to
// distinguish a single tap from a drag-select gesture. Set to 10px to align
// with: 1) Chrome's native stylus click slop (10px in
//    ui/events/gesture_detection/gesture_configuration_aura.cc),
// 2) Chrome's touch slop range (6px on ChromeOS in
//    ui/events/gesture_detection/gesture_configuration_aura.cc up to 15px
//    default in ui/events/gesture_detection/gesture_configuration.h).
const POINTER_DRAG_THRESHOLD_PX = 10;

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

      // True if the IME is currently active.
      isComposing: {type: Boolean},

      adjustedCopyResult: {type: Object},

      isPopupOpen: {type: Boolean},
    };
  }

  accessor browserOmniboxState: OmniboxViewState = {
    browserVersion: 0,
    uiVersion: 0,
    formattedFullUrl: '',
    textPieces: [],
    placeholder: null,
    inlineAutocompletion: '',
    additionalText: '',
    // This follows the semantics of gfx::Range, where backwards
    // direction is indicated by having `selection.start` > `selection.end`.
    selection: null,
    textIsUrl: false,
    userInputInProgress: false,
  };

  accessor omniboxViewState: OmniboxViewState =
      Object.assign(this.browserOmniboxState);

  accessor isComposing: boolean = false;
  accessor adjustedCopyResult: AdjustOmniboxTextForCopyResult|null = null;
  accessor isPopupOpen: boolean = false;

  private focusRequestHandle_: FocusRequestHandle =
      INVALID_FOCUS_REQUEST_HANDLE;

  // The portion of the text that the user entered or accepted (rather than
  // what's being merely suggested by inline autocompletion).
  private userText: string = '';

  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  // If this is true, the sequence of events thus far suggests that the next
  // mouse release should select all.
  private selectAllOnMouseRelease_: boolean = false;

  // Records whether handling of first click's mouse up selected all text.
  private didSelectAllOnClickOne_: boolean = false;

  // The time when we last acquired focus. This is used so that mouse down
  // handling can tell whether it previously had focus or it was acquired
  // immediately before. `null` if there is no focus.
  private lastFocusAcquisition_: number|null = null;
  private isDraggingFromSelf_: boolean = false;
  private onSelectionChangeBound_ = this.onSelectionChange_.bind(this);

  // Bitmap of mouse buttons down. This is using `event.button` as bit position,
  // not their position in `event.buttons`, as that's what's most convenient to
  // use for updates in down/up event. (The two representations are not in the
  // same order).
  private mouseButtonDown_: number = 0;

  // These are the mouse coordinates captured when `selectAllOnMouseRelease_`
  // was set to true.
  private clientXAtMouseDown_: number = 0;
  private clientYAtMouseDown_: number = 0;

  // If this is true, the sequence of events thus far suggests that the next
  // touch release should select all.
  private selectAllOnTouchRelease_: boolean = false;
  private clientXAtTouchDown_: number = 0;
  private clientYAtTouchDown_: number = 0;
  // Tracks active touch/pen pointer IDs to detect multi-finger gestures.
  private activeTouchIds_: Set<number> = new Set();
  // Remembers if the current touch sequence involved multiple touch points at
  // any time, ensuring that release of the final finger does not trigger
  // single-tap select-all.
  private wasMultiTouch_: boolean = false;

  private inputDelegate_: OmniboxInputDelegate = new MojoOmniboxInputDelegate();

  constructor() {
    super();
  }

  override connectedCallback() {
    super.connectedCallback();
    this.focusRequestHandle_ = this.browserProxy_.addFocusRequestListener(
        this.onFocusRequest.bind(this));
    document.addEventListener('selectionchange', this.onSelectionChangeBound_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.browserProxy_.removeFocusRequestListener(this.focusRequestHandle_);
    document.removeEventListener(
        'selectionchange', this.onSelectionChangeBound_);
  }

  override willUpdate(changedProperties: PropertyValues<this>): void {
    super.willUpdate(changedProperties);
    if (changedProperties.has('browserOmniboxState')) {
      // Updates are accepted either if browser version changes, or if the
      // ui version matches.
      if ((this.browserOmniboxState.browserVersion !==
           this.omniboxViewState.browserVersion) ||
          (this.browserOmniboxState.uiVersion ===
           this.omniboxViewState.uiVersion)) {
        this.omniboxViewState = {
          ...this.browserOmniboxState,
          // Don't pay attention to browser selection beyond initial state.
          // This deep-copies to avoid aliasing issues. Not changing should
          // be done by null, not by reusing OmniboxViewState.selection,
          // since it's basically impossible to keep that fully up-to-date.
          selection: this.browserOmniboxState.uiVersion === 0 ?
              copyMaybeSelection(this.browserOmniboxState.selection) :
              null,
        };
      }
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>): void {
    super.firstUpdated(changedProperties);
    this.$.textContainerWrap.addEventListener(
        'focus', this.onWrapFocus.bind(this));
    const textInput = this.$.textInput;
    textInput.addEventListener('focus', this.onInputFocus.bind(this));
    textInput.addEventListener('blur', this.onInputBlur.bind(this));
    // Handle gesture/pointer events for touch interactions.
    textInput.addEventListener(
        'pointerdown', this.onInputPointerDown_.bind(this));
    textInput.addEventListener('pointerup', this.onInputPointerUp_.bind(this));
    textInput.addEventListener(
        'pointermove', this.onInputPointerMove_.bind(this));
    textInput.addEventListener(
        'pointercancel', this.onInputPointerCancel_.bind(this));
    textInput.addEventListener('mousedown', this.onInputMouseDown.bind(this));
    textInput.addEventListener('mouseup', this.onInputMouseUp.bind(this));
    textInput.addEventListener('mousemove', this.onInputMouseMove_.bind(this));
    textInput.addEventListener('input', this.onInputInput.bind(this));
    textInput.addEventListener('keydown', this.onInputKeyDown.bind(this));
    textInput.addEventListener('keyup', this.onInputKeyUp.bind(this));
    textInput.addEventListener('copy', this.onInputCopy_.bind(this));
    textInput.addEventListener('cut', this.onInputCut_.bind(this));
    textInput.addEventListener('paste', this.onInputPaste_.bind(this));
    textInput.addEventListener(
        'compositionstart', this.onInputCompositionstart_.bind(this));
    textInput.addEventListener(
        'compositionend', this.onInputCompositionend_.bind(this));

    this.addEventListener('contextmenu', this.onContextMenu_.bind(this));
    this.addEventListener('dragstart', this.onDragStart_.bind(this));
    this.addEventListener('dragend', this.onDragEnd_.bind(this));
    this.addEventListener('dragenter', this.onDragEnter_.bind(this));
    this.addEventListener('dragleave', this.onDragLeave_.bind(this));
    this.addEventListener('dragover', this.onDragOver_.bind(this));
    this.addEventListener('drop', this.onDrop_.bind(this));
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
      if (this.omniboxViewState.inlineAutocompletion.length > 0 &&
          !this.isComposing) {
        selection = {
          start: this.userText.length,
          end: this.userText.length +
              this.omniboxViewState.inlineAutocompletion.length,
        };
      }

      const allText = this.userText +
          (this.isComposing ? '' : this.omniboxViewState.inlineAutocompletion);
      if (this.$.textInput.value !== allText) {
        this.$.textInput.value = allText;
      }

      if (selection) {
        let selectionDirection: SelectionDirection = 'forward';
        if (selection.start > selection.end) {
          selection = {start: selection.end, end: selection.start};
          selectionDirection = 'backward';
        }

        this.setSelection(selection.start, selection.end, selectionDirection);
      }

      // Make sure we set the right view visible. Normally we want the <input>
      // when we have focus, but it's also responsible for drawing the
      // placeholder if it's enabled.
      const hasFocus = this.hasFocus();

      this.switchView_(hasFocus);
      if (!hasFocus) {
        // Make sure we make the beginning of the line visible when we're not
        // focused.
        this.$.textContainer.scrollLeft = 0;
      }
    }
  }

  hasFocus(): boolean {
    return (
        document.hasFocus() &&
        this.shadowRoot.activeElement === this.$.textInput);
  }

  // Focus requests that come from the browser, rather than direct interaction
  // like clicking to focus.
  //
  // This includes some key shortcuts (Ctrl-L, Ctrl-K) and the browser
  // auto-focusing the location bar for some pages (the NTP and about:blank).
  private onFocusRequest(target: FocusRequestTarget): void {
    let isUserInitiated = false;
    let activateDefaultSearch = false;
    switch (target) {
      case FocusRequestTarget.kLocationBar:
        // Default values of flags are fine.
        break;

      case FocusRequestTarget.kLocationBarUserInitiated:
        isUserInitiated = true;
        break;

      case FocusRequestTarget.kSearch:
        isUserInitiated = true;
        activateDefaultSearch = true;
        break;

      default:
        // Not relevant here.
        return;
    }

    const wasAlreadyFocused = this.hasFocus();
    if (activateDefaultSearch && !this.omniboxViewState.userInputInProgress) {
      // If activateDefaultSearch is on, and text has not been entered,
      // the search will activate with empty box. Do that on this side
      // as well to avoid flicker.
      this.$.textInput.value = '';
      this.updateStateFromTextInput();
    } else if (isUserInitiated) {
      this.unelide();
    }
    this.$.textInput.focus();
    this.switchView_(/*hasFocus=*/ true);

    // The following comments are from OmniboxViewViews::SetFocus:
    // If the user initiated the focus, then we always select-all, even if the
    // omnibox is already focused. This can happen if the user pressed Ctrl+L
    // while already typing in the omnibox.
    //
    // For renderer initiated focuses (like NTP or about:blank page load
    // finish):
    //  - If the omnibox was not already focused, select-all. This handles the
    //    about:blank homepage case, where the location bar has initial focus.
    //    It annoys users if the URL is not pre-selected.
    //    https://crbug.com/40402896.
    //  - If the omnibox is already focused, DO NOT select-all. This can happen
    //    if the user starts typing before the NTP finishes loading. If the NTP
    //    finishes loading and then does a renderer-initiated focus, performing
    //    a select-all here would surprisingly overwrite the user's first few
    //    typed characters. https://crbug.com/40610912.
    if (isUserInitiated || !wasAlreadyFocused) {
      if (activateDefaultSearch) {
        this.selectAllForward();
      } else {
        this.selectAllBackwards();
      }
    }
    // It's important this is done after updating the selection since that
    // prevents inline completion, which isn't desired for these shortcuts.
    this.sendInputToBrowser();

    this.inputDelegate_.handleFocusChange(this, {
      hasFocus: true,
      selection: this.getMojoSelection(),
      requestClearKeyword: wasAlreadyFocused,
      startZeroSuggest: isUserInitiated,
      activateDefaultSearch: activateDefaultSearch,
    });
  }

  private onInputBlur(): void {
    // Blink has somewhat strange behavior when it comes to mouse interaction
    // w/elements that lost focus, particularly due to their document losing
    // focus: the selection isn't visible, but on click it acts as if it's
    // there, so for example trying to drag-select in an element with a
    // "secret" select all state (quite common for the location bar!) results
    // in a text drag instead.
    //
    // So, if we lose focus due to document losing focus, clear both focus
    // and selection, so mouse interactions are more predictable. Unfortunately
    // this does result in location bar losing selection on window switch.
    //
    // TODO(crbug.com/503784990): Perhaps there is a better way.
    if (!document.hasFocus()) {
      document.getSelection()!.removeAllRanges();
      this.$.textInput.blur();
    }
    this.switchView_(/*hasFocus=*/ false);
    this.lastFocusAcquisition_ = null;

    this.inputDelegate_.handleFocusChange(this, {
      hasFocus: false,
      selection: this.getMojoSelection(),
      requestClearKeyword: false,
      startZeroSuggest: false,
      activateDefaultSearch: false,
    });
  }

  private onInputFocus(): void {
    this.lastFocusAcquisition_ = performance.now();
    this.switchView_(/*hasFocus=*/ true);

    this.inputDelegate_.handleFocusChange(this, {
      hasFocus: true,
      selection: this.getMojoSelection(),
      requestClearKeyword: false,
      startZeroSuggest: false,
      activateDefaultSearch: false,
    });
  }

  private onInputMouseDown(event: MouseEvent): void {
    this.mouseButtonDown_ |= (1 << event.button);

    let wasAlreadyFocused = this.hasFocus();

    // Don't count us as already focused when we just got focus from this
    // very click. 100ms matches views::kMinimumTimeBetweenButtonClicks.
    if (wasAlreadyFocused && this.lastFocusAcquisition_ !== null &&
        (performance.now() - this.lastFocusAcquisition_ < 100)) {
      wasAlreadyFocused = false;
    }

    const input = this.$.textInput;

    // Normally, we will select-all when the user releases the button.
    //
    // This won't happen at least when:
    // 1) This already has focus, in which case they'll just want to set the
    //    caret.
    // 2) More than just left mouse button is down.
    // 3) The user-drag selects, which clears `selectAllOnMouseRelease_` in
    //    onInputMouseMove_.
    this.selectAllOnMouseRelease_ =
        isOnlyLeftButton(event) && !wasAlreadyFocused;
    if (this.selectAllOnMouseRelease_) {
      this.clientXAtMouseDown_ = event.clientX;
      this.clientYAtMouseDown_ = event.clientY;
    }

    if (event.detail === 2 && isOnlyLeftButton(event)) {
      this.selectAllOnMouseRelease_ = false;
      if (this.didSelectAllOnClickOne_) {
        // If we selected all, double-click word select would be messed up
        // due to existing selection, so clear it again to let normal behavior
        // happen.
        input.setSelectionRange(0, 0);
      } else {
        // If we did not select all, we may have elided, so default behavior
        // could screw up and select the wrong word. Fortunately, in that case
        // selectionStart will be correct, including adjustment, so we select
        // the word it points at.
        this.selectWord(input.selectionStart!);
        event.preventDefault();
      }
    }

    this.inputDelegate_.handlePointer(this, {
      isPointerDown: true,
      startZeroSuggest: false,
    });
  }

  private onInputMouseUp(event: MouseEvent): void {
    this.mouseButtonDown_ &= ~(1 << event.button);
    const willSelectAll =
        this.selectAllOnMouseRelease_ && isOnlyLeftButton(event);

    if (event.detail === 1) {
      this.didSelectAllOnClickOne_ = willSelectAll;
    }

    // Unelide unless we will select all. Unlike views impl, we always do it
    // on release, to make things slightly easier; in particular it handles
    // middle-click paste on Linux, since that happens on mouse-down.
    //
    // This isn't enough for double-click select, since things still
    // move between clicks; but the second clicks mouseDown takes advantage
    // of us fixing up the caret to know what to do.
    if (!willSelectAll) {
      // We don't want to use MOUSE_RELEASE on double-click since that would
      // extend the word-selection of first word to https://word, which is
      // not desirable.
      this.unelideAndUpdateSelection(
          event.detail === 1 ? UnelisionGesture.MOUSE_RELEASE :
                               UnelisionGesture.DOUBLE_CLICK);
    }

    if (willSelectAll) {
      this.selectAllBackwards();
    }

    // Make sure we stop accepting incremental selection updates from browser
    // at this point.
    ++this.omniboxViewState.uiVersion;
    this.sendInputToBrowser();

    const zeroSuggest = isOnlyLeftButton(event) &&
        (this.selectAllOnMouseRelease_ || this.userText.length === 0);
    this.inputDelegate_.handlePointer(this, {
      isPointerDown: false,
      startZeroSuggest: zeroSuggest,
    });

    this.selectAllOnMouseRelease_ = false;
    this.updateAdjustedCopyResult_();
  }

  private onInputMouseMove_(event: MouseEvent): void {
    // If the user has moved the mouse more than a hair (based on the more
    // conservative of blink drag thresholds [1]) they're probably trying to
    // drag-select rather than click.
    // [1] See kDragThresholdX/Y in:
    //    third_party/blink/renderer/core/input/mouse_event_manager.cc
    if (this.selectAllOnMouseRelease_ && this.mouseButtonDown_ !== 0 &&
        ((Math.abs(event.clientX - this.clientXAtMouseDown_) > 3) ||
         (Math.abs(event.clientY - this.clientYAtMouseDown_) > 3))) {
      this.selectAllOnMouseRelease_ = false;
    }
  }

  private isTouchOrPen_(event: PointerEvent): boolean {
    return event.pointerType === 'touch' || event.pointerType === 'pen';
  }

  private onInputPointerDown_(event: PointerEvent): void {
    if (!this.isTouchOrPen_(event)) {
      return;
    }

    this.activeTouchIds_.add(event.pointerId);
    if (this.activeTouchIds_.size > 1) {
      // More than one finger touches the screen (e.g. pinch-to-zoom or
      // two-finger tap). Mark sequence as multi-touch and cancel select-all.
      this.wasMultiTouch_ = true;
      this.selectAllOnTouchRelease_ = false;
      return;
    }

    let wasAlreadyFocused = this.hasFocus();

    if (wasAlreadyFocused && this.lastFocusAcquisition_ !== null &&
        (performance.now() - this.lastFocusAcquisition_ < 100)) {
      wasAlreadyFocused = false;
    }

    this.selectAllOnTouchRelease_ = !wasAlreadyFocused;
    if (this.selectAllOnTouchRelease_) {
      this.clientXAtTouchDown_ = event.clientX;
      this.clientYAtTouchDown_ = event.clientY;
    }

    this.inputDelegate_.handlePointer(this, {
      isPointerDown: true,
      startZeroSuggest: false,
    });
  }

  private onInputPointerUp_(event: PointerEvent): void {
    if (!this.isTouchOrPen_(event)) {
      return;
    }

    this.activeTouchIds_.delete(event.pointerId);
    const isMultiTouch = this.wasMultiTouch_;
    if (this.activeTouchIds_.size === 0) {
      this.wasMultiTouch_ = false;
    }

    if (isMultiTouch || this.activeTouchIds_.size > 0) {
      // Any finger release during or after a multi-touch sequence must not
      // trigger single-tap select-all.
      this.selectAllOnTouchRelease_ = false;
      if (isMultiTouch && this.activeTouchIds_.size === 0) {
        this.inputDelegate_.handlePointer(this, {
          isPointerDown: false,
          startZeroSuggest: false,
        });
      }
      return;
    }

    const willSelectAll = this.selectAllOnTouchRelease_;

    if (willSelectAll) {
      this.selectAllBackwards();
    }

    ++this.omniboxViewState.uiVersion;
    this.sendInputToBrowser();

    const zeroSuggest = willSelectAll || this.userText.length === 0;
    this.inputDelegate_.handlePointer(this, {
      isPointerDown: false,
      startZeroSuggest: zeroSuggest,
    });

    this.selectAllOnTouchRelease_ = false;
  }

  private onInputPointerMove_(event: PointerEvent): void {
    if (!this.isTouchOrPen_(event)) {
      return;
    }

    if (this.selectAllOnTouchRelease_ &&
        ((Math.abs(event.clientX - this.clientXAtTouchDown_) >
          POINTER_DRAG_THRESHOLD_PX) ||
         (Math.abs(event.clientY - this.clientYAtTouchDown_) >
          POINTER_DRAG_THRESHOLD_PX))) {
      this.selectAllOnTouchRelease_ = false;
    }
  }

  // Fallback in case multi-finger gesture detection fails: if the system or
  // browser cancels the pointer interaction (e.g. system gesture takeover like
  // pinch-to-zoom or scroll begin), reset touch selection and active touches.
  private onInputPointerCancel_(event: PointerEvent): void {
    if (!this.isTouchOrPen_(event)) {
      return;
    }
    this.activeTouchIds_.delete(event.pointerId);
    if (this.activeTouchIds_.size === 0) {
      this.wasMultiTouch_ = false;
    }
    this.selectAllOnTouchRelease_ = false;
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

  // Update our `omniboxViewState` to match what got entered into `textInput`.
  // Also bumps the version.
  private updateStateFromTextInput(): void {
    const newValue = this.$.textInput.value;
    const oldValue = this.userText;
    const oldInline = this.omniboxViewState.inlineAutocompletion;
    const oldAll = oldValue + oldInline;

    this.userText = newValue;
    ++this.omniboxViewState.uiVersion;

    if (this.isComposing && oldInline.length > 0 &&
        newValue.length > oldValue.length && oldAll.startsWith(newValue)) {
      this.omniboxViewState.inlineAutocompletion =
          oldAll.substring(newValue.length);
    } else {
      this.omniboxViewState.inlineAutocompletion = '';
      this.omniboxViewState.additionalText = '';
    }

    this.omniboxViewState.selection = this.getMojoSelection();
    // Sync up the read-only view to have the right text.
    this.updateTextPiecesFromUserText();
  }

  private onInputInput(): void {
    this.omniboxViewState.userInputInProgress = true;
    this.updateStateFromTextInput();
    this.sendInputToBrowser();
  }

  private onInputKeyDown(event: KeyboardEvent): void {
    if (this.mouseButtonDown_ !== 0 && this.selectAllOnMouseRelease_) {
      // https://crbug.com/40123188 If the user presses the mouse button down
      // and begins to type without releasing the mouse button, the subsequent
      // release will delete any newly typed characters due to the SelectAll
      // happening on mouse-up. If we detect this state, do the select-all
      // immediately.
      this.selectAllBackwards();
      this.selectAllOnMouseRelease_ = false;
    }


    const inlineAutocompletion = this.omniboxViewState.inlineAutocompletion;
    if (inlineAutocompletion.length > 0 && !this.isComposing) {
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
        this.omniboxViewState.userInputInProgress = true;
        this.omniboxViewState.selection = this.getMojoSelection();
        ++this.omniboxViewState.uiVersion;
        this.updateTextPiecesFromUserText();

        this.sendInputToBrowser();
        event.preventDefault();
        return;
      }
    }

    if (event.key === 'Home') {
      if (this.unelideAndUpdateSelection(UnelisionGesture.HOME_KEY_PRESSED)) {
        if (event.shiftKey) {
          // Shift-home should select from old selection's start to 0.
          // Note that start here depends on the direction.
          this.setSelection(
              0,
              this.$.textInput.selectionDirection! === 'backward' ?
                  this.$.textInput.selectionEnd! :
                  this.$.textInput.selectionStart!,
              'backward');
        } else {
          // Otherwise just set caret.
          this.setSelection(0, 0);
        }
        this.sendInputToBrowser();
        event.preventDefault();
      }
    }

    this.inputDelegate_.handleKey(this, /*isKeyUp=*/ false, event);
  }

  private onInputKeyUp(event: KeyboardEvent): void {
    this.inputDelegate_.handleKey(this, /*isKeyUp=*/ true, event);
    this.checkForSelectionChange_();
  }

  private onContextMenu_(event: PointerEvent): void {
    // We want the menu handled on the C++ side, so we let default handling
    // happen, and prevent the toolbar's own handling.
    event.stopPropagation();
  }

  private checkForSelectionChange_(): void {
    // If the selection isn't what we think it should be, that suggests the user
    // has changed it, so unelide, but not if the mouse is down. We poll this
    // explicitly to avoid fighting with handling of mouse events, etc.
    if (this.mouseButtonDown_ !== 0) {
      return;
    }

    const currentSelection = this.getMojoSelection();
    if (currentSelection.start !== this.omniboxViewState.selection?.start ||
        currentSelection.end !== this.omniboxViewState.selection?.end) {
      this.unelideAndUpdateSelection(UnelisionGesture.OTHER);
    }
  }

  // Returns the selection with gfx::Range-compatible semantics, suitable for
  // sending over mojo.
  getMojoSelection(): MojomRange {
    // If we're displaying an inline autocompletion, conceptually the selection
    // is a caret at the input end.
    if (this.omniboxViewState.inlineAutocompletion.length !== 0) {
      return {start: this.userText.length, end: this.userText.length};
    }
    return this.getSelection();
  }

  protected getDragFaviconUrl_(): string {
    if (this.adjustedCopyResult?.adjustedUrl) {
      return getFaviconUrl(this.adjustedCopyResult.adjustedUrl, {
        size: 16,
        scaleFactor: `${window.devicePixelRatio}x`,
      });
    }
    return '';
  }

  protected getDragTitle_(): string {
    return this.adjustedCopyResult?.pageTitle || '';
  }

  private populateDataTransfer_(dataTransfer: DataTransfer): boolean {
    const input = this.$.textInput;
    const selectionStart = input.selectionStart!;
    const selectionEnd = input.selectionEnd!;

    if (selectionStart !== selectionEnd && this.adjustedCopyResult) {
      dataTransfer.setData('text/plain', this.adjustedCopyResult.adjustedText);

      if (this.adjustedCopyResult.adjustedUrl) {
        dataTransfer.setData(
            'text/uri-list', this.adjustedCopyResult.adjustedUrl);
      }
      return true;
    }
    return false;
  }

  private onDragStart_(e: DragEvent): void {
    this.isDraggingFromSelf_ = true;

    if (e.dataTransfer && this.populateDataTransfer_(e.dataTransfer)) {
      e.dataTransfer.effectAllowed = 'copy';

      if (this.adjustedCopyResult?.adjustedUrl) {
        const template = this.$.dragTemplate;
        const xOffset = template.clientWidth / 2;
        const yOffset = template.clientHeight / 2;

        e.dataTransfer.setDragImage(template, xOffset, yOffset);
      }
    }
  }

  private onInputCopy_(e: ClipboardEvent): void {
    if (e.clipboardData && this.populateDataTransfer_(e.clipboardData)) {
      e.preventDefault();
    }
  }

  private onInputCut_(e: ClipboardEvent): void {
    if (e.clipboardData && this.populateDataTransfer_(e.clipboardData)) {
      e.preventDefault();
      // Go via execCommand to keep Ctrl-Z happy.
      document.execCommand('delete');
      this.onInputInput();
    }
  }

  private onInputPaste_(e: ClipboardEvent): void {
    if (!e.clipboardData) {
      return;
    }

    // Extract text/plain or fall back to text/uri-list (for link/file drops
    // when text/plain is missing). Other text formats like text/html already
    // provide a text/plain representation. Non-text formats and unhandled
    // clipboard MIME types are explicitly blocked by preventDefault().
    let rawText = e.clipboardData.getData('text/plain');
    if (!rawText) {
      rawText = e.clipboardData.getData('text/uri-list');
    }

    if (rawText) {
      e.preventDefault();
      const sanitizedText = sanitizeTextForPaste(rawText);
      document.execCommand('insertText', false, sanitizedText);
    }
  }

  private onInputCompositionstart_(): void {
    this.isComposing = true;
  }

  private onInputCompositionend_(): void {
    this.isComposing = false;
  }

  private updateAdjustedCopyResult_(): void {
    const input = this.$.textInput;
    const start = input.selectionStart!;
    const end = input.selectionEnd!;
    if (start !== end) {
      const selectedText = input.value.substring(start, end);
      this.browserProxy_.toolbarUIHandler
          .adjustOmniboxTextForCopy(selectedText, start)
          .then(response => {
            this.adjustedCopyResult = response || null;
          })
          .catch(() => {
            this.adjustedCopyResult = null;
          });
    } else {
      this.adjustedCopyResult = null;
    }
  }

  private onSelectionChange_(): void {
    if (this.mouseButtonDown_ !== 0) {
      return;
    }
    this.updateAdjustedCopyResult_();
  }

  private onDragEnd_(): void {
    this.isDraggingFromSelf_ = false;
  }

  private onDragEnter_(e: DragEvent): void {
    if (this.isDraggingFromSelf_) {
      return;
    }
    const types = e.dataTransfer?.types;
    if (types &&
        (types.includes('text/uri-list') || types.includes('text/plain') ||
         types.includes('Files'))) {
      e.preventDefault();
      this.classList.add('dragging-over');
    }
  }

  private onDragLeave_(): void {
    this.classList.remove('dragging-over');
  }

  private onDragOver_(e: DragEvent): void {
    if (this.isDraggingFromSelf_) {
      return;
    }
    const types = e.dataTransfer?.types;
    if (types &&
        (types.includes('text/uri-list') || types.includes('text/plain') ||
         types.includes('Files'))) {
      e.preventDefault();
      e.stopPropagation();
      e.dataTransfer.dropEffect = 'copy';
    }
  }

  private onDrop_(e: DragEvent): void {
    if (this.isDraggingFromSelf_) {
      return;
    }

    this.classList.remove('dragging-over');
    e.preventDefault();
    e.stopPropagation();

    if (!e.dataTransfer) {
      return;
    }

    const types = e.dataTransfer.types;

    if (types.includes('text/uri-list')) {
      let url = e.dataTransfer.getData('text/uri-list');
      // For restricted URLs (e.g. javascript: or chrome:// links), Blink's IPC
      // sanitization intercepts the drop and overwrites the URL with
      // about:blank#blocked. We can recover the original string from text/plain
      // and forward it to C++ where it will be properly sanitized
      // (e.g. via StripJavascriptSchemas) before being set in the omnibox.
      if (url.startsWith('about:blank#blocked') &&
          types.includes('text/plain')) {
        const plainText = e.dataTransfer.getData('text/plain');
        if (plainText) {
          url = plainText;
        }
      }

      if (url) {
        this.inputDelegate_.handleDropText(this, {
          text: url.split('\n')[0]!,
        });
      }
    } else if (types.includes('Files')) {
      this.inputDelegate_.handleDropFile(this, {
        dropPosition: {x: e.clientX, y: e.clientY},
      });
    } else if (types.includes('text/plain')) {
      const text = e.dataTransfer.getData('text/plain');
      if (text) {
        this.inputDelegate_.handleDropText(this, {
          text,
        });
      }
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

  private isAllSelected(): boolean {
    const input = this.$.textInput;
    return input.selectionStart === 0 &&
        input.selectionEnd === input.value.length;
  }

  isCaretAtStart(): boolean {
    const inputProper = this.$.textInput;
    return inputProper.selectionStart === 0 && inputProper.selectionEnd === 0;
  }

  private selectAllBackwards(): void {
    this.setSelection(0, this.userText.length, 'backward');
  }

  private selectAllForward(): void {
    this.setSelection(0, this.userText.length);
  }

  private onWrapFocus(): void {
    // We forward focus requests from the entirety of textContainerWrap to
    // textInput.
    this.$.textInput.focus();
  }

  // If the current input is an elided version of a full URL (e.g. with
  // 'https://' omitted), and circumstances seem to indicate that it would be
  // more helpful to share the full URL, replaces the shortened URL with the
  // full version, and incrementally adjusts the selection to be logically
  // consistent with what happened before.
  // Does not notify the browser.
  //
  // Returns whether unelision happened.
  //
  // Compare OmniboxViewViews::UnapplySteadyStateElisions(UnelisionGesture),
  // which this is heavily based on and incorporates the comments close to
  // verbatim.
  private unelideAndUpdateSelection(gesture: UnelisionGesture): boolean {
    // If everything is selected, the user likely does not intend to edit the
    // URL. An exception is if the Home key is pressed, which is a pretty
    // strong signal that the user wants to interact with the text at the
    // beginning of the URL (where the hidden scheme would be).
    if (this.isAllSelected() && gesture !== UnelisionGesture.HOME_KEY_PRESSED) {
      return false;
    }

    const input = this.$.textInput;
    const originalText = this.userText;
    // Save selection before unelide() since it changes it.
    let selectionStart: number = input.selectionStart!;
    let selectionEnd: number = input.selectionEnd!;
    const selectionDirection = input.selectionDirection!;
    if (!this.unelide()) {
      return false;
    }

    // Find the length of the prefix that was chopped off to form the elided
    // URL. This simple logic only works because we elide only prefixes from the
    // full URL.
    let offset = this.userText.lastIndexOf(originalText);

    // Some intranet URLs have an elided form that's not a substring of the full
    // URL string. e.g. "https://foobar" has the elided form "foobar/". This is
    // to prevent elided URLs from looking like search terms. See
    // AutocompleteInput::FormattedStringWithEquivalentMeaning for details.
    //
    // In this special case, chop off the trailing slash and search again.
    if (offset === -1 && originalText.endsWith('/')) {
      offset = this.userText.lastIndexOf(
          originalText.substring(0, originalText.length - 1));
    }

    // We expect offset to be valid now, but if it's somehow not, it's probably
    // best to just not do anything with the selection.
    if (offset !== -1) {
      if (gesture === UnelisionGesture.MOUSE_RELEASE) {
        // For user selections that look like a URL instead of a Search:
        // If we are uneliding at the end of a drag-select (on mouse release),
        // and the selection spans to the beginning of the elided URL, ensure
        // that the new selection spans to the beginning of the unelided URL
        // too.
        // i.e. google.com/maps => https://www.google.com/maps
        //      ^^^^^^^^^^         ^^^^^^^^^^^^^^^^^^^^^^
        if (selectionStart !== 0) {
          selectionStart = selectionStart + offset;
        }
        if (selectionEnd !== 0) {
          selectionEnd = selectionEnd + offset;
        }
      } else {
        selectionStart = selectionStart + offset;
        selectionEnd = selectionEnd + offset;
      }

      this.setSelection(selectionStart, selectionEnd, selectionDirection);
    }

    return true;
  }

  // Replaces the current input with the full version of the URL. Does not
  // notify the browser. Returns whether it did anything.
  //
  // Compare OmniboxEditModel::Unelide() (note that this version doesn't
  // change the selection, to avoid messing things up for middle-click paste).
  private unelide(): boolean {
    // User changed stuff, so should not unelide.
    if (this.omniboxViewState.userInputInProgress) {
      return false;
    }

    // Already showing the right thing.
    if (this.userText === this.omniboxViewState.formattedFullUrl) {
      return false;
    }

    this.$.textInput.value = this.omniboxViewState.formattedFullUrl;
    this.updateStateFromTextInput();
    return true;
  }

  // Selects the word in input that includes `offset`.
  // TODO(crbug.com/503784990): this may not have the right left/right affinity
  // at edge.
  private selectWord(offset: number): void {
    const segmenter = new Intl.Segmenter(undefined, {granularity: 'word'});
    const segments = segmenter.segment(this.userText);
    for (const segment of segments) {
      const segmentEnd = (segment.index + segment.segment.length);
      if ((segment.index <= offset && offset < segmentEnd) ||
          (offset === this.userText.length && offset === segmentEnd)) {
        this.setSelection(segment.index, segmentEnd);
        return;
      }
    }
  }

  private sendInputToBrowser(): void {
    this.inputDelegate_.handleTextInput(this, {
      uiVersion: this.omniboxViewState.uiVersion,
      browserVersion: this.omniboxViewState.browserVersion,
      text: this.userText,
      inlineAutocompletion: this.omniboxViewState.inlineAutocompletion,
      selection: this.getMojoSelection(),
    });
  }

  // Updates all the copies of selection state. The input has semantics
  // for HTMLInputElement selection --- `start` <= `end`, with direction
  // specified separately.
  private setSelection(
      start: number, end: number, dir: SelectionDirection = 'forward') {
    this.$.textInput.setSelectionRange(start, end, dir);
    this.omniboxViewState.selection = this.getMojoSelection();
  }

  // Selects the proper view (<input> or our syntax highlighting one
  // active.
  private switchView_(hasFocus: boolean): void {
    const useInputElement = hasFocus ||
        this.omniboxViewState.placeholder && this.userText.length === 0;
    if (useInputElement) {
      this.$.textInput.style.opacity = '1';
      this.$.textContainer.style.opacity = '0';
    } else {
      this.$.textInput.style.opacity = '0';
      this.$.textContainer.style.opacity = '1';
    }
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

  protected getInputPlaceholder_(): string|undefined {
    return this.omniboxViewState.placeholder?.text;
  }

  protected getInputClasses_(): string|undefined {
    const placeholder = this.omniboxViewState.placeholder;
    if (placeholder) {
      return ReadonlyOmniboxElement.getTextPieceClasses(placeholder);
    } else {
      return undefined;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'readonly-omnibox': ReadonlyOmniboxElement;
  }
}

customElements.define(ReadonlyOmniboxElement.is, ReadonlyOmniboxElement);
