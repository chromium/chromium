// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['testing/common_e2e_test_base.js']);
GEN_INCLUDE(['testing/mock_storage.js']);

/** Test fixture for local_storage.js. */
AccessibilityExtensionLocalStorageTest = class extends CommonE2ETestBase {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    await importModule('LocalStorage', '/common/local_storage.js');

    chrome = chrome || {};
    chrome.storage = chrome.storage || MockStorage;
  }
};

AX_TEST_F(
    'AccessibilityExtensionLocalStorageTest', 'Migration', async function() {
      localStorage['catSound'] = 'meow';
      localStorage['catIsShy'] = String(false);
      localStorage['catIsFluffy'] = String(true);
      localStorage['pawCount'] = String(4);
      localStorage['catLengthInMeters'] = String(.5);
      const catToys = [['toy_mouse', 'toy_bird'], ['red_car', 'blue_car']];
      localStorage['catToys'] = JSON.stringify(catToys);
      const cat = {
        sound: 'meow',
        isFluffy: true,
        pawCount: 4,
        lengthInMeters: .5,
        toys: ['mouse', 'car'],
        children: {},
      };
      localStorage['cat'] = JSON.stringify(cat);

      // Creating more than once instance of LocalStorage causes an error.
      delete LocalStorage.instance;
      await LocalStorage.init();

      // Check that values were migrated.
      assertEquals(localStorage['catSound'], undefined);
      assertEquals(typeof (LocalStorage.get('catSound')), 'string');
      assertEquals(LocalStorage.get('catSound'), 'meow');

      // Check that booleans are converted properly.
      assertEquals(localStorage['catIsShy'], undefined);
      assertEquals(typeof (LocalStorage.get('catIsShy')), 'boolean');
      assertFalse(LocalStorage.get('catIsShy'));

      assertEquals(localStorage['catIsFluffy'], undefined);
      assertEquals(typeof (LocalStorage.get('catIsFluffy')), 'boolean');
      assertTrue(LocalStorage.get('catIsFluffy'));

      // Check that integers are converted properly.
      assertEquals(localStorage['pawCount'], undefined);
      assertEquals(typeof (LocalStorage.get('pawCount')), 'number');
      assertEquals(LocalStorage.get('pawCount'), 4);

      // Check that floating point numbers are converted properly.
      assertEquals(localStorage['catLengthInMeters'], undefined);
      assertEquals(typeof (LocalStorage.get('catLengthInMeters')), 'number');
      assertEquals(LocalStorage.get('catLengthInMeters'), 0.5);

      // Check that arrays are parsed correctly, including nested arrays.
      assertEquals(localStorage['catToys'], undefined);
      assertEquals(typeof (LocalStorage.get('catToys')), 'object');
      assertTrue(LocalStorage.get('catToys') instanceof Array);
      assertDeepEquals(LocalStorage.get('catToys'), catToys);

      // Check that objects are parsed correctly, including all types of
      // parameters.
      assertEquals(localStorage['cat'], undefined);
      assertEquals(typeof (LocalStorage.get('cat')), 'object');
      assertDeepEquals(LocalStorage.get('cat'), cat);
    });
