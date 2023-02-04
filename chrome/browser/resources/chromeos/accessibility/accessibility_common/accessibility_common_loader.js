// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Flags} from '../common/flags.js';
import {InstanceChecker} from '../common/instance_checker.js';

import {Autoclick} from './autoclick/autoclick.js';
import {Dictation} from './dictation/dictation.js';
import {Magnifier} from './magnifier/magnifier.js';

/**
 * Class to manage loading resources depending on which Accessibility features
 * are enabled.
 */
export class AccessibilityCommon {
  constructor() {
    /** @private {Autoclick} */
    this.autoclick_ = null;
    /** @private {Magnifier} */
    this.magnifier_ = null;
    /** @private {Dictation} */
    this.dictation_ = null;

    // For tests.
    /** @private {?function()} */
    this.autoclickLoadCallbackForTest_ = null;
    /** @private {?function()} */
    this.magnifierLoadCallbackForTest_ = null;

    this.init_();
  }

  static async init() {
    await Flags.init();
    globalThis.accessibilityCommon = new AccessibilityCommon();
  }

  /**
   * @return {Autoclick}
   */
  getAutoclickForTest() {
    return this.autoclick_;
  }

  /**
   * @return {Magnifier}
   */
  getMagnifierForTest() {
    return this.magnifier_;
  }

  /**
   * Initializes the AccessibilityCommon extension.
   * @private
   */
  init_() {
    chrome.accessibilityFeatures.autoclick.get(
        {}, details => this.onAutoclickUpdated_(details));
    chrome.accessibilityFeatures.autoclick.onChange.addListener(
        details => this.onAutoclickUpdated_(details));

    chrome.accessibilityFeatures.screenMagnifier.get(
        {},
        details =>
            this.onMagnifierUpdated_(Magnifier.Type.FULL_SCREEN, details));
    chrome.accessibilityFeatures.screenMagnifier.onChange.addListener(
        details =>
            this.onMagnifierUpdated_(Magnifier.Type.FULL_SCREEN, details));

    chrome.accessibilityFeatures.dockedMagnifier.get(
        {},
        details => this.onMagnifierUpdated_(Magnifier.Type.DOCKED, details));
    chrome.accessibilityFeatures.dockedMagnifier.onChange.addListener(
        details => this.onMagnifierUpdated_(Magnifier.Type.DOCKED, details));

    chrome.accessibilityFeatures.dictation.get(
        {}, details => this.onDictationUpdated_(details));
    chrome.accessibilityFeatures.dictation.onChange.addListener(
        details => this.onDictationUpdated_(details));

    // AccessibilityCommon is an IME so it shows in the input methods list
    // when it starts up. Remove from this list, Dictation will add it back
    // whenever needed.
    Dictation.removeAsInputMethod();
  }

  /**
   * Called when the autoclick feature is enabled or disabled.
   * @param {*} details
   * @private
   */
  onAutoclickUpdated_(details) {
    if (details.value && !this.autoclick_) {
      // Initialize the Autoclick extension.
      this.autoclick_ = new Autoclick();
      if (this.autoclickLoadCallbackForTest_) {
        this.autoclick_.setOnLoadDesktopCallbackForTest(
            this.autoclickLoadCallbackForTest_);
        this.autoclickLoadCallbackForTest_ = null;
      }
    } else if (!details.value && this.autoclick_) {
      // TODO(crbug.com/1096759): Consider using XHR to load/unload autoclick
      // rather than relying on a destructor to clean up state.
      this.autoclick_.onAutoclickDisabled();
      this.autoclick_ = null;
    }
  }

  /**
   * @param {!Magnifier.Type} type
   * @param {*} details
   * @private
   */
  onMagnifierUpdated_(type, details) {
    if (details.value && !this.magnifier_) {
      this.magnifier_ = new Magnifier(type);
      if (this.magnifierLoadCallbackForTest_) {
        this.magnifier_.setOnLoadDesktopCallbackForTest(
            this.magnifierLoadCallbackForTest_);
        this.magnifierLoadCallbackForTest_ = null;
      }
    } else if (
        !details.value && this.magnifier_ && this.magnifier_.type === type) {
      this.magnifier_.onMagnifierDisabled();
      this.magnifier_ = null;
    }
  }

  /**
   * Called when the dictation feature is enabled or disabled.
   * @param {*} details
   * @private
   */
  onDictationUpdated_(details) {
    if (details.value && !this.dictation_) {
      this.dictation_ = new Dictation();
    } else if (!details.value && this.dictation_) {
      this.dictation_.onDictationDisabled();
      this.dictation_ = null;
    }
  }

  /**
   * Used by C++ tests to ensure a feature load is completed.
   * Set on AccessibilityCommon in case the feature has not started up yet.
   * @param {string} feature The feature name.
   * @param {!function()} callback Callback for feature JS load complete.
   */
  setFeatureLoadCallbackForTest(feature, callback) {
    if (feature === 'autoclick') {
      if (!this.autoclick_) {
        this.autoclickLoadCallbackForTest_ = callback;
        return;
      }
      // Autoclick already loaded.
      this.autoclick_.setOnLoadDesktopCallbackForTest(callback);
    } else if (feature === 'magnifier') {
      if (!this.magnifier_) {
        this.magnifierLoadCallbackForTest_ = callback;
        return;
      }
      // Magnifier already loaded.
      this.magnifier_.setOnLoadDesktopCallbackForTest(callback);
    }
  }
}

InstanceChecker.closeExtraInstances();
// Initialize the AccessibilityCommon extension.
AccessibilityCommon.init();
