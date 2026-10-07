// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './webui_browser_searchbox.css.js';
import {getHtml} from './webui_browser_searchbox.html.js';

export interface WebuiBrowserSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

import {SearchboxSelectionMixin} from '//resources/cr_components/searchbox/searchbox_selection_mixin.js';

/**
 * An element that supports the (experimental) Unbounded Element API, which
 * renders the element into a separate OS window so that it can escape the
 * bounds of the browser window.
 */
interface UnboundedElement extends HTMLElement {
  showUnboundedElement(): Promise<void>;
  hideUnboundedElement(): Promise<void>;
}

/**
 * @return Whether the Unbounded Element API is available in this context.
 */
function isUnboundedSupported(): boolean {
  return 'showUnboundedElement' in HTMLElement.prototype;
}

const WebuiBrowserSearchboxElementBase = SearchboxMixin(
    SearchboxSelectionMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement))));

export class WebuiBrowserSearchboxElement extends
    WebuiBrowserSearchboxElementBase implements SearchboxMixinInterface {
  override get isAimButtonVisible(): boolean {
    return false;
  }

  override get showContextEntrypoint(): boolean {
    return false;
  }

  override get virtualFocusEnabled(): boolean {
    return loadTimeData.valueExists('webuiBrowserVirtualFocusNavigation') &&
        loadTimeData.getBoolean('webuiBrowserVirtualFocusNavigation');
  }

  static get is() {
    return 'webui-browser-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },
      searchboxChromeRefreshTheming: {
        type: Boolean,
        reflect: true,
      },
      searchboxSteadyStateShadow: {
        type: Boolean,
        reflect: true,
      },
      searchboxIcon_: {type: String},
      searchboxVoiceSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
      useWebkitSearchIcons_: {
        type: Boolean,
        reflect: true,
      },
      unboundedSupported_: {type: Boolean},
    };
  }

  accessor placeholderText: string = '';
  accessor searchboxChromeRefreshTheming: boolean =
      loadTimeData.getBoolean('searchboxCr23Theming');
  accessor searchboxSteadyStateShadow: boolean =
      loadTimeData.getBoolean('searchboxCr23SteadyStateShadow');
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxVoiceSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxVoiceSearch');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor useWebkitSearchIcons_: boolean = false;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;

  // Whether the Unbounded Element API is available. If so, `#inputWrapper`
  // (the input and the dropdown) is shown in a separate OS window while the
  // dropdown is visible, so that it can extend beyond the browser window.
  protected accessor unboundedSupported_: boolean = isUnboundedSupported();

  // Set when the platform requested an interactive dismissal of the unbounded
  // window and it was canceled (see `onInputWrapperBeforetoggle_()`). The
  // platform requests one for either:
  // - A pointer press (click or tap) outside of the unbounded window. Nothing
  //   needs to be done here: the press is still delivered to the page.
  // - An Escape key press. Some platforms consume the Escape keydown in that
  //   case (e.g. Aura), so the searchbox only sees the keyup, and the keydown
  //   needs to be replayed on keyup.
  private pendingInteractiveDismissal_: boolean = false;
  // Whether the searchbox received the (trusted) keydown of the current Escape
  // key press. Whether the platform consumes it is platform-specific: e.g. on
  // Aura the unbounded window consumes it, whereas on Mac the unbounded window
  // never becomes the key window, so the keydown is delivered to the page as
  // usual and the window is not dismissed. The keydown is only replayed if it
  // was not received, so that Escape is never handled twice.
  private escapeKeydownReceived_: boolean = false;
  private boundOnDocumentPointerdown_ = () => {
    this.pendingInteractiveDismissal_ = false;
  };

  constructor() {
    super();
    const browserProxy = SearchboxBrowserProxy.getInstance();
    this.pageHandler_ = browserProxy.handler;
    this.callbackRouter_ = browserProxy.callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.callbackRouter_.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
    // A pointer press (click or tap) outside of the unbounded window requests
    // an interactive dismissal too. Clear the flag set for it once the press
    // reaches the page, so that it can't cause a later Escape keyup to be
    // replayed as a keydown.
    document.addEventListener(
        'pointerdown', this.boundOnDocumentPointerdown_, {capture: true});
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }
    document.removeEventListener(
        'pointerdown', this.boundOnDocumentPointerdown_, {capture: true});
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('searchboxChromeRefreshTheming')) {
      this.useWebkitSearchIcons_ = this.searchboxChromeRefreshTheming;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.initialInputScrollHeight = this.$.input.scrollHeight;
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('dropdownIsVisible')) {
      // Runs after the dropdown has been (un)hidden in the DOM but before the
      // next frame is produced, so the unbounded window (if any) is sized to
      // include the dropdown.
      this.updateUnboundedWindow_();
    }
  }

  //========================================================================
  // Unbounded Element
  //========================================================================

  private getUnboundedWrapper_(): UnboundedElement {
    return this.$.inputWrapper as UnboundedElement;
  }

  private isUnboundedWindowShown_(): boolean {
    return this.unboundedSupported_ &&
        this.$.inputWrapper.matches(':unbounded');
  }

  /**
   * Shows `#inputWrapper` in a separate OS window only while the dropdown is
   * visible. When the dropdown is hidden, no separate window exists and
   * `#inputWrapper` is rendered as part of the browser window. Since
   * `#inputWrapper` is always `visibility: visible`, switching between the two
   * never produces a frame in which it is hidden.
   */
  private updateUnboundedWindow_() {
    if (!this.unboundedSupported_) {
      return;
    }
    const isShown = this.isUnboundedWindowShown_();
    if (this.dropdownIsVisible && !isShown) {
      this.getUnboundedWrapper_().showUnboundedElement().catch(
          err => this.handleUnboundedError_('showUnboundedElement', err));
    } else if (!this.dropdownIsVisible && isShown) {
      this.getUnboundedWrapper_().hideUnboundedElement().catch(
          err => this.handleUnboundedError_('hideUnboundedElement', err));
    }
  }

  private handleUnboundedError_(operation: string, err: unknown) {
    if (err instanceof DOMException && err.name === 'AbortError') {
      // Expected if the window was hidden before it finished showing (e.g.
      // the dropdown was opened and closed quickly).
      return;
    }
    console.error(`${operation} failed:`, err);
  }

  protected onInputWrapperBeforetoggle_(e: Event) {
    const toggleEvent = e as Event & {newState?: string};
    if (toggleEvent.newState !== 'closed') {
      return;
    }
    if (!e.cancelable) {
      // The unbounded window is closing and this can't be prevented. This is
      // the case for `hideUnboundedElement()`, which is only called once the
      // dropdown is already hidden. Otherwise, the platform is closing the
      // window regardless of the searchbox (e.g. depending on the platform,
      // when the user switches to another OS window). In that case,
      // `#inputWrapper` goes back to being rendered as part of the browser
      // window, where the dropdown would be clipped by the bounds of the
      // browser window, so hide the dropdown as if it were dismissed. Since
      // `#inputWrapper` is always `visibility: visible`, this doesn't flicker.
      // If the focus also leaves the searchbox, the regular focusout path
      // stops autocomplete as usual.
      this.dropdownIsVisible = false;
      return;
    }
    // Cancelable 'beforetoggle' events are fired for interactive dismissals
    // requested by the platform: an Escape key press or a pointer press (click
    // or tap) outside of the unbounded window. The visibility of the dropdown
    // (and therefore of the unbounded window) is owned by the searchbox, so
    // cancel the dismissal:
    // - For an outside press, the press is still delivered and closes the
    //   dropdown through the regular focusout path if focus leaves the
    //   searchbox.
    // - For Escape, if the keydown was consumed by the platform, it is replayed
    //   on the following keyup so that the searchbox can apply the omnibox
    //   Escape behavior (e.g. revert the selection to the default match first).
    e.preventDefault();
    this.pendingInteractiveDismissal_ = true;
  }

  protected onInputWrapperToggle_(e: Event) {
    const toggleEvent = e as Event & {newState?: string};
    if (toggleEvent.newState === 'closed') {
      this.pendingInteractiveDismissal_ = false;
      return;
    }
    // The dropdown may have been hidden while the window was being shown.
    if (!this.dropdownIsVisible) {
      this.updateUnboundedWindow_();
    }
  }

  protected onInputWrapperKeydown_(e: KeyboardEvent) {
    if (e.key === 'Escape' && e.isTrusted) {
      this.escapeKeydownReceived_ = true;
    }
    return this.onInputWrapperKeydown(e);
  }

  protected onInputWrapperKeyup_(e: KeyboardEvent) {
    if (e.key !== 'Escape') {
      return;
    }
    const keydownReceived = this.escapeKeydownReceived_;
    this.escapeKeydownReceived_ = false;
    if (!this.pendingInteractiveDismissal_) {
      return;
    }
    this.pendingInteractiveDismissal_ = false;
    if (keydownReceived || !this.isUnboundedWindowShown_()) {
      return;
    }
    // Replay the Escape keydown consumed by the unbounded window.
    this.$.input.inputElement.dispatchEvent(new KeyboardEvent('keydown', {
      key: 'Escape',
      code: 'Escape',
      bubbles: true,
      cancelable: true,
      composed: true,
      shiftKey: e.shiftKey,
      ctrlKey: e.ctrlKey,
      altKey: e.altKey,
      metaKey: e.metaKey,
    }));
  }

  protected onInputWrapperFocusout_(e: FocusEvent) {
    this.pendingInteractiveDismissal_ = false;
    this.escapeKeydownReceived_ = false;
    this.onInputWrapperFocusout(e);
  }

  focusInput() {
    this.$.input.focus();
  }

  setInputText(text: string) {
    this.$.input.setInputText(text);
  }

  selectAll() {
    this.$.input.select();
  }

  //========================================================================
  // SearchboxMixin abstract method implementations
  //========================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    return this.$.matches;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override pageHandler(): PageHandlerInterface {
    return this.pageHandler_;
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  protected shouldShowVoiceLens_(isEnabled: boolean): boolean {
    if (!isEnabled) {
      return false;
    }

    if (!this.isInputEmpty()) {
      return false;
    }

    return true;
  }

  //========================================================================
  // Event handlers
  //========================================================================

  protected onInputFocusin_() {
    this.pageHandler_.onFocusChanged(true);
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e);
  }

  protected onVoiceSearchClick_() {
    this.dispatchEvent(new Event('open-voice-search'));
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.dispatchEvent(new Event('open-lens-search'));
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'webui-browser-searchbox': WebuiBrowserSearchboxElement;
  }
}

customElements.define(
    WebuiBrowserSearchboxElement.is, WebuiBrowserSearchboxElement);
