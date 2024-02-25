// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/common/testing/test_import_manager.js';

import {Flags} from '/common/flags.js';
import {InstanceChecker} from '/common/instance_checker.js';
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Autoclick} from './autoclick/autoclick.js';
import {Dictation} from './dictation/dictation.js';
import {FaceGaze} from './facegaze/facegaze.js';
import {Magnifier} from './magnifier/magnifier.js';

declare global {
  var accessibilityCommon: AccessibilityCommon;
}

/**
 * Class to manage loading resources depending on which Accessibility features
 * are enabled.
 */
export class AccessibilityCommon {
  private autoclick_: Autoclick|null = null;
  private magnifier_: Magnifier|null = null;
  private dictation_: Dictation|null = null;
  private faceGaze_: FaceGaze|null = null;

  // For tests.
  private autoclickLoadCallbackForTest_: Function|null = null;
  // TODO(b:315990318): Migrate these callbacks to Function after
  // setOnLoadDesktopCallbackForTest() is migrated to typescript.
  private magnifierLoadCallbackForTest_: (() => void)|null = null;
  private dictationLoadCallbackForTest_: Function|null = null;
  private facegazeLoadCallbackForTest_: Function|null = null;

  static readonly FACEGAZE_PREF_NAME = 'settings.a11y.face_gaze.enabled';


  constructor() {
    this.init_();
  }

  static async init(): Promise<void> {
    await Flags.init();
    globalThis.accessibilityCommon = new AccessibilityCommon();
  }

  getAutoclickForTest(): Autoclick|null {
    return this.autoclick_;
  }

  getFaceGazeForTest(): FaceGaze|null {
    return this.faceGaze_;
  }

  getMagnifierForTest(): Magnifier|null {
    return this.magnifier_;
  }

  /**
   * Initializes the AccessibilityCommon extension.
   */
  private init_(): void {
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

    const faceGazeFeature =
        chrome.accessibilityPrivate.AccessibilityFeature.FACE_GAZE;
    chrome.accessibilityPrivate.isFeatureEnabled(faceGazeFeature, enabled => {
      if (!enabled) {
        return;
      }
      // TODO(b/309121742): Add FaceGaze pref to the accessibilityFeatures
      // extension API.
      chrome.settingsPrivate.getPref(
          AccessibilityCommon.FACEGAZE_PREF_NAME,
          pref => this.onFaceGazeUpdated_(pref));
      chrome.settingsPrivate.onPrefsChanged.addListener(prefs => {
        for (const pref of prefs) {
          if (pref.key === AccessibilityCommon.FACEGAZE_PREF_NAME) {
            this.onFaceGazeUpdated_(pref);
            break;
          }
        }
      });
    });

    // AccessibilityCommon is an IME so it shows in the input methods list
    // when it starts up. Remove from this list, Dictation will add it back
    // whenever needed.
    Dictation.removeAsInputMethod();
  }

  /**
   * Called when the autoclick feature is enabled or disabled.
   */
  private onAutoclickUpdated_(
      details: chrome.accessibilityFeatures.ChromeSettingsResponse): void {
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
   * Called when the FaceGaze feature is fetched enabled or disabled.
   */
  private onFaceGazeUpdated_(details: chrome.settingsPrivate.PrefObject): void {
    if (details.value && !this.faceGaze_) {
      // Initialize the FaceGaze extension.
      this.faceGaze_ = new FaceGaze();
      if (this.facegazeLoadCallbackForTest_) {
        this.facegazeLoadCallbackForTest_();
        this.facegazeLoadCallbackForTest_ = null;
      }
    } else if (!details.value && this.faceGaze_) {
      this.faceGaze_.onFaceGazeDisabled();
      this.faceGaze_ = null;
    }
  }

  /**
   * Called when the magnifier feature is fetched enabled or disabled.
   */
  private onMagnifierUpdated_(
      type: Magnifier.Type,
      details: chrome.accessibilityFeatures.ChromeSettingsResponse): void {
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
   */
  private onDictationUpdated_(
      details: chrome.accessibilityFeatures.ChromeSettingsResponse): void {
    if (details.value && !this.dictation_) {
      this.dictation_ = new Dictation();
      if (this.dictationLoadCallbackForTest_) {
        this.dictationLoadCallbackForTest_();
        this.dictationLoadCallbackForTest_ = null;
      }
    } else if (!details.value && this.dictation_) {
      this.dictation_.onDictationDisabled();
      this.dictation_ = null;
    }
  }

  /**
   * Used by C++ tests to ensure a feature load is completed.
   * Set on AccessibilityCommon in case the feature has not started up yet.
   */
  setFeatureLoadCallbackForTest(feature: string, callback: () => void): void {
    if (feature === 'autoclick') {
      if (!this.autoclick_) {
        this.autoclickLoadCallbackForTest_ = callback;
        return;
      }
      // Autoclick already loaded.
      this.autoclick_.setOnLoadDesktopCallbackForTest(callback);
    } else if (feature === 'dictation') {
      if (!this.dictation_) {
        this.dictationLoadCallbackForTest_ = callback;
        return;
      }
      // Dictation already loaded.
      callback();
    } else if (feature === 'magnifier') {
      if (!this.magnifier_) {
        this.magnifierLoadCallbackForTest_ = callback;
        return;
      }
      // Magnifier already loaded.
      this.magnifier_.setOnLoadDesktopCallbackForTest(callback);
    } else if (feature === 'facegaze') {
      if (!this.faceGaze_) {
        this.facegazeLoadCallbackForTest_ = callback;
        return;
      }
      // Facegaze already loaded.
      callback();
    }
  }
}


InstanceChecker.closeExtraInstances();
// Initialize the AccessibilityCommon extension.
AccessibilityCommon.init();

TestImportManager.exportForTesting(
    ['AccessibilityCommon', AccessibilityCommon]);
