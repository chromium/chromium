// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/searchbox/searchbox_dropdown.js';
import '//resources/cr_components/searchbox/searchbox_input.js';
import '//resources/cr_components/searchbox/searchbox_compose_button.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';

import {GlifAnimationState} from '//resources/cr_components/composebox/common.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
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
import type {PageCallbackRouter, PageHandlerInterface, TabInfo} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ModelMode, ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './omnibox.css.js';
import {getHtml} from './omnibox.html.js';

export interface OmniboxEverywhereOmniboxElement {
  $: {
    input: SearchboxInputElement,
    inputWrapper: HTMLElement,
    matches: SearchboxDropdownElement,
  };
}

// Note: Copied from omnibox_popup_searchbox.ts.
//       I18nMixinLit may eventually be moved to SearchboxMixin.
const OmniboxEverywhereOmniboxElementBase =
    SearchboxMixin(I18nMixinLit(WebUiListenerMixinLit(CrLitElement)));

export class OmniboxEverywhereOmniboxElement extends
    OmniboxEverywhereOmniboxElementBase implements SearchboxMixinInterface {
  static get is() {
    return 'omnibox-everywhere-omnibox';
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
      animationState_: {type: String},
      composeButtonEnabled: {type: Boolean, reflect: true},
      ntpRealboxNextEnabled: {type: Boolean, reflect: true},
      contextMenuGlifAnimationState: {
        type: String,
        reflect: true,
      },
      inputState_: {type: Object},
      tabSuggestions_: {type: Array},
      searchboxLayoutMode: {type: String},
      tabSuggestionsLoading_: {type: Boolean},
      tabSuggestionsHasLoaded_: {type: Boolean},
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
  protected accessor useWebkitSearchIcons_: boolean = true;
  protected accessor animationState_: GlowAnimationState =
      GlowAnimationState.NONE;
  protected accessor composeButtonEnabled: boolean =
      loadTimeData.getBoolean('searchboxShowComposeEntrypoint');
  protected accessor ntpRealboxNextEnabled: boolean =
      loadTimeData.getBoolean('ntpRealboxNextEnabled');
  accessor contextMenuGlifAnimationState: GlifAnimationState =
      GlifAnimationState.STARTED;
  protected accessor inputState_: InputState|null = null;
  protected accessor tabSuggestions_: TabInfo[] = [];
  protected accessor searchboxLayoutMode: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor tabSuggestionsLoading_: boolean = false;
  protected accessor tabSuggestionsHasLoaded_: boolean = false;

  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private autocompleteResultChangedListenerId_: number|null = null;
  private inputStateListenerId_: number|null = null;

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
    this.inputStateListenerId_ =
        this.callbackRouter_.onInputStateChanged.addListener(
            (inputState: InputState) => {
              this.inputState_ = inputState;
            });
    this.pageHandler_.getInputState().then((response) => {
      this.inputState_ = response.state;
    });
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.autocompleteResultChangedListenerId_ !== null) {
      this.callbackRouter_.removeListener(
          this.autocompleteResultChangedListenerId_);
      this.autocompleteResultChangedListenerId_ = null;
    }
    if (this.inputStateListenerId_ !== null) {
      this.callbackRouter_.removeListener(this.inputStateListenerId_);
      this.inputStateListenerId_ = null;
    }
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
    if (this.ntpRealboxNextEnabled) {
      return 'Ask Google';
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

  protected async openComposeboxWithMode_(
      mode: ToolMode = ToolMode.kUnspecified,
      model: ModelMode = ModelMode.kUnspecified) {
    this.animationState_ = GlowAnimationState.NONE;
    await this.updateComplete;
    this.animationState_ = GlowAnimationState.LISTENING;
    setTimeout(() => {
      this.fire('open-composebox', {
        text: this.$.input.inputElement.value,
        mode: mode,
        model: model,
      });
    }, 300);
  }

  protected onComposeClick_() {
    this.openComposeboxWithMode_();
  }

  protected onContextMenuEntrypointClick_() {
    this.pageHandler().activateMetricsFunnel('PlusButton');
  }

  protected async refreshTabSuggestions_(forceRefresh: boolean = false) {
    if (this.tabSuggestionsLoading_ ||
        (this.tabSuggestionsHasLoaded_ && !forceRefresh)) {
      return;
    }
    this.tabSuggestionsLoading_ = true;
    try {
      const {tabs} = await this.pageHandler_.getRecentTabs();
      this.tabSuggestions_ = [...tabs];
      this.tabSuggestionsHasLoaded_ = true;
    } finally {
      this.tabSuggestionsLoading_ = false;
    }
  }

  protected onContextMenuOpened_() {
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onContextMenuClosed_() {
    this.tabSuggestionsHasLoaded_ = false;
  }

  protected onRequestTabSuggestionsLoad() {
    this.refreshTabSuggestions_(/*forceRefresh=*/ true);
  }

  protected onToolClick_(e: CustomEvent<{toolMode: ToolMode}>) {
    this.openComposeboxWithMode_(e.detail.toolMode);
  }

  protected onDeepSearchClick_() {
    this.openComposeboxWithMode_(ToolMode.kDeepSearch);
  }

  protected onCreateImageClick_() {
    this.openComposeboxWithMode_(ToolMode.kImageGen);
  }

  protected onModelClick_(e: CustomEvent<{model: ModelMode}>) {
    this.openComposeboxWithMode_(ToolMode.kUnspecified, e.detail.model);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-omnibox': OmniboxEverywhereOmniboxElement;
  }
}

customElements.define(
    OmniboxEverywhereOmniboxElement.is, OmniboxEverywhereOmniboxElement);
