// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Possible UI states of the screen. Must be in the same order as
 * UpdateRequiredView::UIState enum values.
 */
/** @const */ var UI_STATE = {
  UPDATE_REQUIRED_MESSAGE: 'update-required-message',
  UPDATE_PROCESS: 'update-process',
  UPDATE_NEED_PERMISSION: 'update-need-permission',
  UPDATE_COMPLETED_NEED_REBOOT: 'update-completed-need-reboot',
  UPDATE_ERROR: 'update-error',
  EOL_REACHED: 'eol',
  UPDATE_NO_NETWORK: 'update-no-network'
};

Polymer({
  is: 'update-required-card-element',

  behaviors: [
    OobeI18nBehavior,
    OobeDialogHostBehavior,
    LoginScreenBehavior,
    MultiStepBehavior,
  ],

  EXTERNAL_API: [
    'setIsConnected',
    'setUpdateProgressUnavailable',
    'setUpdateProgressValue',
    'setUpdateProgressMessage',
    'setEstimatedTimeLeftVisible',
    'setEstimatedTimeLeft',
    'setUIState',
    'setEnterpriseAndDeviceName',
    'setEolMessage',
    'setIsUserDataPresent',
  ],

  properties: {
    /**
     * Is device connected to network?
     */
    isNetworkConnected: {type: Boolean, value: false},

    updateProgressUnavailable: {type: Boolean, value: true},

    updateProgressValue: {type: Number, value: 0},

    updateProgressMessage: {type: String, value: ''},

    estimatedTimeLeftVisible: {type: Boolean, value: false},

    enterpriseDomain: {type: String, value: ''},

    deviceName: {type: String, value: ''},

    eolAdminMessage_: {type: String, value: ''},

    usersDataPresent_: {type: Boolean, value: false},

    /**
     * Estimated time left in seconds.
     */
    estimatedTimeLeft: {
      type: Number,
      value: 0,
    },
  },

  ready() {
    this.initializeLoginScreen('UpdateRequiredScreen', {
      resetAllowed: true,
    });
    this.updateEolDeleteUsersDataMessage_();
  },

  /** Initial UI State for screen */
  getOobeUIInitialState() {
    return OOBE_UI_STATE.BLOCKING;
  },

  defaultUIStep() {
    return UI_STATE.UPDATE_REQUIRED_MESSAGE;
  },

  UI_STEPS: UI_STATE,

  onBeforeShow() {
    cr.ui.login.invokePolymerMethod(
        this.$['checking-downloading-update'], 'onBeforeShow');
  },

  /** Called after resources are updated. */
  updateLocalizedContent() {
    this.i18nUpdateLocale();
    this.updateEolDeleteUsersDataMessage_();
  },

  /** @param {string} domain Enterprise domain name */
  /** @param {string} device Device name */
  setEnterpriseAndDeviceName(enterpriseDomain, device) {
    this.enterpriseDomain = enterpriseDomain;
    this.deviceName = device;
  },

  /**
   * @param {string} eolMessage Not sanitized end of life message from policy
   */
  setEolMessage(eolMessage) {
    this.eolAdminMessage_ = loadTimeData.sanitizeInnerHtml(eolMessage);
  },

  /** @param {boolean} connected */
  setIsConnected(connected) {
    this.isNetworkConnected = connected;
  },

  /**
   * @param {boolean} unavailable
   */
  setUpdateProgressUnavailable(unavailable) {
    this.updateProgressUnavailable = unavailable;
  },

  /**
   * Sets update's progress bar value.
   * @param {number} progress Percentage of the progress bar.
   */
  setUpdateProgressValue(progress) {
    this.updateProgressValue = progress;
  },

  /**
   * Sets message below progress bar.
   * @param {string} message Message that should be shown.
   */
  setUpdateProgressMessage(message) {
    this.updateProgressMessage = message;
  },

  /**
   * Shows or hides downloading ETA message.
   * @param {boolean} visible Are ETA message visible?
   */
  setEstimatedTimeLeftVisible(visible) {
    this.estimatedTimeLeftVisible = visible;
  },

  /**
   * Sets estimated time left until download will complete.
   * @param {number} seconds Time left in seconds.
   */
  setEstimatedTimeLeft(seconds) {
    this.estimatedTimeLeft = seconds;
  },

  /**
   * Sets current UI state of the screen.
   * @param {number} ui_state New UI state of the screen.
   */
  setUIState(ui_state) {
    this.setUIStep(Object.values(UI_STATE)[ui_state]);
  },

  /** @param {boolean} data_present */
  setIsUserDataPresent(data_present) {
    this.usersDataPresent_ = data_present;
  },

  /**
   * @private
   */
  onSelectNetworkClicked_() {
    this.userActed('select-network');
  },

  /**
   * @private
   */
  onUpdateClicked_() {
    this.userActed('update');
  },

  /**
   * @private
   */
  onFinishClicked_() {
    this.userActed('finish');
  },

  /**
   * @private
   */
  onCellularPermissionRejected_() {
    this.userActed('update-reject-cellular');
  },

  /**
   * @private
   */
  onCellularPermissionAccepted_() {
    this.userActed('update-accept-cellular');
  },

  /**
   * Simple equality comparison function.
   * @private
   */
  eq_(one, another) {
    return one === another;
  },

  /**
   * @private
   */
  isEmpty_(eolAdminMessage) {
    return !eolAdminMessage || eolAdminMessage.trim().length == 0;
  },

  /**
   * @private
   */
  updateEolDeleteUsersDataMessage_() {
    this.$$('#deleteUsersDataMessage').innerHTML = this.i18nAdvanced(
        'eolDeleteUsersDataMessage',
        {substitutions: [loadTimeData.getString('deviceType')], attrs: ['id']});
    const linkElement = this.$$('#deleteDataLink');
    linkElement.setAttribute('is', 'action-link');
    linkElement.classList.add('oobe-local-link');
    linkElement.addEventListener('click', () => this.showConfirmationDialog_());
  },

  /**
   * @private
   */
  showConfirmationDialog_() {
    this.$.confirmationDialog.showDialog();
  },

  /**
   * @private
   */
  hideConfirmationDialog_() {
    this.$.confirmationDialog.hideDialog();
  },

  /**
   * @private
   */
  onDeleteUsersConfirmed_() {
    this.userActed('confirm-delete-users');
    this.hideConfirmationDialog_();
  },

});
