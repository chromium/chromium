// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/searchbox/searchbox_icon.js';
import '//resources/cr_components/searchbox/searchbox_thumbnail.js';

import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from '//resources/cr_components/searchbox/searchbox_dropdown.js';
import type {SearchboxInputElement} from '//resources/cr_components/searchbox/searchbox_input.js';
import {SearchboxMixin} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import type {SearchboxMixinInterface} from '//resources/cr_components/searchbox/searchbox_mixin.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from '//resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {AutocompleteMatch, PageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './lens_searchbox.css.js';
import {getHtml} from './lens_searchbox.html.js';

// LINT.IfChange(GhostLoaderTagName)
const LENS_GHOST_LOADER_TAG_NAME = 'cr-searchbox-ghost-loader';
// LINT.ThenChange(/chrome/browser/resources/lens/shared/searchbox_ghost_loader.ts:GhostLoaderTagName)

export interface LensSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
  };
}

const LensSearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class LensSearchboxElement extends LensSearchboxElementBase implements
    SearchboxMixinInterface {
  static get is() {
    return 'cr-lens-searchbox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      //========================================================================
      // Public properties
      //========================================================================

      placeholderText: {
        type: String,
        reflect: true,
        notify: true,
      },

      //========================================================================
      // Private properties
      //========================================================================

      /** Searchbox default icon (i.e., Google G icon or the search loupe). */
      searchboxIcon_: {type: String},

      thumbnailUrl_: {type: String},
      isThumbnailDeletable_: {type: Boolean},
      searchboxLensSearchEnabled_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  accessor placeholderText: string = '';
  protected accessor searchboxIcon_: string =
      loadTimeData.getString('searchboxDefaultIcon');
  protected accessor searchboxLensSearchEnabled_: boolean =
      loadTimeData.getBoolean('searchboxLensSearch');
  protected accessor thumbnailUrl_: string = '';
  protected accessor isThumbnailDeletable_: boolean = false;

  private proxy_: SearchboxBrowserProxy = SearchboxBrowserProxy.getInstance();

  private autocompleteResultChangedListenerId_: number|null = null;
  private thumbnailChangedListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.autocompleteResultChangedListenerId_ =
        this.proxy_.callbackRouter.autocompleteResultChanged.addListener(
            this.onAutocompleteResultChanged.bind(this));
    this.thumbnailChangedListenerId_ =
        this.proxy_.callbackRouter.setThumbnail.addListener(
            this.onSetThumbnail_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();

    assert(this.autocompleteResultChangedListenerId_);
    this.proxy_.callbackRouter.removeListener(
        this.autocompleteResultChangedListenerId_);
    this.autocompleteResultChangedListenerId_ = null;
    assert(this.thumbnailChangedListenerId_);
    this.proxy_.callbackRouter.removeListener(this.thumbnailChangedListenerId_);
    this.thumbnailChangedListenerId_ = null;
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('thumbnailUrl_')) {
      this.showThumbnail = !!this.thumbnailUrl_;
    }
    if (changedPrivateProperties.has('result') ||
        changedPrivateProperties.has('selectedMatchIndex')) {
      this.selectedMatch = this.computeSelectedMatch_();
    }
  }

  override onInputWrapperFocusout(e: FocusEvent) {
    const newlyFocusedEl = e.relatedTarget as Element;

    // If this is a Lens searchbox, treat the ghost loader as keeping searchbox
    // focus.
    // TODO(crbug.com/380467089): This workaround wouldn't be needed
    // if the ghost loader was part of the searchbox element. Remove
    // this workaround once they are combined.
    if (newlyFocusedEl?.tagName.toLowerCase() === LENS_GHOST_LOADER_TAG_NAME) {
      return;
    }

    super.onInputWrapperFocusout(e);
  }

  onFocusin(e: FocusEvent) {
    // Check event path to ensure input is not focused from this function if
    // thumbnail is told to focus. This is better than stopping propagation in
    // case other higher components depend on focus in events.
    const path = e.composedPath();
    if (path.some(
            el => (el as HTMLElement).tagName === 'CR-SEARCHBOX-THUMBNAIL')) {
      return;
    }
    this.proxy_.handler.onFocusChanged(true);
    this.focusInput();
  }

  blurInput() {
    this.$.input.blur();
  }

  focusInput() {
    this.$.input.focus();
  }

  hasThumbnail(): boolean {
    return !!this.thumbnailUrl_;
  }

  //============================================================================
  // SearchboxMixin overrides
  //============================================================================

  override getInputElement(): SearchboxInputElement {
    return this.$.input;
  }

  override getDropdownElement(): SearchboxDropdownElement {
    const matches =
        this.shadowRoot.querySelector<SearchboxDropdownElement>('#matches');
    assert(matches);
    return matches;
  }

  override pageHandler(): PageHandlerInterface {
    return this.proxy_.handler;
  }

  override getWrapperElement(): HTMLElement {
    return this.$.inputWrapper;
  }

  override onSearchboxInputTextUpdated(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    // Query autocomplete for Lens searchbox even on empty input
    this.queryAutocomplete(e.detail.value, e.detail.isComposing);
  }

  override handleKeyNavigation(e: KeyboardEvent) {
    if (this.showThumbnail) {
      const thumbnail =
          this.shadowRoot.querySelector<HTMLElement>('cr-searchbox-thumbnail');
      if (thumbnail === this.shadowRoot.activeElement) {
        if (e.key === 'Backspace' || e.key === 'Enter') {
          // Remove thumbnail, focus input, and notify browser.
          this.thumbnailUrl_ = '';
          this.focusInput();
          this.clearAutocompleteMatches();
          this.proxy_.handler.onThumbnailRemoved();
          const inputValue = this.$.input.getInputValue();
          // Clearing the autocomplete matches above doesn't allow for
          // navigation directly after removing the thumbnail. Must manually
          // query autocomplete after removing the thumbnail since the
          // thumbnail isn't part of the text input.
          this.queryAutocomplete(inputValue);
          e.preventDefault();
        } else if (e.key === 'Tab' && !e.shiftKey) {
          this.focusInput();
          e.preventDefault();
        } else if (
            this.dropdownIsVisible &&
            (e.key === 'ArrowUp' || e.key === 'ArrowDown')) {
          // If the dropdown is visible, arrowing up and down unfocuses the
          // thumbnail and follows standard arrow up/down behavior (selects
          // the next/previous match).
          this.focusInput();
        }
      } else if (
          this.isThumbnailDeletable_ &&
          this.$.input.inputElement.selectionStart === 0 &&
          this.$.input.inputElement.selectionEnd === 0 &&
          this.$.input === this.shadowRoot.activeElement &&
          (e.key === 'Backspace' || (e.key === 'Tab' && e.shiftKey))) {
        // Backspacing or shift-tabbing the thumbnail results in the thumbnail
        // being focused.
        thumbnail?.focus();
        e.preventDefault();
      }
    }
    super.handleKeyNavigation(e);
  }

  queryInputAutocomplete() {
    this.queryAutocomplete(this.$.input.inputElement.value, false);
  }

  isInputEmpty(): boolean {
    // If this is called before first render, the input element will not exist.
    if (!this.shadowRoot?.querySelector('#input') || !this.$.input ||
        !this.$.input.lastInput()) {
      return true;
    }
    return !this.$.input.lastInput()!.text.trim();
  }

  private computeSelectedMatch_(): AutocompleteMatch|null {
    if (!this.result || !this.result.matches) {
      return null;
    }
    return this.result.matches[this.selectedMatchIndex] || null;
  }

  //============================================================================
  // Callbacks
  //============================================================================

  private onSetThumbnail_(thumbnailUrl: string, isDeletable: boolean) {
    this.thumbnailUrl_ = thumbnailUrl;
    this.isThumbnailDeletable_ = isDeletable;
  }

  //============================================================================
  // Event handlers
  //============================================================================

  protected onRemoveThumbnailClick_() {
    /* Remove thumbnail, focus input, and notify browser. */
    this.thumbnailUrl_ = '';
    this.focusInput();
    this.clearAutocompleteMatches();
    this.proxy_.handler.onThumbnailRemoved();
    // Clearing the autocomplete matches above doesn't allow for
    // navigation directly after removing the thumbnail. Must manually
    // query autocomplete after removing the thumbnail since the
    // thumbnail isn't part of the text input.
    const inputValue = this.$.input.getInputValue();
    this.queryAutocomplete(inputValue);
  }

  protected onLensSearchClick_() {
    this.dropdownIsVisible = false;
    this.fire('open-lens-search');
  }

  //============================================================================
  // Helpers
  //============================================================================

  protected computePlaceholderText_(placeholderText: string): string {
    return placeholderText ||
        this.i18n(
            this.showThumbnail ? 'searchBoxHintMultimodal' : 'searchBoxHint');
  }

  protected getThumbnailTabindex_(): string {
    // If the thumbnail can't be deleted, returning an empty string will set the
    // tabindex to nothing, which will make the thumbnail not focusable.
    return this.isThumbnailDeletable_ ? '1' : '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-lens-searchbox': LensSearchboxElement;
  }
}

customElements.define(LensSearchboxElement.is, LensSearchboxElement);
