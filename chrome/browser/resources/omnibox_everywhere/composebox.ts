// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox_dropdown.js';
import '//resources/cr_components/composebox/composebox_file_inputs.js';
import '//resources/cr_components/composebox/composebox_input.js';
import '//resources/cr_components/composebox/composebox_tool_chip.js';
import '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import '//resources/cr_components/composebox/composebox_submit.js';
import '//resources/cr_components/composebox/file_carousel.js';
import '//resources/cr_components/search/animated_glow.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_elements/cr_action_menu/cr_action_menu.js';

import {getLoadTimeBoolean} from '//resources/cr_components/composebox/common.js';
import type {PageHandlerRemote} from '//resources/cr_components/composebox/composebox.mojom-webui.js';
import type {ComposeboxDropdownElement} from '//resources/cr_components/composebox/composebox_dropdown.js';
import type {ComposeboxFileInputsElement} from '//resources/cr_components/composebox/composebox_file_inputs.js';
import type {ComposeboxInputElement} from '//resources/cr_components/composebox/composebox_input.js';
import {ComposeboxEmbedderMixin, SubmitButtonIconType} from '//resources/cr_components/composebox/composebox_mixin.js';
import {ComposeboxProxyImpl} from '//resources/cr_components/composebox/composebox_proxy.js';
import type {ContextualEntrypointAndMenuElement} from '//resources/cr_components/composebox/contextual_entrypoint_and_menu.js';
import type {ContextualEntrypointButtonElement} from '//resources/cr_components/composebox/contextual_entrypoint_button.js';
import {GlowAnimationState} from '//resources/cr_components/search/constants.js';
import {AnchorAlignment} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import type {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter as SearchboxPageCallbackRouter, PageHandlerRemote as SearchboxPageHandlerRemote} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {ToolMode} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './composebox.css.js';
import {getHtml} from './composebox.html.js';
import {UnboundedMenuManager} from './unbounded_utils.js';

export interface OmniboxEverywhereComposeboxElement {
  $: {
    composeboxInput: ComposeboxInputElement,
    composebox: HTMLElement,
    matches: ComposeboxDropdownElement,
    fileInputs: ComposeboxFileInputsElement,
  };
}

export class OmniboxEverywhereComposeboxElement extends ComposeboxEmbedderMixin
(CrLitElement) {
  static get is() {
    return 'omnibox-everywhere-composebox';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      applyContextButtonBackground: {
        reflect: true,
        type: Boolean,
      },
      entrypointName: {type: String, reflect: true},
      disableComposeboxAnimation: {type: Boolean},
      energyEffectAnimationEnabled: {type: Boolean},
      submitButtonIconType: {type: String},
      screenshotMenuOpen: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  /**
   * Entrypoint name used by SearchAnimatedGlowElement and
   * ComposeboxEmbedderMixin to apply embedder-specific styling and themes.
   */
  accessor entrypointName: string = 'OmniboxEverywhere';
  accessor disableComposeboxAnimation: boolean = false;
  accessor applyContextButtonBackground: boolean = false;
  override accessor energyEffectAnimationEnabled: boolean =
      getLoadTimeBoolean('composeboxEnergyEffectAnimationEnabled', true);
  override accessor submitButtonIconType = SubmitButtonIconType.FORWARD;
  protected accessor screenshotMenuOpen: boolean = false;

  override onVoiceSearchButtonClick() {
    this.dispatchEvent(
        new Event('open-voice-search', {bubbles: true, composed: true}));
  }

  protected onLensSearchClick_(e: Event) {
    this.screenshotMenuOpen = true;
    const menu =
        this.shadowRoot.querySelector<CrActionMenuElement>('#screenshotMenu')!;
    const anchor = e.currentTarget as HTMLElement;
    const rect = anchor.getBoundingClientRect();

    menu.showAtPosition({
      top: rect.top,
      left: rect.left,
      height: rect.height - 2,
      width: rect.width,
      anchorAlignmentX: AnchorAlignment.AFTER_START,
      anchorAlignmentY: AnchorAlignment.AFTER_END,
      maxX: Number.MAX_SAFE_INTEGER,
    });

    this.screenshotMenuManager_.onContextMenuOpened();
  }

  protected onScreenshotMenuClose_() {
    this.screenshotMenuOpen = false;
    this.screenshotMenuManager_.onContextMenuClosed();
  }

  protected onScreenshotWindowClick_() {
    // TODO(crbug.com/532197177): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }

  protected onScreenshotEntireScreenClick_() {
    // TODO(crbug.com/532197177): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }

  protected onScreenshotRegionClick_() {
    // TODO(crbug.com/532198850): Hook up screenshot/screenshare capture
    // trigger.
    this.shadowRoot.querySelector<CrActionMenuElement>(
                       '#screenshotMenu')!.close();
  }
  private webuiOmniboxSimplificationEnabled_: boolean =
      getLoadTimeBoolean('webuiOmniboxSimplificationEnabled', false);
  private pageHandler_: PageHandlerRemote;
  private searchboxCallbackRouter_: SearchboxPageCallbackRouter;
  private searchboxHandler_: SearchboxPageHandlerRemote;

  constructor() {
    super();
    this.pageHandler_ = ComposeboxProxyImpl.getInstance().handler;
    this.searchboxCallbackRouter_ =
        ComposeboxProxyImpl.getInstance().searchboxCallbackRouter;
    this.searchboxHandler_ = ComposeboxProxyImpl.getInstance().searchboxHandler;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.animationState = GlowAnimationState.EXPANDING;
    this.refreshTabSuggestions(/*forceRefresh=*/ true);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('inputState')) {
      const inToolMode = this.inputState?.activeTool !== ToolMode.kUnspecified;
      this.applyContextButtonBackground =
          this.webuiOmniboxSimplificationEnabled_ && !inToolMode;
    }
  }

  override firstUpdated(changedProperties: PropertyValues<this>) {
    super.firstUpdated(changedProperties);
    this.focusInput();
  }

  override getActiveElement(): Element|null {
    return this.shadowRoot?.activeElement || null;
  }

  override getDropdownElement(): ComposeboxDropdownElement {
    return this.$.matches;
  }

  override getInputElement(): ComposeboxInputElement {
    return this.$.composeboxInput;
  }

  override getPageHandler(): PageHandlerRemote {
    return this.pageHandler_;
  }

  override getSearchboxCallbackRouter(): SearchboxPageCallbackRouter {
    return this.searchboxCallbackRouter_;
  }

  override getSearchboxHandler(): SearchboxPageHandlerRemote {
    return this.searchboxHandler_;
  }

  override getContextEntrypointElement(): ContextualEntrypointButtonElement|
      ContextualEntrypointAndMenuElement|null {
    return this.shadowRoot?.querySelector<ContextualEntrypointAndMenuElement>(
               '#contextEntrypoint') ||
        null;
  }

  private unboundedMenuManager_ = new UnboundedMenuManager(
      () => this.getContextEntrypointElement() as HTMLElement | null);

  private screenshotMenuManager_ = new UnboundedMenuManager(
      () => this.shadowRoot?.querySelector('#screenshotMenu') ?? null, () => {
        const menu = this.shadowRoot?.querySelector<CrActionMenuElement>(
            '#screenshotMenu');
        menu?.close();
      });

  override computeShowDropdown(): boolean {
    return (this.unboundedMenuManager_?.isDialogOpen() ?? false) ||
        (this.screenshotMenuManager_?.isDialogOpen() ?? false) ||
        super.computeShowDropdown();
  }

  override onContextMenuOpened() {
    super.onContextMenuOpened();
    this.showDropdown = this.computeShowDropdown();
    this.unboundedMenuManager_.onContextMenuOpened();
  }

  override async onContextMenuClosed(): Promise<void> {
    await super.onContextMenuClosed();
    this.showDropdown = this.computeShowDropdown();
    this.unboundedMenuManager_.onContextMenuClosed();
  }


  override shouldShowDivider(): boolean {
    if (this.searchboxLayoutMode === 'TallBottomContext' &&
        !this.showFileCarousel) {
      return false;
    }

    return super.shouldShowDivider();
  }

  override selectFirstMatch() {
    if (this.result?.matches && this.result.matches.length > 0 &&
        this.result.matches[0]?.allowedToBeDefaultMatch &&
        this.selectedMatchIndex !== -1) {
      this.getDropdownElement().selectFirst();
    }
  }

  override submitQuery() {
    super.submitQuery();
  }

  override hasValidQuery(): boolean {
    // If there is at least one file that supports unimodal search, query is
    // valid.
    if (this.files.size > 0 &&
        Array.from(this.files.values()).some(file => file.supportsUnimodal)) {
      return true;
    }

    // If an autocomplete match is selected, it's a valid query.
    if (this.selectedMatchIndex >= 0 && !!this.result) {
      return true;
    }

    if (this.input.trim().length > 0) {
      return true;
    }

    return false;
  }

  setInputText(text: string) {
    const inputElem = this.getInputElement();
    if (inputElem) {
      inputElem.input = text;
    }
  }

  playGlowAnimation() {
    this.animationState = GlowAnimationState.NONE;
    requestAnimationFrame(() => {
      this.animationState = GlowAnimationState.EXPANDING;
    });
  }
}


declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-composebox': OmniboxEverywhereComposeboxElement;
  }
}

customElements.define(
    OmniboxEverywhereComposeboxElement.is, OmniboxEverywhereComposeboxElement);
