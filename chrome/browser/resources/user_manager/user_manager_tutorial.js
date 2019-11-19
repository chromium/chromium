// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
const TutorialSteps = {
  YOUR_CHROME: 'yourChrome',
  FRIENDS: 'friends',
  GUESTS: 'guests',
  COMPLETE: 'complete',
  NOT_YOU: 'notYou'
};

/**
 * @fileoverview 'user-manager-tutorial' is the element that controls the
 * tutorial steps for the user manager page.
 */
(function() {
Polymer({
  is: 'user-manager-tutorial',

  properties: {
    /**
     * True if the tutorial is currently hidden.
     * @private {boolean}
     */
    hidden_: {type: Boolean, value: true},

    /**
     * Current tutorial step ID.
     * @type {string}
     */
    currentStep_: {type: String, value: ''},

    /**
     * Enum values for the step IDs.
     * @private {TutorialSteps}
     */
    steps_: {readOnly: true, type: Object, value: TutorialSteps}
  },

  /**
   * Determines whether a given step is displaying.
   * @param {string} currentStep Index of the current step
   * @param {string} step Name of the given step
   * @return {boolean}
   * @private
   */
  isStepHidden_: function(currentStep, step) {
    return currentStep != step;
  },

  /**
   * Navigates to the next step.
   * @param {!Event} event
   * @private
   */
  onNextClick_: function(event) {
    this.currentStep_ =
        event.composedPath().find(e => e.tagName == 'CR-BUTTON').dataset.next;
  },

  /**
   * Handler for the link in the last step. Takes user to the create-profile
   * page in order to add a new profile.
   * @param {!Event} event
   * @private
   */
  onAddUserTap_: function(event) {
    this.onDissmissTap_();
    // Event is caught by user-manager-pages.
    this.fire('change-page', {page: 'create-user-page'});
  },

  /**
   * Starts the tutorial.
   */
  startTutorial: function() {
    this.currentStep_ = TutorialSteps.YOUR_CHROME;
    this.hidden_ = false;

    // If there's only one pod, show the steps to the side of the pod.
    // Otherwise, center the steps and disable interacting with the pods
    // while the tutorial is showing.
    const podRow = /** @type {{focusPod: !function(), pods: !Array}} */
        ($('pod-row'));

    this.classList.toggle('single-pod', podRow.pods.length == 1);

    podRow.focusPod();  // No focused pods.
    $('inner-container').classList.add('disabled');
  },

  /**
   * Ends the tutorial.
   * @private
   */
  onDissmissTap_: function() {
    $('inner-container').classList.remove('disabled');
    this.hidden_ = true;
  }
});
})();
