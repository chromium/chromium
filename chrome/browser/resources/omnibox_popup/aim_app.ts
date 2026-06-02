// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import './omnibox_composebox.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import type {VoicePermissionPromptState} from '//resources/cr_components/composebox/composebox_voice_search.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {SearchContext} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './aim_app.css.js';
import {getHtml} from './aim_app.html.js';
import type {OmniboxComposeboxElement} from './omnibox_composebox.js';
import {browserProxyFactory} from './omnibox_popup_aim.mojom-webui.js';
import type {BrowserProxy} from './omnibox_popup_aim.mojom-webui.js';

export interface OmniboxAimAppElement {
  $: {
    composebox: ComposeboxElement|OmniboxComposeboxElement,
  };
}

export class OmniboxAimAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-aim-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      composeboxForkEnabled_: {type: Boolean},
      searchboxLayoutMode_: {type: String},
      hasAllowedInputs_: {type: Boolean},
      caretAnimationsEnabled_: {type: Boolean},
      disableComposeboxAnimation_: {type: Boolean},
      energyEffectEnabled_: {
        type: Boolean,
        reflect: true,
      },
      energyEffectAnimationEnabled_: {
        type: Boolean,
        reflect: true,
      },
      disableVoiceSearchAnimation_: {type: Boolean},
      usePecApi_: {type: Boolean},
      smartTabSharingVisible_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      webuiOmniboxSimplificationEnabled_: {type: Boolean},
    };
  }

  protected accessor composeboxForkEnabled_: boolean =
      loadTimeData.getBoolean('composeboxForkEnabled');
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor hasAllowedInputs_: boolean = false;
  protected accessor disableComposeboxAnimation_: boolean =
      loadTimeData.getBoolean('composeboxAnimationDisabled');
  // Voice search animation is disabled outside of voice coherence.
  protected accessor disableVoiceSearchAnimation_: boolean =
      !loadTimeData.getBoolean('voiceSearchCoherenceComposeboxesEnabled');

  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor energyEffectEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  // The use of energyEffectEnabled to set energyEffectAnimationEnabled_ is
  // intentional. This is to align the gating properties for energy effects
  // across all surfaces (= Nextbox, Omnibox, and Realbox).
  protected accessor energyEffectAnimationEnabled_: boolean =
      loadTimeData.getBoolean('energyEffectEnabled');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor smartTabSharingVisible_: boolean =
      loadTimeData.getBoolean('composeboxSmartTabSharingVisible');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor webuiOmniboxSimplificationEnabled_: boolean =
      loadTimeData.getBoolean('webuiOmniboxSimplificationEnabled');

  private eventTracker_ = new EventTracker();
  private browserProxy_: BrowserProxy;
  private listenerIds_: number[] = [];
  private preserveContextOnClose_: boolean = false;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.browserProxy_ = browserProxyFactory.getInstance();
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.browserProxy_.callbackRouter.onPopupShown.addListener(
          this.onPopupShown_.bind(this)),
      this.browserProxy_.callbackRouter.addContext.addListener(
          this.addContext_.bind(this)),
      this.browserProxy_.callbackRouter.focusInput.addListener(
          this.focusInput_.bind(this)),
      this.browserProxy_.callbackRouter.clearPopup.addListener(
          this.clearPopup_.bind(this)),
      this.browserProxy_.callbackRouter.setPreserveContextOnClose.addListener(
          this.setPreserveContextOnClose_.bind(this)),
      this.browserProxy_.callbackRouter.onContextMenuClosed.addListener(
          this.onContextMenuClosed_.bind(this)),
    ];

    this.eventTracker_.add(
        this.$.composebox, 'input-state-changed',
        (e: CustomEvent<{inputState: InputState}>) => {
          const inputState = e.detail.inputState;
          this.hasAllowedInputs_ = inputState.allowedModels.length > 0 ||
              inputState.allowedTools.length > 0 ||
              inputState.allowedInputTypes.length > 0;
        });

    this.focusInput_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();

    for (const listenerId of this.listenerIds_) {
      this.browserProxy_.callbackRouter.removeListener(listenerId);
    }
    this.listenerIds_ = [];
  }

  protected getSearchboxLayoutMode_(): string {
    if (this.searchboxLayoutMode_.startsWith('Tall') &&
        !this.hasAllowedInputs_) {
      return 'Compact';
    }
    return this.searchboxLayoutMode_;
  }

  protected onContextMenuEntrypointClick_(
      e: CustomEvent<{x: number, y: number}>) {
    e.preventDefault();
    const point = {
      x: e.detail.x,
      y: e.detail.y,
    };
    // Force the button to keep its hover background visually while
    // the menu is open, even if the mouse doesn't move out of the button
    // area after clicking.
    const contextButton = this.$.composebox.getContextEntrypointElement();
    if (contextButton) {
      contextButton.classList.add('menu-open');
    }

    this.browserProxy_.handler.showContextMenu(point);
  }

  private onContextMenuClosed_() {
    const contextButton = this.$.composebox.getContextEntrypointElement();
    if (contextButton) {
      contextButton.classList.remove('menu-open');
    }
  }

  // Fired from voice search component in cr-composebox.
  protected onEmbeddedVoicePermissionPromptChanged(
      e: CustomEvent<VoicePermissionPromptState>) {
    if (e.detail.isOpened) {  // Permission prompt opened.
      if (this.$.composebox) {
        this.$.composebox.classList.add('has-embedded-permission-prompt');
        this.$.composebox.style.setProperty(
            '--cr_composebox_minimum_height', `${e.detail.height}px`);
        this.$.composebox.style.setProperty(
            '--cr_composebox_minimum_width', `${e.detail.width}px`);
      }
    } else {  // Permission prompt closed.
      if (this.$.composebox) {
        this.$.composebox.classList.remove('has-embedded-permission-prompt');
        this.$.composebox.style.removeProperty(
            '--cr_composebox_minimum_height');
        this.$.composebox.style.removeProperty('--cr_composebox_minimum_width');
      }
    }
  }
  protected onCloseComposebox_() {
    this.browserProxy_.handler.requestClose();
  }

  protected setPreserveContextOnClose_(preserveContextOnClose: boolean) {
    assert(document.visibilityState === 'visible');
    this.preserveContextOnClose_ = preserveContextOnClose;
  }

  private onPopupShown_(context: SearchContext) {
    if (!this.preserveContextOnClose_) {
      // Avoid showing the glow animation when coming back from a preserved
      // context on close as this indicates that the user is returning to the
      // widget after adding context.
      this.$.composebox.playGlowAnimation();
      this.$.composebox.setDefaultModel();
    }
    this.$.composebox.addSearchContext(context);
    this.focusInput_();
    this.preserveContextOnClose_ = false;
  }

  private addContext_(context: SearchContext) {
    this.$.composebox.addSearchContext(context);
    this.focusInput_();
  }

  private focusInput_() {
    this.$.composebox.focusInput();
  }

  private async clearPopup_(): Promise<{input: string}> {
    const input = this.$.composebox.input;
    if (!this.preserveContextOnClose_) {
      this.$.composebox.clearAllInputs(
          /* querySubmitted= */ false,
          /* shouldBlockAutoSuggestedTabs= */ false);
      this.$.composebox.clearAutocompleteMatches();
      this.$.composebox.resetModes();
      this.$.composebox.resetToolsAndModels();
    }
    await this.updateComplete;
    // Transfer input text to the location bar.
    return Promise.resolve({input});
  }

  protected onComposeboxSubmit_() {
    this.$.composebox.clearAllInputs(/* querySubmitted= */ true,
                                     /* shouldBlockAutoSuggestedTabs= */ false);
  }

  setSearchboxLayoutModeForTesting(mode: string) {
    this.searchboxLayoutMode_ = mode;
  }

  setHasAllowedInputsForTesting(hasAllowedInputs: boolean) {
    this.hasAllowedInputs_ = hasAllowedInputs;
  }

  getSearchboxLayoutModeForTesting(): string {
    return this.getSearchboxLayoutMode_();
  }

  getHasAllowedInputsForTesting(): boolean {
    return this.hasAllowedInputs_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-aim-app': OmniboxAimAppElement;
  }
}

customElements.define(OmniboxAimAppElement.is, OmniboxAimAppElement);
