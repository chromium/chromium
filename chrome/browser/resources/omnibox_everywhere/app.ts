// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './omnibox.js';
import './composebox.js';
import '/strings.m.js';
import '//resources/cr_components/composebox/composebox_voice_search.js';
import '//resources/cr_components/search/animated_glow.js';

import type {ComposeboxState} from '//resources/cr_components/composebox/common.js';
import type {ComposeboxVoiceSearchElement, VoicePermissionPromptState} from '//resources/cr_components/composebox/composebox_voice_search.js';
import type {SearchAnimatedGlowElement} from '//resources/cr_components/search/animated_glow.js';
import {SearchboxBrowserProxy} from '//resources/cr_components/searchbox/searchbox_browser_proxy.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PageCallbackRouter} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';

import {getCss} from './app.css.js';
import {getHtml} from './app.html.js';
import type {OmniboxEverywhereComposeboxElement} from './composebox.js';
import type {OmniboxEverywhereOmniboxElement} from './omnibox.js';

const PERMISSION_PROMPT_CSS_CLASS = 'permission-prompt-showing';
const VOICE_IDLE_TIMEOUT_MS = 8000;
const VOICE_QUERY_LENGTH_LIMIT = 120;

export class OmniboxEverywhereAppElement extends CrLitElement {
  static get is() {
    return 'omnibox-everywhere-app';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      omniboxPopupDebugEnabled_: {
        type: Boolean,
        reflect: true,
      },
      isComposeboxMode_: {type: Boolean},
      searchboxLayoutMode_: {type: String},
      caretAnimationsEnabled_: {type: Boolean},
      disableComposeboxAnimation_: {type: Boolean},
      usePecApi_: {type: Boolean},
      isOblongShape_: {type: Boolean},
      contextManagementInComposeboxEnabled_: {type: Boolean},
      composeboxState_: {type: Object},
      showVoiceSearchOverlay_: {type: Boolean},
      hasVoiceSearchError_: {type: Boolean},
      voiceSearchTranscript_: {type: String},
      voiceSearchReceivedSpeech_: {type: Boolean},
      voiceSearchListening_: {type: Boolean},
      voiceIdleTimeoutMs_: {type: Number},
      voiceQueryLengthLimit_: {type: Number},
      callbackRouter_: {type: Object},
    };
  }

  protected accessor omniboxPopupDebugEnabled_ =
      loadTimeData.getBoolean('omniboxPopupDebugEnabled');
  protected accessor isComposeboxMode_: boolean = false;
  protected accessor searchboxLayoutMode_: string =
      loadTimeData.getString('searchboxLayoutMode');
  protected accessor caretAnimationsEnabled_: boolean =
      loadTimeData.getBoolean('caretAnimationEnabled');
  protected accessor disableComposeboxAnimation_: boolean =
      loadTimeData.getBoolean('composeboxAnimationDisabled');
  protected accessor usePecApi_: boolean =
      loadTimeData.getBoolean('contextualMenuUsePecApi');
  protected accessor isOblongShape_: boolean =
      loadTimeData.getBoolean('contextButtonShapeIsOblong');
  protected accessor contextManagementInComposeboxEnabled_: boolean =
      loadTimeData.getBoolean('contextManagementInComposeboxEnabled');
  protected accessor composeboxState_: ComposeboxState|null = null;
  protected accessor showVoiceSearchOverlay_: boolean = false;
  protected accessor hasVoiceSearchError_: boolean = false;
  protected accessor voiceSearchTranscript_: string = '';
  protected accessor voiceSearchReceivedSpeech_: boolean = false;
  protected accessor voiceSearchListening_: boolean = false;
  protected accessor voiceIdleTimeoutMs_: number = VOICE_IDLE_TIMEOUT_MS;
  protected accessor voiceQueryLengthLimit_: number = VOICE_QUERY_LENGTH_LIMIT;
  protected accessor callbackRouter_: PageCallbackRouter =
      SearchboxBrowserProxy.getInstance().callbackRouter;

  private isDebug_: boolean =
      new URLSearchParams(window.location.search).has('debug');
  private eventTracker_ = new EventTracker();

  override connectedCallback() {
    super.connectedCallback();
    this.eventTracker_.add(
        document.documentElement, 'visibilitychange',
        this.onVisibilitychange_.bind(this));
    this.onVisibilitychange_();
    if (!this.isDebug_) {
      this.eventTracker_.add(
          document.documentElement, 'contextmenu', (e: Event) => {
            e.preventDefault();
          });
    }
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.eventTracker_.removeAll();
  }

  protected async onOpenComposebox_(e: CustomEvent<ComposeboxState>) {
    this.composeboxState_ = e.detail;
    this.isComposeboxMode_ = true;
    await this.updateComplete;
    const composebox =
        this.shadowRoot.querySelector('omnibox-everywhere-composebox');
    if (composebox) {
      composebox.focusInput();
      composebox.playGlowAnimation();
    }
  }

  protected onCloseComposebox_() {
    this.isComposeboxMode_ = false;
  }

  protected onComposeboxSubmit_() {
    this.isComposeboxMode_ = false;
  }

  private async onVisibilitychange_() {
    if (document.visibilityState !== 'visible') {
      return;
    }

    await this.updateComplete;
    const searchbox =
        this.shadowRoot.querySelector('omnibox-everywhere-omnibox');
    if (searchbox) {
      searchbox.focusInput();
    }
  }

  // TODO(b/540973063): Extract common voice search lifecycle handling into
  // SearchboxMixin.
  protected async onOpenVoiceSearch_() {
    this.showVoiceSearchOverlay_ = true;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
    await this.updateComplete;
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && !dialog.open) {
      dialog.showModal();
    }
    const voiceSearch =
        this.shadowRoot?.querySelector<ComposeboxVoiceSearchElement>(
            '#voiceSearch');
    if (voiceSearch) {
      voiceSearch.start();
    }
  }

  protected onVoiceSearchOverlayClose_() {
    const dialog =
        this.shadowRoot?.querySelector<HTMLDialogElement>('#voiceSearchDialog');
    if (dialog && dialog.open) {
      dialog.close();
    }
    this.showVoiceSearchOverlay_ = false;
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = false;
  }

  protected onVoicePermissionChanged_(
      e: CustomEvent<VoicePermissionPromptState>) {
    if (e.detail.isOpened) {
      this.voiceSearchListening_ = false;
    } else {
      this.voiceSearchListening_ =
          this.showVoiceSearchOverlay_ && !this.hasVoiceSearchError_;
    }
    const audioAnimation =
        this.shadowRoot?.querySelector<SearchAnimatedGlowElement>(
            '#voiceSearchGlow');
    if (audioAnimation) {
      if (e.detail.isOpened) {
        audioAnimation.classList.add(PERMISSION_PROMPT_CSS_CLASS);
      } else {
        audioAnimation.classList.remove(PERMISSION_PROMPT_CSS_CLASS);
      }
    }
    const voiceSearchElement =
        this.shadowRoot?.querySelector<ComposeboxVoiceSearchElement>(
            '#voiceSearch');
    if (voiceSearchElement) {
      if (e.detail.isOpened) {
        voiceSearchElement.classList.add(PERMISSION_PROMPT_CSS_CLASS);
      } else {
        voiceSearchElement.classList.remove(PERMISSION_PROMPT_CSS_CLASS);
      }
    }
  }

  protected onVoiceSearchCancel_() {
    this.onVoiceSearchOverlayClose_();
  }

  protected onVoiceSearchError_() {
    if (!this.showVoiceSearchOverlay_) {
      return;
    }
    this.hasVoiceSearchError_ = true;
  }

  protected onVoiceSearchRestart_() {
    this.hasVoiceSearchError_ = false;
    this.voiceSearchListening_ = true;
    this.voiceSearchReceivedSpeech_ = false;
    this.voiceSearchTranscript_ = '';
  }

  protected onVoiceSearchTranscriptUpdate_(e: CustomEvent<string>) {
    this.voiceSearchTranscript_ = e.detail;
  }

  protected onVoiceSearchSpeechReceived_() {
    this.voiceSearchReceivedSpeech_ = true;
  }

  protected onVoiceSearchDialogClick_(e: MouseEvent) {
    const dialog = e.currentTarget as HTMLDialogElement;
    if (e.target === dialog) {
      this.onVoiceSearchOverlayClose_();
    }
  }

  protected onVoiceSearchFinalResult_(e: CustomEvent<string>) {
    this.onVoiceSearchOverlayClose_();
    const query = e.detail;
    if (query && query.trim().length > 0) {
      if (this.isComposeboxMode_) {
        const composebox =
            this.shadowRoot?.querySelector<OmniboxEverywhereComposeboxElement>(
                '#composebox');
        if (composebox) {
          composebox.setInputText(query);
          composebox.focusInput();
        }
      } else {
        const searchbox =
            this.shadowRoot?.querySelector<OmniboxEverywhereOmniboxElement>(
                '#searchbox');
        if (searchbox) {
          searchbox.setInputText(query);
          searchbox.focusInput();
          searchbox.queryAutocomplete(query, false, false);
        }
      }
    }
  }

  protected onVoiceSearchRecordingStopped_(e: CustomEvent<string>) {
    this.onVoiceSearchFinalResult_(e);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'omnibox-everywhere-app': OmniboxEverywhereAppElement;
  }
}

customElements.define(
    OmniboxEverywhereAppElement.is, OmniboxEverywhereAppElement);
