// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_components/composebox/composebox.js';
import './omnibox_composebox.js';
import '/strings.m.js';

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';
import type {ComposeboxElement} from '//resources/cr_components/composebox/composebox.js';
import {assert} from '//resources/js/assert.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {SearchContext} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {InputState} from '//resources/mojo/components/omnibox/composebox/composebox_query.mojom-webui.js';

import {getCss} from './aim_app.css.js';
import {getHtml} from './aim_app.html.js';
import {BrowserProxy} from './aim_browser_proxy.js';
import type {OmniboxComposeboxElement} from './omnibox_composebox.js';
import type {PageCallbackRouter, PageHandlerInterface} from './omnibox_popup_aim.mojom-webui.js';

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

  private eventTracker_ = new EventTracker();
  private pageHandler_: PageHandlerInterface;
  private callbackRouter_: PageCallbackRouter;
  private listenerIds_: number[] = [];
  private preserveContextOnClose_: boolean = false;

  constructor() {
    super();
    ColorChangeUpdater.forDocument().start();
    this.callbackRouter_ = BrowserProxy.getInstance().callbackRouter;
    this.pageHandler_ = BrowserProxy.getInstance().handler;
  }

  override connectedCallback() {
    super.connectedCallback();

    this.listenerIds_ = [
      this.callbackRouter_.onPopupShown.addListener(
          this.onPopupShown_.bind(this)),
      this.callbackRouter_.addContext.addListener(this.addContext_.bind(this)),
      this.callbackRouter_.focusInput.addListener(this.focusInput_.bind(this)),
      this.callbackRouter_.clearPopup.addListener(this.clearPopup_.bind(this)),
      this.callbackRouter_.setPreserveContextOnClose.addListener(
          this.setPreserveContextOnClose_.bind(this)),
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
      this.callbackRouter_.removeListener(listenerId);
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
    this.pageHandler_.showContextMenu(point);
  }

  protected onCloseComposebox_() {
    this.pageHandler_.requestClose();
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
