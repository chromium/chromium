// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="utils.js">
// <include src="setting_zippy.js">
// <include src="voice_match_entry.js">
// <include src="browser_proxy.js">
// <include src="assistant_get_more.js">
// <include src="assistant_loading.js">
// <include src="assistant_related_info.js">
// <include src="assistant_third_party.js">
// <include src="assistant_value_prop.js">
// <include src="assistant_voice_match.js">

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

Polymer({
  is: 'assistant-optin-flow',

  behaviors: [OobeI18nBehavior, OobeDialogHostBehavior],

  /** @private {?assistant.BrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = assistant.BrowserProxyImpl.getInstance();
  },

  /**
   * Indicates the type of the opt-in flow.
   */
  FlowType: {
    // The whole consent flow.
    CONSENT_FLOW: 0,
    // The voice match enrollment flow.
    SPEAKER_ID_ENROLLMENT: 1,
    // The voice match retrain flow.
    SPEAKER_ID_RETRAIN: 2,
  },

  /**
   * Signal from host to show the screen.
   * @param {?string} type The type of the flow.
   * @param {?string} captionBarHeight The height of the caption bar.
   */
  onShow(type, captionBarHeight) {
    captionBarHeight = captionBarHeight ? captionBarHeight + 'px' : '0px';
    this.style.setProperty('--caption-bar-height', captionBarHeight);

    type = type ? type : this.FlowType.CONSENT_FLOW.toString();
    var flowType = Number(type);
    switch (flowType) {
      case this.FlowType.CONSENT_FLOW:
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
      case this.FlowType.SPEAKER_ID_RETRAIN:
        this.flowType = flowType;
        break;
      default:
        console.error('Invalid flow type, using default.');
        this.flowType = this.FlowType.CONSENT_FLOW;
        break;
    }

    this.boundShowLoadingScreen = this.showLoadingScreen.bind(this);
    this.boundOnScreenLoadingError = this.onScreenLoadingError.bind(this);
    this.boundOnScreenLoaded = this.onScreenLoaded.bind(this);

    this.$.loading.onBeforeShow();
    this.$.loading.addEventListener('reload', this.onReload.bind(this));

    switch (this.flowType) {
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
      case this.FlowType.SPEAKER_ID_RETRAIN:
        this.$.valueProp.hidden = true;
        this.$.voiceMatch.isFirstScreen = true;
        this.showScreen(this.$.voiceMatch);
        break;
      default:
        this.showScreen(this.$.valueProp);
    }
    this.browserProxy_.initialized([this.flowType]);
  },

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   */
  reloadContent(data) {
    this.voiceMatchEnforcedOff = data['voiceMatchEnforcedOff'];
    this.voiceMatchDisabled = loadTimeData.getBoolean('voiceMatchDisabled');
    this.betterAssistantEnabled =
        loadTimeData.getBoolean('betterAssistantEnabled');
    data['flowType'] = this.flowType;
    this.$.valueProp.reloadContent(data);
    this.$.relatedInfo.reloadContent(data);
    this.$.thirdParty.reloadContent(data);
    this.$.getMore.reloadContent(data);
  },

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
      case 'disclosure':
        this.$.thirdParty.addSettingZippy(data);
        break;
      case 'get-more':
        this.$.getMore.addSettingZippy(data);
        break;
      default:
        console.error('Undefined zippy data type: ' + type);
    }
  },

  /**
   * Show the next screen in the flow.
   */
  showNextScreen() {
    switch (this.currentScreen) {
      case this.$.valueProp:
        if (this.betterAssistantEnabled) {
          this.showScreen(this.$.relatedInfo);
        } else {
          this.showScreen(this.$.thirdParty);
        }
        break;
      case this.$.relatedInfo:
        if (this.voiceMatchEnforcedOff || this.voiceMatchDisabled) {
          this.browserProxy_.flowFinished();
        } else {
          this.showScreen(this.$.voiceMatch);
        }
        break;
      case this.$.thirdParty:
        if (this.voiceMatchEnforcedOff || this.voiceMatchDisabled) {
          this.showScreen(this.$.getMore);
        } else {
          this.showScreen(this.$.voiceMatch);
        }
        break;
      case this.$.voiceMatch:
        if (this.flowType == this.FlowType.SPEAKER_ID_ENROLLMENT ||
            this.flowType == this.FlowType.SPEAKER_ID_RETRAIN ||
            this.betterAssistantEnabled) {
          this.browserProxy_.flowFinished();
        } else {
          this.showScreen(this.$.getMore);
        }
        break;
      case this.$.getMore:
        this.browserProxy_.flowFinished();
        break;
      default:
        console.error('Undefined');
        this.browserProxy_.dialogClose();
    }
  },

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   */
  onVoiceMatchUpdate(state) {
    if (!this.currentScreen == this.$.voiceMatch) {
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
  },

  /**
   * Show the given screen.
   *
   * @param {Element} screen The screen to be shown.
   */
  showScreen(screen) {
    if (this.currentScreen == screen) {
      return;
    }

    this.$.loading.hidden = true;
    screen.hidden = false;
    screen.addEventListener('loading', this.boundShowLoadingScreen);
    screen.addEventListener('error', this.boundOnScreenLoadingError);
    screen.addEventListener('loaded', this.boundOnScreenLoaded);
    if (this.currentScreen) {
      this.currentScreen.hidden = true;
      this.currentScreen.removeEventListener(
          'loading', this.boundShowLoadingScreen);
      this.currentScreen.removeEventListener(
          'error', this.boundOnScreenLoadingError);
      this.currentScreen.removeEventListener(
          'loaded', this.boundOnScreenLoaded);
    }
    this.currentScreen = screen;
    this.currentScreen.onBeforeShow();
    this.currentScreen.onShow();
  },

  /**
   * Show the loading screen.
   */
  showLoadingScreen() {
    this.$.loading.hidden = false;
    this.currentScreen.hidden = true;
    this.$.loading.onShow();
  },

  /**
   * Called when the screen failed to load.
   */
  onScreenLoadingError() {
    this.$.loading.hidden = false;
    this.currentScreen.hidden = true;
    this.$.loading.onErrorOccurred();
  },

  /**
   * Called when all the content of current screen has been loaded.
   */
  onScreenLoaded() {
    this.currentScreen.hidden = false;
    this.$.loading.hidden = true;
    this.$.loading.onPageLoaded();
  },

  /**
   * Called when user request the screen to be reloaded.
   */
  onReload() {
    this.currentScreen.reloadPage();
  },
});
