// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for SettingsManager.
 */
ChromeVoxSettingsManagerTest = class extends ChromeVoxE2ETest {
  async getStoragePrefs(prefNames) {
    const prefs = {};
    for (const prefName of prefNames) {
      prefs[prefName] = LocalStorage.get(prefName);
    }
    return prefs;
  }

  async getSettingsPrefs(prefNames) {
    const prefs = {};
    for (const prefName of prefNames) {
      const {value} = await new Promise(
          resolve => chrome.settingsPrivate.getPref(
              SettingsManager.getPrefName_(prefName), resolve));
      prefs[prefName] = value;
    }
    return prefs;
  }

  async setStoragePrefsAndMigrate(prefs) {
    Object.entries(prefs).forEach(
        ([key, value]) => LocalStorage.set(key, value));

    // Reinitialize and Migrate Prefs.
    Settings.instance = undefined;
    SettingsManager.instance = undefined;
    await SettingsManager.init();
  }

  async ensureStoragePrefsRemoved(prefs) {
    const storagePrefs = await this.getStoragePrefs(Object.keys(prefs));
    assertEqualsJSON(storagePrefs, {}, 'Storage Prefs still present.');
  }

  async ensureSettingsPrefsIncludes(expectedPrefs) {
    const actualPrefs = await this.getSettingsPrefs(Object.keys(expectedPrefs));
    assertEqualsJSON(
        expectedPrefs, actualPrefs,
        'Settings Prefs don\'t match expected results.');
  }
};

// Leaves default storage prefs, runs the migration in prefs_manager, verifies
// that prefs are set to their default values, and verifies that there are still
// no storage prefs. This mimics the state of a fresh user profile.
AX_TEST_F(
    'ChromeVoxSettingsManagerTest',
    'DefaultSettingsPrefsSetAfterNoStoragePrefsSet', async function() {
      const defaultPrefs = {
        announceDownloadNotifications: true,
        announceRichTextAttributes: true,
        audioStrategy: 'audioNormal',
        brailleSideBySide: true,
        brailleTable: '',
        brailleTable6: 'en-ueb-g2',
        brailleTable8: 'en-nabcc',
        brailleTableType: 'brailleTable8',
        brailleWordWrap: true,
        capitalStrategy: 'increasePitch',
        capitalStrategyBackup: '',
        menuBrailleCommands: false,
        enableBrailleLogging: false,
        enableEarconLogging: false,
        enableEventStreamLogging: false,
        enableSpeechLogging: false,
        languageSwitching: false,
        numberReadingStyle: 'asWords',
        punctuationEcho: 1,
        smartStickyMode: true,
        speakTextUnderMouse: false,
        usePitchChanges: true,
        useVerboseMode: true,
        virtualBrailleColumns: 40,
        virtualBrailleRows: 1,
        voiceName: 'chromeos_system_voice',
      };

      // Set no storage prefs.
      await this.setStoragePrefsAndMigrate({});

      // Check all storage prefs migrated to settings prefs.
      await this.ensureSettingsPrefsIncludes(defaultPrefs);

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved(defaultPrefs);
    });

// Sets some user (non-developer) storage prefs, runs the migration in
// SettingsManager, verifies prefs migrated to settings prefs, and verifies that
// storage prefs are removed.
AX_TEST_F(
    'ChromeVoxSettingsManagerTest',
    'PrefsMigratedToSettingsAndDefaultsSetAfterSomeStoragePrefsSet',
    async function() {
      const changedPrefs = {
        audioStrategy: 'audioSuspend',
        brailleSideBySide: false,
        brailleWordWrap: false,
        capitalStrategy: 'announceCapitals',
        capitalStrategyBackup: 'announceCapitals',
        enableEventStreamLogging: true,
        enableSpeechLogging: true,
        languageSwitching: true,
        numberReadingStyle: 'asDigits',
        punctuationEcho: 2,
        smartStickyMode: false,
        speakTextUnderMouse: true,
        usePitchChanges: false,
        useVerboseMode: false,
        virtualBrailleColumns: 29,
        virtualBrailleRows: 3,
        voiceName: 'eSpeak Danish',
      };

      const defaultPrefs = {
        announceDownloadNotifications: true,
        announceRichTextAttributes: true,
        brailleTable: '',
        brailleTable6: 'en-ueb-g2',
        brailleTable8: 'en-nabcc',
        brailleTableType: 'brailleTable8',
        enableBrailleLogging: false,
        enableEarconLogging: false,
        menuBrailleCommands: false,
      };

      const allPrefs = {...changedPrefs, ...defaultPrefs};

      // Set changed storage prefs.
      await this.setStoragePrefsAndMigrate(changedPrefs);

      // Check all storage prefs migrated to settings prefs.
      await this.ensureSettingsPrefsIncludes(allPrefs);

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved(allPrefs);
    });

// Sets all user (non-developer) storage prefs, runs the migration in
// SettingsManager, verifies prefs migrated to settings prefs, and verifies that
// storage prefs are removed.
AX_TEST_F(
    'ChromeVoxSettingsManagerTest',
    'AllPrefsMigratedToSettingsAfterStoragePrefsSet', async function() {
      const prefs = {
        announceDownloadNotifications: false,
        announceRichTextAttributes: false,
        audioStrategy: 'audioSuspend',
        brailleSideBySide: false,
        brailleTable: 'hy',
        brailleTable6: 'hy',
        brailleTable8: 'fi-fi-8dot',
        brailleTableType: 'brailleTable6',
        brailleWordWrap: false,
        capitalStrategy: 'announceCapitals',
        capitalStrategyBackup: 'announceCapitals',
        enableBrailleLogging: true,
        enableEarconLogging: false,
        enableEventStreamLogging: false,
        enableSpeechLogging: true,
        languageSwitching: true,
        menuBrailleCommands: false,
        numberReadingStyle: 'asDigits',
        punctuationEcho: 2,
        smartStickyMode: false,
        speakTextUnderMouse: true,
        usePitchChanges: false,
        useVerboseMode: false,
        virtualBrailleColumns: 29,
        virtualBrailleRows: 3,
        voiceName: 'eSpeak Danish',
      };

      // Set all storage prefs.
      await this.setStoragePrefsAndMigrate(prefs);

      // Check all storage prefs migrated to settings prefs.
      await this.ensureSettingsPrefsIncludes(prefs);

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved(prefs);
    });

// Sets all developer storage prefs, runs the migration in
// SettingsManager, verifies prefs migrated to settings prefs, and verifies that
// storage prefs are removed.
AX_TEST_F(
    'ChromeVoxSettingsManagerTest',
    'AllDeveloperPrefsMigratedToSettingsAfterStoragePrefsSet',
    async function() {
      const loggingPrefs = {
        enableBrailleLogging: true,
        enableEarconLogging: false,
        enableEventStreamLogging: true,
        enableSpeechLogging: false,
      };

      const eventStreamFilters = {
        activedescendantchanged: true,
        alert: true,
        // TODO(crbug.com/1464633) Fully remove ariaAttributeChangedDeprecated
        // starting in 122, because although it was removed in 118, it is still
        // present in earlier versions of LaCros.
        ariaAttributeChangedDeprecated: true,
        autocorrectionOccured: false,
        blur: true,
        checkedStateChanged: false,
        childrenChanged: false,
        clicked: true,
        documentSelectionChanged: true,
        documentTitleChanged: false,
        expandedChanged: false,
        focus: true,
        focusContext: true,
        hide: false,
        hitTestResult: true,
        hover: true,
        imageFrameUpdated: false,
        invalidStatusChanged: true,
        layoutComplete: false,
        liveRegionChanged: false,
        liveRegionCreated: false,
        loadComplete: false,
        locationChanged: true,
        mediaStartedPlaying: true,
        mediaStoppedPlaying: true,
        menuEnd: true,
        menuItemSelected: false,
        menuPopupEnd: false,
        menuPopupStart: true,
        menuStart: true,
        mouseCanceled: true,
        mouseDragged: true,
        mouseMoved: true,
        mousePressed: false,
        mouseReleased: false,
        rowCollapsed: true,
        rowCountChanged: true,
        rowExpanded: false,
        scrollPositionChanged: true,
        scrolledToAnchor: true,
        selectedChildrenChanged: true,
        selection: true,
        selectionAdd: false,
        selectionRemove: false,
        show: true,
        stateChanged: false,
        textChanged: true,
        textSelectionChanged: true,
        treeChanged: true,
        valueInTextFieldChanged: true,
      };

      const storagePrefs = {...loggingPrefs, ...eventStreamFilters};
      const expectedSettingsPrefs = {...loggingPrefs, eventStreamFilters};

      // Set storage prefs.
      await this.setStoragePrefsAndMigrate(storagePrefs);

      // Check storage prefs migrated to settings prefs.
      await this.ensureSettingsPrefsIncludes(expectedSettingsPrefs);

      // Ensure all storage prefs deleted.
      await this.ensureStoragePrefsRemoved(storagePrefs);
    });
