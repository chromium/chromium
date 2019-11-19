// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for the security token PIN dialog shown during
 * sign-in.
 */

Polymer({
  is: 'security-token-pin',

  behaviors: [OobeDialogHostBehavior, I18nBehavior],

  properties: {
    /**
     * Contains the OobeTypes.SecurityTokenPinDialogParameters object. It can be
     * null when our element isn't used.
     *
     * Changing this field resets the dialog state. (Please note that, due to
     * the Polymer's limitation, only assigning a new object is observed;
     * changing just a subproperty won't work.)
     */
    parameters: {
      type: Object,
      observer: 'onParametersChanged_',
    },

    /**
     * The i18n string ID containing the error label to be shown to the user.
     * Is undefined when there's no error label.
     * @private
     */
    errorLabelId_: {
      type: String,
      computed: 'computeErrorLabelId_(parameters)',
    },

    /**
     * Whether the current state is the wait for the processing completion
     * (i.e., the backend is verifying the entered PIN).
     * @private
     */
    processingCompletion_: {
      type: Boolean,
      value: false,
    },

    /**
     * Whether the user has made changes in the input field since the dialog
     * was initialized or reset.
     * @private
     */
    userEdited_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * Returns the i18n string ID for the current error label.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {string|undefined}
   * @private
   */
  computeErrorLabelId_: function(parameters) {
    if (!parameters)
      return;
    switch (parameters.errorLabel) {
      case OobeTypes.SecurityTokenPinDialogErrorType.NONE:
        return;
      case OobeTypes.SecurityTokenPinDialogErrorType.UNKNOWN:
        return 'securityTokenPinDialogUnknownError';
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PIN:
        return 'securityTokenPinDialogUnknownInvalidPin';
      case OobeTypes.SecurityTokenPinDialogErrorType.INVALID_PUK:
        return 'securityTokenPinDialogUnknownInvalidPuk';
      case OobeTypes.SecurityTokenPinDialogErrorType.MAX_ATTEMPTS_EXCEEDED:
        return 'securityTokenPinDialogUnknownMaxAttemptsExceeded';
      default:
        assertNotReached(`Unexpected enum value: ${parameters.errorLabel}`);
    }
  },

  /**
   * Invoked when the "Back" button is clicked.
   * @private
   */
  onBackClicked_: function() {
    this.fire('cancel');
  },

  /**
   * Invoked when the "Next" button is clicked.
   * @private
   */
  onNextClicked_: function() {
    if (this.processingCompletion_) {
      // Race condition: This could happen if Polymer hasn't yet updated the
      // "disabled" state of the "Next" button before the user clicked on it for
      // the second time.
      return;
    }
    this.processingCompletion_ = true;
    this.fire('completed', this.$.pinKeyboard.value);
  },

  /**
   * Observer that is called when the |parameters| property gets changed.
   * @private
   */
  onParametersChanged_: function() {
    // Reset the dialog to the initial state.
    this.$.pinKeyboard.value = '';
    this.processingCompletion_ = false;
    this.userEdited_ = false;
    this.$.pinKeyboard.focusInput();
  },

  /**
   * Observer that is called when the user changes the PIN input field.
   * @private
   */
  onPinChange_: function() {
    this.userEdited_ = true;
  },

  /**
   * Returns whether an error message should be displayed to the user.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  hasError_: function(parameters, userEdited) {
    if (!parameters)
      return false;
    if (parameters.attemptsLeft != -1)
      return true;
    return parameters.errorLabel !=
        OobeTypes.SecurityTokenPinDialogErrorType.NONE &&
        !userEdited;
  },

  /**
   * Returns whether the error label should be shown.
   * @param {string} errorLabelId
   * @param {boolean} userEdited
   * @return {boolean}
   * @private
   */
  isErrorLabelVisible_: function(errorLabelId, userEdited) {
    return errorLabelId && !userEdited;
  },

  /**
   * Returns whether the PIN attempts left count should be shown.
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @return {boolean}
   * @private
   */
  isAttemptsLeftVisible_: function(parameters) {
    return parameters && parameters.attemptsLeft != -1;
  },

  /**
   * Returns the aria label to be used for the PIN input field.
   * @param {string} locale
   * @param {OobeTypes.SecurityTokenPinDialogParameters} parameters
   * @param {string} errorLabelId
   * @param {boolean} userEdited
   * @return {string}
   * @private
   */
  getAriaLabel_: function(locale, parameters, errorLabelId, userEdited) {
    var pieces = [];
    if (this.isErrorLabelVisible_(errorLabelId, userEdited)) {
      pieces.push(this.i18n(errorLabelId));
      pieces.push(this.i18n('securityTokenPinDialogTryAgain'));
    }
    if (this.isAttemptsLeftVisible_(parameters)) {
      pieces.push(this.i18n(
          'securityTokenPinDialogAttemptsLeft', parameters.attemptsLeft));
    }
    // Note: The language direction is not taken into account here, since the
    // order of pieces follows the reading order.
    return pieces.join(' ');
  },
});
