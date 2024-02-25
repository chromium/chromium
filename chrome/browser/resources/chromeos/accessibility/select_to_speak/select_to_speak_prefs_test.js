// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../common/testing/mock_storage.js']);

/**
 * Browser tests for Select-to-speak's preferences and prefs migration.
 */
SelectToSpeakPrefsTest = class extends SelectToSpeakE2ETest {
  /** @override */
  constructor() {
    super();
    this.mockStorage_ = MockStorage;
    chrome.storage = this.mockStorage_;

    chrome.i18n = {
      getMessage(msgid) {
        return msgid;
      },
    };
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await this.resetStorage();
  }

  async resetStorage() {
    this.mockStorage_.clear();
    await selectToSpeak.prefsManager_.initPreferences();
  }

  // This must be done before setting STS rate and pitch for tests to work
  // properly.
  setGlobalRateAndPitch(rate, pitch) {
    chrome.settingsPrivate.setPref('settings.tts.speech_rate', rate);
    chrome.settingsPrivate.setPref('settings.tts.speech_pitch', pitch);
  }

  setStsRateAndPitch(rate, pitch) {
    this.mockStorage_.sync.set({rate});
    this.mockStorage_.sync.set({pitch});
  }

  setStoragePrefs(prefs) {
    this.mockStorage_.sync.set(prefs);

    for (const [pref, value] of Object.keys(prefs)) {
      if (value === undefined) {
        this.mockStorage_.sync.remove(pref);
      }
    }
  }

  ensurePrefsRemovedAndGlobalSetTo(rate, pitch) {
    const onPrefsRemovedFromStorage = this.newCallback(() => {
      // Once prefs are removed from storage, make sure the global prefs are
      // updated to the appropriate values.
      chrome.settingsPrivate.getPref(
          'settings.tts.speech_rate', this.newCallback(pref => {
            assertEquals(rate, pref.value);
          }));
      chrome.settingsPrivate.getPref(
          'settings.tts.speech_pitch', this.newCallback(pref => {
            assertEquals(pitch, pref.value);
          }));
    });
    this.mockStorage_.onChanged.addListener(prefs => {
      // checks that rate and pitch are removed.
      if (prefs !== undefined && !('rate' in prefs) && !('pitch' in prefs)) {
        onPrefsRemovedFromStorage();
      }
    });
  }

  async getStoragePrefs(prefNames) {
    // Filter list from this.mockStorage_.sync.get, which returns all prefs.
    const allPrefs =
        await new Promise(resolve => this.mockStorage_.sync.get([], resolve));
    const prefs = {};
    for (const prefName of prefNames) {
      if (allPrefs.hasOwnProperty(prefName)) {
        prefs[prefName] = allPrefs[prefName];
      }
    }
    return prefs;
  }

  async getSettingsPrefs(prefNames) {
    const prefs = {};
    for (const prefName of prefNames) {
      const {value} = await new Promise(
          resolve => chrome.settingsPrivate.getPref(prefName, resolve));
      prefs[prefName] = value;
    }
    return prefs;
  }

  async setStoragePrefsAndMigrate(prefs) {
    // Clear out Storage Prefs and Listeners.
    this.mockStorage_.clear();

    // Load Storage Prefs.
    this.mockStorage_.sync.set(prefs);

    // Reinitialize and Migrate Prefs.
    await selectToSpeak.prefsManager_.initPreferences();
  }

  async ensureStoragePrefsRemoved() {
    const prefs =
        await this.getStoragePrefs(SelectToSpeakPrefsTest.STORAGE_PREF_NAMES);
    assertEqualsJSON(prefs, {}, 'Storage Prefs still present.');
  }

  async ensureSettingsPrefsIncludes(expectedPrefs) {
    const actualPrefs = await this.getSettingsPrefs(Object.keys(expectedPrefs));
    assertEqualsJSON(
        expectedPrefs, actualPrefs,
        'Settings Prefs don\'t match expected results.');
  }
};

SelectToSpeakPrefsTest.STORAGE_PREF_NAMES = [
  'backgroundShading',
  'enhancedNetworkVoices',
  'enhancedVoiceName',
  'enhancedVoicesDialogShown',
  'highlightColor',
  'navigationControls',
  'voice',
  'voiceSwitching',
  'wordHighlight',
];

// TODO(katie): Test no alert -- this is hard because it happens last.
TEST_F(
    'SelectToSpeakPrefsTest', 'RemovesPrefsWithNoAlertIfAllDefault',
    function() {
      this.setGlobalRateAndPitch(1.0, 1.0);
      this.setStsRateAndPitch(1.0, 1.0);
      this.mockStorage_.callOnChangedListeners();

      this.ensurePrefsRemovedAndGlobalSetTo(1.0, 1.0);
    });

// TODO(katie): Test no alert -- this is hard because it happens last.
TEST_F(
    'SelectToSpeakPrefsTest', 'RemovesPrefsWithNoAlertIfAllEqual', function() {
      this.setGlobalRateAndPitch(1.5, 1.8);
      this.setStsRateAndPitch(1.5, 1.8);
      this.mockStorage_.callOnChangedListeners();

      this.ensurePrefsRemovedAndGlobalSetTo(1.5, 1.8);
    });

TEST_F('SelectToSpeakPrefsTest', 'SavesNonDefaultStsPrefsToGlobal', function() {
  this.setGlobalRateAndPitch(1.0, 1.0);
  this.setStsRateAndPitch(2.0, 2.5);
  this.mockStorage_.callOnChangedListeners();

  this.ensurePrefsRemovedAndGlobalSetTo(2.0, 2.5);
});

TEST_F(
    'SelectToSpeakPrefsTest',
    'DoesNotSaveNonDefaultStsPrefsToGlobalIfGlobalChanged', function() {
      this.setGlobalRateAndPitch(1.0, 1.5);
      this.setStsRateAndPitch(1.0, 2.5);
      this.mockStorage_.callOnChangedListeners();

      this.ensurePrefsRemovedAndGlobalSetTo(1.0, 1.5);
    });

TEST_F(
    'SelectToSpeakPrefsTest', 'DoesNotSaveStsPrefsToGlobalIfGlobalChanged',
    function() {
      this.setGlobalRateAndPitch(2.0, 1.0);
      this.setStsRateAndPitch(1.0, 1.0);
      this.mockStorage_.callOnChangedListeners();

      this.ensurePrefsRemovedAndGlobalSetTo(2.0, 1.0);
    });

// Clears all storage prefs, runs the migration in prefs_manager, verifies that
// prefs are set to their default values, and verifies that there are still no
// storage prefs. This mimics the state of a fresh user profile.
AX_TEST_F(
    'SelectToSpeakPrefsTest', 'DefaultSettingsPrefsSetAfterNoStoragePrefsSet',
    async function() {
      // Set no storage prefs.
      await this.setStoragePrefsAndMigrate({});

      // Check settings prefs match expected defaults.
      await this.ensureSettingsPrefsIncludes({
        [PrefsManager.BACKGROUND_SHADING_KEY]: false,
        [PrefsManager.ENHANCED_NETWORK_VOICES_KEY]: false,
        [PrefsManager.ENHANCED_VOICE_NAME_KEY]: 'default-wavenet',
        [PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY]: false,
        [PrefsManager.HIGHLIGHT_COLOR_KEY]: '#5e9bff',
        [PrefsManager.NAVIGATION_CONTROLS_KEY]: true,
        [PrefsManager.VOICE_NAME_KEY]: 'select_to_speak_system_voice',
        [PrefsManager.VOICE_SWITCHING_KEY]: false,
        [PrefsManager.WORD_HIGHLIGHT_KEY]: true,
      });

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved();
    });

// Sets some storage prefs, runs the migration in prefs_manager, verifies that
// the prefs that were set are migrated, verifies the rest are set according to
// their default values, and verifies that storage prefs are removed.
AX_TEST_F(
    'SelectToSpeakPrefsTest',
    'PrefsMigratedToSettingsAndDefaultsSetAfterSomeStoragePrefsSet',
    async function() {
      // Set some storage prefs.
      await this.setStoragePrefsAndMigrate({
        backgroundShading: true,
        enhancedNetworkVoices: true,
        enhancedVoiceName: 'enhanced_cool_voice',
        enhancedVoicesDialogShown: true,
        navigationControls: false,
        voiceSwitching: true,
      });

      // Check set storage prefs migrated to settings prefs and other settings
      // prefs match expected defaults.
      await this.ensureSettingsPrefsIncludes({
        [PrefsManager.BACKGROUND_SHADING_KEY]: true,
        [PrefsManager.ENHANCED_NETWORK_VOICES_KEY]: true,
        [PrefsManager.ENHANCED_VOICE_NAME_KEY]: 'enhanced_cool_voice',
        [PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY]: true,
        [PrefsManager.HIGHLIGHT_COLOR_KEY]: '#5e9bff',
        [PrefsManager.NAVIGATION_CONTROLS_KEY]: false,
        [PrefsManager.VOICE_NAME_KEY]: 'select_to_speak_system_voice',
        [PrefsManager.VOICE_SWITCHING_KEY]: true,
        [PrefsManager.WORD_HIGHLIGHT_KEY]: true,
      });

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved();
    });

// Sets all storage prefs, runs the migration in prefs_manager, verifies all
// prefs migrated to settings prefs, and verifies that storage prefs are
// removed.
AX_TEST_F(
    'SelectToSpeakPrefsTest',
    'AllPrefsMigratedToSettingsAfterAllStoragePrefsSet', async function() {
      // Set all storage prefs.
      await this.setStoragePrefsAndMigrate({
        backgroundShading: true,
        enhancedNetworkVoices: true,
        enhancedVoiceName: 'enhanced_cool_voice',
        enhancedVoicesDialogShown: true,
        highlightColor: '#123456',
        navigationControls: false,
        voice: 'cool_voice',
        voiceSwitching: true,
        wordHighlight: false,
      });

      // Check all storage prefs migrated to settings prefs.
      await this.ensureSettingsPrefsIncludes({
        [PrefsManager.BACKGROUND_SHADING_KEY]: true,
        [PrefsManager.ENHANCED_NETWORK_VOICES_KEY]: true,
        [PrefsManager.ENHANCED_VOICE_NAME_KEY]: 'enhanced_cool_voice',
        [PrefsManager.ENHANCED_VOICES_DIALOG_SHOWN_KEY]: true,
        [PrefsManager.HIGHLIGHT_COLOR_KEY]: '#123456',
        [PrefsManager.NAVIGATION_CONTROLS_KEY]: false,
        [PrefsManager.VOICE_NAME_KEY]: 'cool_voice',
        [PrefsManager.VOICE_SWITCHING_KEY]: true,
        [PrefsManager.WORD_HIGHLIGHT_KEY]: false,
      });

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved();
    });
