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
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerInterface as SearchboxPageHandlerInterface} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {browserProxyFactory} from './omnibox_popup.mojom-webui.js';
import type {PageCallbackRouter as PopupPageCallbackRouter} from './omnibox_popup.mojom-webui.js';
import {getCss} from './omnibox_popup_searchbox.css.js';
import {getHtml} from './omnibox_popup_searchbox.html.js';

export interface OmniboxPopupSearchboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

// TODO(crbug.com/497883783): Move I18nMixinLit to SearchboxMixin.
const OmniboxPopupSearchboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class OmniboxPopupSearchboxElement extends
    OmniboxPopupSearchboxElementBase implements SearchboxMixinInterface {
  static get is() {
    return 'omnibox-popup-searchbox';
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
      // TODO(b/517218130): Ensure Omnibox is laid out correctly when
      //   `isTouchUi_` is true.
      isTouchUi_: {
        type: Boolean,
        reflect: true,
      },
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
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
  // TODO(b/519185419): Remove `isTouchUi_` property and from `loadTimeData` and
  // get layout constants and font sizes from a C++ layout helper instead.
  protected accessor isTouchUi_: boolean = loadTimeData.getBoolean('isTouchUi');
  protected accessor omniboxPopupDebugEnabled_: boolean =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');

  private searchboxPageHandler_: SearchboxPageHandlerInterface;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private popupCallbackRouter_: PopupPageCallbackRouter;
  private listenerIds_: number[] = [];
  private popupListenerIds_: number[] = [];

  constructor() {
    super();
    const searchboxBrowserProxy = SearchboxBrowserProxy.getInstance();
    this.searchboxPageHandler_ = searchboxBrowserProxy.handler;
    this.searchboxCallbackRouter_ = searchboxBrowserProxy.callbackRouter;
    const popupBrowserProxy = browserProxyFactory.getInstance();
    this.popupCallbackRouter_ = popupBrowserProxy.callbackRouter;
  }

  override connectedCallback() {
    super.connectedCallback();
    // TODO(crbug.com/497883783): Move autocompleteResultChangedListenerId_
    // property to SearchboxMixin.
    this.listenerIds_ = [
      this.searchboxCallbackRouter_.autocompleteResultChanged.addListener(
          this.onAutocompleteResultChanged.bind(this)),
    ];
    this.popupListenerIds_ = [
      this.popupCallbackRouter_.setInputText.addListener(
          (input: string) => this.$.input.setInputText(input)),
    ];
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.listenerIds_.forEach(
        id => this.searchboxCallbackRouter_.removeListener(id));
    this.listenerIds_ = [];
    this.popupListenerIds_.forEach(
        id => this.popupCallbackRouter_.removeListener(id));
    this.popupListenerIds_ = [];
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

  focusInput() {
    this.$.input.focus();
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

  override pageHandler(): SearchboxPageHandlerInterface {
    return this.searchboxPageHandler_;
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
    this.searchboxPageHandler_.onFocusChanged(true);
  }

  protected computePlaceholderText_(): string {
    if (this.placeholderText) {
      return this.placeholderText;
    }
    return this.i18n('searchBoxHint');
  }

  protected onSearchboxInputTextUpdated_(
      e: CustomEvent<{value: string, isComposing: boolean}>) {
    this.onSearchboxInputTextUpdated(e, /*forceAutocomplete=*/ false);
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
    'omnibox-popup-searchbox': OmniboxPopupSearchboxElement;
  }
}

customElements.define(
    OmniboxPopupSearchboxElement.is, OmniboxPopupSearchboxElement);
