// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

import '../components/common_styles/oobe_common_styles.css.js';
import './assistant_common_styles.css.js';
import './assistant_icons.html.js';
import './assistant_loading.js';
import './assistant_related_info.js';
import './assistant_voice_match.js';
import './assistant_value_prop.js';
import './setting_zippy.js';

import {loadTimeData} from '//resources/ash/common/load_time_data.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MultiStepMixin} from '../components/mixins/multi_step_mixin.js';
import {OobeDialogHostMixin} from '../components/mixins/oobe_dialog_host_mixin.js';
import {OobeI18nMixin} from '../components/mixins/oobe_i18n_mixin.js';

import {AssistantOptinFlowType, BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';


/**
 * UI mode for the dialog.
 * @enum {string}
 */
const AssistantUIState = {
  LOADING: 'loading',
  VALUE_PROP: 'value-prop',
  RELATED_INFO: 'related-info',
  VOICE_MATCH: 'voice-match',
};

/**
 * @constructor
 * @extends {PolymerElement}
 */
const AssistantOptInFlowBase =
    OobeDialogHostMixin(MultiStepMixin(OobeI18nMixin(PolymerElement)));

/**
 * @polymer
 */
export class AssistantOptInFlow extends AssistantOptInFlowBase {
  static get is() {
    return 'assistant-optin-flow-element';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  constructor() {
    super();

    /** @private {!BrowserProxy} */
    this.browserProxy_ = BrowserProxyImpl.getInstance();
  }

  /** @override */
  UI_STEPS() {
    return AssistantUIState;
  }

  /** @override */
  attached() {
    window.addEventListener('orientationchange', () => this.onWindowResized_());
    window.addEventListener('resize', () => this.onWindowResized_());
  }

  /** @override */
  detached() {
    window.removeEventListener(
        'orientationchange', () => this.onWindowResized_());
    window.removeEventListener('resize', () => this.onWindowResized_());
  }

  defaultUIStep() {
    return AssistantUIState.LOADING;
  }

  /**
   * Signal from host to show the screen.
   * @param {?string} type The type of the flow.
   * @param {?string} captionBarHeight The height of the caption bar.
   */
  onShow(type, captionBarHeight) {
    captionBarHeight = captionBarHeight ? captionBarHeight + 'px' : '0px';
    this.style.setProperty('--caption-bar-height', captionBarHeight);
    this.onWindowResized_();

    type = type ? type : AssistantOptinFlowType.CONSENT_FLOW.toString();
    const flowType = Number(type);
    switch (flowType) {
      case AssistantOptinFlowType.CONSENT_FLOW:
      case AssistantOptinFlowType.SPEAKER_ID_ENROLLMENT:
      case AssistantOptinFlowType.SPEAKER_ID_RETRAIN:
        this.flowType = flowType;
        break;
      default:
        console.error('Invalid flow type, using default.');
        this.flowType = AssistantOptinFlowType.CONSENT_FLOW;
        break;
    }

    this.boundShowLoadingScreen = () => this.showLoadingScreen();
    this.boundOnScreenLoadingError = () => this.onScreenLoadingError();
    this.boundOnScreenLoaded = () => this.onScreenLoaded();

    this.$.loading.onBeforeShow();
    this.$.loading.addEventListener('reload', () => this.onReload());

    switch (this.flowType) {
      case AssistantOptinFlowType.SPEAKER_ID_ENROLLMENT:
      case AssistantOptinFlowType.SPEAKER_ID_RETRAIN:
        this.$.voiceMatch.isFirstScreen = true;
        this.showStep(AssistantUIState.VOICE_MATCH);
        break;
      default:
        this.showStep(AssistantUIState.VALUE_PROP);
    }
    this.browserProxy_.initialized([this.flowType]);
  }

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   */
  reloadContent(data) {
    this.voiceMatchEnforcedOff = data['voiceMatchEnforcedOff'];
    this.shouldSkipVoiceMatch = data['shouldSkipVoiceMatch'];
    this.voiceMatchDisabled = loadTimeData.getBoolean('voiceMatchDisabled');
    data['flowType'] = this.flowType;
    this.$.valueProp.reloadContent(data);
    this.$.relatedInfo.reloadContent(data);
    this.$.voiceMatch.reloadContent(data);
  }

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param {string} type type of the setting zippy.
   * @param {!Object} data String and url for the setting zippy.
   */
  addSettingZippy(type, data) {
    switch (type) {
      case 'settings':
        this.$.valueProp.addSettingZippy(data);
        break;
      default:
        console.error('Undefined zippy data type: ' + type);
    }
  }

  /**
   * Show the next screen in the flow.
   */
  showNextScreen() {
    switch (this.currentStep) {
      case AssistantUIState.VALUE_PROP:
        this.showStep(AssistantUIState.RELATED_INFO);
        break;
      case AssistantUIState.RELATED_INFO:
        if (this.voiceMatchEnforcedOff || this.voiceMatchDisabled ||
            this.shouldSkipVoiceMatch) {
          this.browserProxy_.flowFinished();
        } else {
          this.showStep(AssistantUIState.VOICE_MATCH);
        }
        break;
      case AssistantUIState.VOICE_MATCH:
        this.browserProxy_.flowFinished();
        break;
      default:
        console.error('Undefined');
        this.browserProxy_.dialogClose();
    }
  }

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   */
  onVoiceMatchUpdate(state) {
    if (this.currentStep !== AssistantUIState.VOICE_MATCH) {
      return;
    }
    switch (state) {
      case 'listen':
        this.$.voiceMatch.listenForHotword();
        break;
      case 'process':
        this.$.voiceMatch.processingHotword();
        break;
      case 'done':
        this.$.voiceMatch.voiceMatchDone();
        break;
      case 'failure':
        this.onScreenLoadingError();
        break;
      default:
        break;
    }
  }

  /**
   * Called to show the next settings when there are multiple unbundled
   * activity control settings in the Value prop screen.
   */
  onValuePropUpdate() {
    if (this.currentStep !== AssistantUIState.VALUE_PROP) {
      return;
    }
    this.$.valueProp.showNextStep();
  }

  /**
   * Show the given step.
   *
   * @param {AssistantUIState} step The step to be shown.
   */
  showStep(step) {
    if (this.currentStep === step) {
      return;
    }
    if (this.currentStep) {
      this.applyToStepElements((screen) => {
        screen.removeEventListener('loading', this.boundShowLoadingScreen);
        screen.removeEventListener('error', this.boundOnScreenLoadingError);
        screen.removeEventListener('loaded', this.boundOnScreenLoaded);
      });
    }
    this.setUIStep(step);
    this.currentStep = step;
    this.applyToStepElements((screen) => {
      screen.addEventListener('loading', this.boundShowLoadingScreen);
      screen.addEventListener('error', this.boundOnScreenLoadingError);
      screen.addEventListener('loaded', this.boundOnScreenLoaded);
      screen.onShow();
    });
  }

  /**
   * Show the loading screen.
   */
  showLoadingScreen() {
    this.setUIStep(AssistantUIState.LOADING);
    this.$.loading.onShow();
  }

  /**
   * Called when the screen failed to load.
   */
  onScreenLoadingError() {
    this.setUIStep(AssistantUIState.LOADING);
    this.$.loading.onErrorOccurred();
  }

  /**
   * Called when all the content of current screen has been loaded.
   */
  onScreenLoaded() {
    this.setUIStep(this.currentStep);
    this.$.loading.onPageLoaded();
  }

  /**
   * Called when user request the screen to be reloaded.
   */
  onReload() {
    this.applyToStepElements((screen) => {
      screen.reloadPage();
    }, this.currentStep);
  }

  /**
   * Called during initialization, when the window is resized, or the window's
   * orientation is updated.
   */
  onWindowResized_() {
    // Dialog size and orientation value needs to be updated for in-session
    // assistant dialog.
    if (!document.documentElement.hasAttribute('screen')) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');
      if (window.innerWidth > window.innerHeight) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    }
    // In landscape mode, animation element should reside in subtitle slot which
    // is shown at the bottom left of the screen. In portrait mode, animation
    // element should reside in content slot which allows scrolling with the
    // rest of the content.
    const slot = window.innerWidth > window.innerHeight ? 'subtitle' : 'content';
    this.$.valueProp.getAnimationContainer().slot = slot;
    this.$.relatedInfo.getAnimationContainer().slot = slot;
  }
}

customElements.define(AssistantOptInFlow.is, AssistantOptInFlow);
