// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['select_to_speak_e2e_test_base.js']);
GEN_INCLUDE(['../../../../../../ui/webui/resources/js/cr.js']);
GEN_INCLUDE(['../../../../../test/data/webui/fake_chrome_event.js']);
GEN_INCLUDE(['fake_settings_private.js']);
GEN_INCLUDE(['mock_storage.js']);

/**
 * Browser tests for select-to-speak's feature to speak text
 * at the press of a keystroke.
 */
SelectToSpeakPrefsTest = class extends SelectToSpeakE2ETest {
  constructor() {
    super();
    this.mockStorage_ = MockStorage;
    chrome.storage = this.mockStorage_;

    this.mockSettingsPrivate_ = new settings.FakeSettingsPrivate([
      {type: 'number', key: 'settings.tts.speech_rate', value: 1.0},
      {type: 'number', key: 'settings.tts.speech_pitch', value: 1.0}
    ]);
    this.mockSettingsPrivate_.allowSetPref();
    chrome.settingsPrivate = this.mockSettingsPrivate_;

    chrome.i18n = {
      getMessage(msgid) {
        return msgid;
      }
    };
  }

  /** @override */
  setUp() {
    var runTest = this.deferRunTest(WhenTestDone.EXPECT);

    (async () => {
      await importModule(
          'selectToSpeak', '/select_to_speak/select_to_speak_main.js');
      await importModule(
          'SelectToSpeakConstants',
          '/select_to_speak/select_to_speak_constants.js');
      this.resetStorage();

      runTest();
    })();
  }

  resetStorage() {
    this.mockStorage_.clear();
    selectToSpeak.prefsManager_.initPreferences();
  }

  // This must be done before setting STS rate and pitch for tests to work
  // properly.
  setGlobalRateAndPitch(rate, pitch) {
    const unused = () => {};
    this.mockSettingsPrivate_.setPref(
        'settings.tts.speech_rate', rate, '', unused);
    this.mockSettingsPrivate_.setPref(
        'settings.tts.speech_pitch', pitch, '', unused);
  }

  setStsRateAndPitch(rate, pitch) {
    this.mockStorage_.sync.set({rate});
    this.mockStorage_.sync.set({pitch});
  }

  ensurePrefsRemovedAndGlobalSetTo(rate, pitch) {
    const onPrefsRemovedFromStorage = this.newCallback(() => {
      // Once prefs are removed from storage, make sure the global prefs are
      // updated to the appropriate values.
      this.mockSettingsPrivate_.getPref(
          'settings.tts.speech_rate', this.newCallback((pref) => {
            assertEquals(rate, pref.value);
          }));
      this.mockSettingsPrivate_.getPref(
          'settings.tts.speech_pitch', this.newCallback((pref) => {
            assertEquals(pitch, pref.value);
          }));
    });
    this.mockStorage_.onChanged.addListener((prefs) => {
      // checks that rate and pitch are removed.
      if (prefs !== undefined && !('rate' in prefs) && !('pitch' in prefs)) {
        onPrefsRemovedFromStorage();
      }
    });
  }
};

// TODO(katie): Test no alert -- this is hard because it happens last.
TEST_F(
    'SelectToSpeakPrefsTest', 'RemovesPrefsWithNoAlertIfAllDefault',
    function() {
      this.setGlobalRateAndPitch(1.0, 1.0);
      this.setStsRateAndPitch(1.0, 1.0);
      this.mockStorage_.updatePrefs();

      this.ensurePrefsRemovedAndGlobalSetTo(1.0, 1.0);
    });

// TODO(katie): Test no alert -- this is hard because it happens last.
TEST_F(
    'SelectToSpeakPrefsTest', 'RemovesPrefsWithNoAlertIfAllEqual', function() {
      this.setGlobalRateAndPitch(1.5, 1.8);
      this.setStsRateAndPitch(1.5, 1.8);
      this.mockStorage_.updatePrefs();

      this.ensurePrefsRemovedAndGlobalSetTo(1.5, 1.8);
    });

TEST_F('SelectToSpeakPrefsTest', 'SavesNonDefaultStsPrefsToGlobal', function() {
  this.setGlobalRateAndPitch(1.0, 1.0);
  this.setStsRateAndPitch(2.0, 2.5);
  this.mockStorage_.updatePrefs();

  this.ensurePrefsRemovedAndGlobalSetTo(2.0, 2.5);
});

TEST_F(
    'SelectToSpeakPrefsTest',
    'DoesNotSaveNonDefaultStsPrefsToGlobalIfGlobalChanged', function() {
      this.setGlobalRateAndPitch(1.0, 1.5);
      this.setStsRateAndPitch(1.0, 2.5);
      this.mockStorage_.updatePrefs();

      this.ensurePrefsRemovedAndGlobalSetTo(1.0, 1.5);
    });

TEST_F(
    'SelectToSpeakPrefsTest', 'DoesNotSaveStsPrefsToGlobalIfGlobalChanged',
    function() {
      this.setGlobalRateAndPitch(2.0, 1.0);
      this.setStsRateAndPitch(1.0, 1.0);
      this.mockStorage_.updatePrefs();

      this.ensurePrefsRemovedAndGlobalSetTo(2.0, 1.0);
    });
