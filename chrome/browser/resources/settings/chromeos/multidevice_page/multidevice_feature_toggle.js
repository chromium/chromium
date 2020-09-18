// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A toggle button specially suited for the MultiDevice Settings UI use-case.
 * Instead of changing on clicks, it requests a pref change from the
 * MultiDevice service and/or triggers a password check to grab an auth token
 * for the user. It also receives real time updates on feature states and
 * reflects them in the toggle status.
 */
Polymer({
  is: 'settings-multidevice-feature-toggle',

  behaviors: [MultiDeviceFeatureBehavior],

  properties: {
    /** @type {!settings.MultiDeviceFeature} */
    feature: Number,

    /** @private {boolean} */
    checked_: Boolean,
  },

  listeners: {
    'click': 'onDisabledInnerToggleClick_',
  },

  // Note that, although this.feature does not change throughout the element's
  // lifecycle, it must be listed as an observer dependency to ensure that
  // this.feature is defined by the time of the observer's first call.
  observers: ['resetChecked_(feature, pageContentData)'],

  /** @override */
  focus() {
    this.$.toggle.focus();
  },

  /**
   * Because MultiDevice prefs are only meant to be controlled via the
   * MultiDevice mojo service, we need the cr-toggle to appear not to change
   * when pressed. This method resets it before a change is visible to the
   * user.
   * @private
   */
  resetChecked_() {
    this.checked_ = this.getFeatureState(this.feature) ===
        settings.MultiDeviceFeatureState.ENABLED_BY_USER;
  },

  /**
   * This handles the edge case in which the inner toggle (i.e., the cr-toggle)
   * is disabled. For context, the cr-toggle element naturally stops clicks
   * from propagating as long as its disabled attribute is false. However, if
   * the cr-toggle's disabled attribute is set to true, its pointer-event CSS
   * property is set to 'none' automatically. Thus, if the cr-toggle is clicked
   * while it is disabled, the click event targets the parent element directly
   * instead of propagating through the cr-toggle. This handler prevents such a
   * click from unintentionally bubbling up the tree.
   * @private
   */
  onDisabledInnerToggleClick_(event) {
    event.stopPropagation();
  },

  /**
   * Callback for clicking on the toggle. It attempts to toggle the feature's
   * status if the user is allowed.
   * @private
   */
  onChange_() {
    this.resetChecked_();

    // Pass the negation of |this.checked_|: this indicates that if the toggle
    // is checked, the intent is for it to be unchecked, and vice versa.
    this.fire(
        'feature-toggle-clicked',
        {feature: this.feature, enabled: !this.checked_});
  },
});
