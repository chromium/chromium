// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * voice match screen.
 */

import '//resources/ash/common/cr_elements/cr_lottie/cr_lottie.js';
import '//resources/ash/common/cr_elements/icons.html.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '../components/buttons/oobe_next_button.js';
import '../components/buttons/oobe_text_button.js';
import '../components/common_styles/oobe_dialog_host_styles.css.js';
import '../components/dialogs/oobe_adaptive_dialog.js';
import '../components/oobe_cr_lottie.js';
import './assistant_common_styles.css.js';
import './assistant_icons.html.js';
import './voice_match_entry.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {announceAccessibleMessage} from '//resources/ash/common/util.js';
import {afterNextRender, html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStepMixin} from '../components/mixins/multi_step_mixin.js';
import {OobeI18nMixin} from '../components/mixins/oobe_i18n_mixin.js';

import {BrowserProxyImpl} from './browser_proxy.js';


/** Maximum recording index. */
const MAX_INDEX = 4;

/**
 * Name of the screen.
 * @type {string}
 */
const VOICE_MATCH_SCREEN_ID = 'VoiceMatchScreen';

const VoiceMatchUIState = {
  INTRO: 'intro',
  RECORDING: 'recording',
  COMPLETED: 'completed',
  ALREADY_SETUP: 'already-setup',
};

/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {MultiStepMixinInterface}
 */
const AssistantVoiceMatchBase = MultiStepMixin(OobeI18nMixin(PolymerElement));

/**
 * @polymer
 */
class AssistantVoiceMatch extends AssistantVoiceMatchBase {
  static get is() {
    return 'assistant-voice-match';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Indicates whether to use same design for accept/decline buttons.
       */
      equalWeightButtons_: {
        type: Boolean,
        value: false,
      },

      /**
       * The given name of the user, if a child account is in use; otherwise,
       * this is an empty string.
       */
      childName_: {
        type: String,
        value: '',
      },

      /**
       * @private {boolean}
       */
      isTabletMode_: {
        type: Boolean,
        value: false,
      },
    };
  }

  constructor() {
    super();

    /**
     * Whether voice match is the first screen of the flow.
     * @type {boolean}
     */
    this.isFirstScreen = false;

    /**
     * Current recording index.
     * @type {number}
     * @private
     */
    this.currentIndex_ = 0;

    /**
     * The delay in ms between speaker ID enrollment finishes and the
     * voice-match-done action is reported to chrome.
     * @private {number}
     */
    this.doneActionDelayMs_ = 3000;

    /** @private {?BrowserProxy} */
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  }

  defaultUIStep() {
    return VoiceMatchUIState.INTRO;
  }

  get UI_STEPS() {
    return VoiceMatchUIState;
  }

  /**
   * Overrides the default delay for sending voice-match-done action.
   * @param {number} delay The delay to be used in tests.
   */
  setDoneActionDelayForTesting(delay) {
    this.doneActionDelayMs_ = delay;
  }

  /**
   * On-tap event handler for skip button.
   *
   * @private
   */
  onSkipTap_() {
    this.$['voice-match-lottie'].playing = false;
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['skip-pressed']);
  }

  /**
   * On-tap event handler for agree button.
   *
   * @private
   */
  onAgreeTap_() {
    this.setUIStep(VoiceMatchUIState.RECORDING);
    this.dispatchEvent(
        new CustomEvent('loading', {bubbles: true, composed: true}));
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['record-pressed']);
  }

  /**
   * Reset the status of page elements.
   *
   * @private
   */
  resetElements_() {
    this.currentIndex_ = 0;

    this.$['voice-match-entries'].hidden = false;
    this.$['later-button'].hidden = false;
    this.$['loading-animation'].hidden = true;

    for (let i = 0; i < MAX_INDEX; ++i) {
      const entry = this.$['voice-entry-' + i];
      entry.removeAttribute('active');
      entry.removeAttribute('completed');
    }
  }

  /**
   * Reload the page with the given settings data.
   */
  reloadContent(data) {
    this.equalWeightButtons_ = data['equalWeightButtons'];
    this.childName_ = data['childName'];
    this.isTabletMode_ = data['isTabletMode'];
  }

  /**
   * Reloads voice match flow.
   */
  reloadPage() {
    this.setUIStep(VoiceMatchUIState.INTRO);
    if (!this.equalWeightButtons_) {
      this.$['agree-button'].focus();
    }
    this.resetElements_();
    this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['reload-requested']);
    this.dispatchEvent(
        new CustomEvent('loaded', {bubbles: true, composed: true}));
  }

  /**
   * Called when the server is ready to listening for hotword.
   */
  listenForHotword() {
    if (this.currentIndex_ === 0) {
      this.dispatchEvent(
          new CustomEvent('loaded', {bubbles: true, composed: true}));
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchRecording'));
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchA11yMessage'));
    }
    const currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.setAttribute('active', true);
  }

  /**
   * Called when the server has detected and processing hotword.
   */
  processingHotword() {
    const currentEntry = this.$['voice-entry-' + this.currentIndex_];
    currentEntry.removeAttribute('active');
    currentEntry.setAttribute('completed', true);
    this.currentIndex_++;
    if (this.currentIndex_ === MAX_INDEX) {
      this.$['voice-match-entries'].hidden = true;
      this.$['later-button'].hidden = true;
      this.$['loading-animation'].hidden = false;
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchUploading'));
    } else {
      announceAccessibleMessage(
          loadTimeData.getString('assistantVoiceMatchComplete'));
    }
  }

  voiceMatchDone() {
    this.dispatchEvent(
        new CustomEvent('loaded', {bubbles: true, composed: true}));
    announceAccessibleMessage(
        loadTimeData.getString('assistantVoiceMatchCompleted'));
    if (this.currentIndex_ !== MAX_INDEX) {
      // Existing voice model found on cloud. No need to train.
      this.$['later-button'].hidden = true;
      this.setUIStep(VoiceMatchUIState.ALREADY_SETUP);
    } else {
      this.setUIStep(VoiceMatchUIState.COMPLETED);
    }

    window.setTimeout(() => {
      this.$['voice-match-lottie'].playing = false;
      this.browserProxy_.userActed(VOICE_MATCH_SCREEN_ID, ['voice-match-done']);
    }, this.doneActionDelayMs_);
  }

  /**
   * Signal from host to show the screen.
   */
  onShow() {
    if (this.isFirstScreen) {
      // If voice match is the first screen, slightly delay showing the content
      // for the lottie animations to load.
      this.dispatchEvent(
          new CustomEvent('loading', {bubbles: true, composed: true}));
      window.setTimeout(() => {
        this.dispatchEvent(
            new CustomEvent('loaded', {bubbles: true, composed: true}));
      }, 100);
    }

    this.browserProxy_.screenShown(VOICE_MATCH_SCREEN_ID);
    this.$['voice-match-lottie'].playing = true;
    afterNextRender(this, () => {
      if (!this.equalWeightButtons_) {
        this.$['agree-button'].focus();
      }
    });
  }

  /**
   * Returns the text for dialog title.
   */
  getDialogTitle_(locale, uiStep, childName) {
    if (uiStep === VoiceMatchUIState.INTRO) {
      return childName ?
          this.i18n('assistantVoiceMatchTitleForChild', childName) :
          this.i18n('assistantVoiceMatchTitle');
    } else if (uiStep === VoiceMatchUIState.RECORDING) {
      return childName ?
          this.i18n('assistantVoiceMatchRecordingForChild', childName) :
          this.i18n('assistantVoiceMatchRecording');
    } else if (uiStep === VoiceMatchUIState.COMPLETED) {
      return this.i18n('assistantVoiceMatchCompleted');
    }
    return trustedTypes.emptyHTML;
  }

  /**
   * Returns the text for subtitle.
   */
  getSubtitleMessage_(locale, uiStep, childName) {
    if (uiStep === VoiceMatchUIState.INTRO) {
      return childName ? this.i18nAdvanced(
                             'assistantVoiceMatchMessageForChild',
                             {substitutions: [childName]}) :
                         this.i18nAdvanced('assistantVoiceMatchMessage');
    } else if (
        uiStep === VoiceMatchUIState.RECORDING ||
        uiStep === VoiceMatchUIState.COMPLETED) {
      return this.i18nAdvanced(
          'assistantVoiceMatchFooterForChild', {substitutions: [childName]});
    }
    return trustedTypes.emptyHTML;
  }

  getVoiceMatchAnimationUrl_(isTabletMode) {
    return './assistant_optin/voice_' + (isTabletMode ? 'tablet' : 'laptop') +
        '.json';
  }
}

customElements.define(AssistantVoiceMatch.is, AssistantVoiceMatch);
