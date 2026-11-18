// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../../testing/chromevox_e2e_test_base.js']);

/**
 * Test fixture for BrailleTranslatorManager tests.
 * This is an E2E test because there's no easy way to load a data file in
 * a webui-style test.
 */
ChromeVoxBrailleTranslatorManagerTest = class extends ChromeVoxE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();

    this.liblouis = new FakeLibLouis();
    this.manager = new BrailleTranslatorManager(this.liblouis);
    this.liblouis.translatorManager = this.manager;
    // This is called by an event handler in production, but we don't rely
    // on that for this test.
    this.manager.loadLiblouis_();
  }

  addChangeListener(callback) {
    return this.manager.addChangeListener(callOnce(this.newCallback(callback)));
  }

  /** @override */
  get featureList() {
    let list = super.featureList || {};
    list.enabled = list.enabled || [];
    list.enabled.push('features::kAccessibilityChromeVoxJapaneseBraille');
    return list;
  }
};


/** @extends {LibLouis} */
function FakeLibLouis() {}

FakeLibLouis.prototype = {
  /** @override */
  attachToElement() {},

  /** @override */
  async getTranslator(fileNames) {
    const tables = this.translatorManager.getTablesForTest();
    let result = null;
    if (tables != null) {
      const found = tables.filter(table => table.fileNames === fileNames)[0];
      if (found) {
        result = new FakeTranslator(found);
      }
    }
    return Promise.resolve(result);
  },
};

FakeTranslator = class {
  /**
   * @param {BrailleTable.Table} table
   */
  constructor(table) {
    this.table = table;
  }
};

function createSuccessfulTenjiTranslator() {
  return {
    init() {
      return Promise.resolve(true);
    },
    translate() {},
    backTranslate() {},
    isFakeSuccessfulTenjiTranslator_: true,
  };
}

function callOnce(callback) {
  let called = false;
  return function() {
    if (!called) {
      called = true;
      callback.apply(null, arguments);
    }
  };
}

TEST_F('ChromeVoxBrailleTranslatorManagerTest', 'testInitial', function() {
  assertEquals(null, this.manager.getExpandingTranslator());
  assertEquals(null, this.manager.getDefaultTranslator());
  assertEquals(null, this.manager.getUncontractedTranslator());
  this.addChangeListener(function() {
    assertNotEquals(null, this.manager.getExpandingTranslator());
    assertEquals('en-us-comp6', this.manager.getDefaultTranslator().table.id);
    assertNotEquals(null, this.manager.getUncontractedTranslator());
  });
});

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithoutChange',
    function() {
      this.addChangeListener(function() {
        assertNotEquals(null, this.manager.getExpandingTranslator());
        // This works because the fake liblouis is actually not asynchonous.
        this.manager.addChangeListener(function() {
          assertNotReached('Refresh should not be called without a change.');
        });
        this.manager.refresh(SettingsManager.getString('brailleTable'));
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithChange',
    function() {
      this.addChangeListener(function() {
        assertNotEquals(null, this.manager.getExpandingTranslator());
        this.addChangeListener(function() {
          assertEquals(
              'en-ueb-g2', this.manager.getDefaultTranslator().table.id);
          assertEquals(
              'en-nabcc', this.manager.getUncontractedTranslator().table.id);
        });
        this.manager.refresh('en-ueb-g2');
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithTenjiTableId',
    function() {
      this.addChangeListener(function() {
        this.manager.setTenjiTranslatorFactoryForTest(
            () => createSuccessfulTenjiTranslator());
        this.addChangeListener(function() {
          assertTrue(this.manager.getDefaultTranslator()
                         .isFakeSuccessfulTenjiTranslator_);
          assertNotEquals(null, this.manager.getExpandingTranslator());
          assertEquals(null, this.manager.getUncontractedTranslator());
        });
        this.manager.refresh('ja-tenji');
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testRefreshWithTenjiTableIdAgain',
    function() {
      this.addChangeListener(function() {
        this.manager.setTenjiTranslatorFactoryForTest(
            () => createSuccessfulTenjiTranslator());
        this.addChangeListener(function() {
          // After switching to Tenji, refreshing with the same table id should
          // return early without firing the change listener.
          this.manager.addChangeListener(function() {
            assertNotReached('Refresh should not be called without a change.');
          });
          this.manager.refresh('ja-tenji');
        });
        this.manager.refresh('ja-tenji');
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testRefreshFromTenjiToLiblouisTable', function() {
      this.addChangeListener(function() {
        this.manager.setTenjiTranslatorFactoryForTest(
            () => createSuccessfulTenjiTranslator());
        this.addChangeListener(function() {
          assertTrue(this.manager.getDefaultTranslator()
                         .isFakeSuccessfulTenjiTranslator_);
          this.addChangeListener(function() {
            assertFalse(!!this.manager.getDefaultTranslator()
                              .isFakeSuccessfulTenjiTranslator_);
            assertEquals(
                'en-ueb-g2', this.manager.getDefaultTranslator().table.id);
          });
          this.manager.refresh('en-ueb-g2');
        });
        this.manager.refresh('ja-tenji');
      });
    });

TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testRefreshWithJapaneseLocaleAndNoTableId', function() {
      this.addChangeListener(function() {
        // Simulate a Japanese UI locale so that refresh('') picks
        // TenjiTranslator.
        this.manager.setTenjiTranslatorFactoryForTest(
            () => createSuccessfulTenjiTranslator());
        const origGetMessage = chrome.i18n.getMessage;
        chrome.i18n.getMessage = (key) =>
            key === '@@ui_locale' ? 'ja' : origGetMessage(key);
        this.addChangeListener(function() {
          chrome.i18n.getMessage = origGetMessage;
          assertTrue(this.manager.getDefaultTranslator()
                         .isFakeSuccessfulTenjiTranslator_);
          assertEquals(null, this.manager.getUncontractedTranslator());
          assertNotEquals(null, this.manager.getExpandingTranslator());
        });
        this.manager.refresh('');
      });
    });

/**
 * Returns a fake TenjiTranslator factory whose init() results come from
 * |results| in order (true, false, or an Error to reject with). The number of
 * init() calls is counted in |counter.initCalls|.
 */
function createTenjiTranslatorFactory(results, counter = {
  initCalls: 0
}) {
  return () => ({
           init() {
             const result = results[counter.initCalls++];
             return result instanceof Error ? Promise.reject(result) :
                                              Promise.resolve(result);
           },
           translate() {},
           backTranslate() {},
           isFakeSuccessfulTenjiTranslator_: true,
         });
}

/** Makes the UI locale Japanese until the returned function is called. */
function fakeJapaneseLocale() {
  const origGetMessage = chrome.i18n.getMessage;
  chrome.i18n.getMessage = key =>
      key === '@@ui_locale' ? 'ja' : origGetMessage(key);
  return () => chrome.i18n.getMessage = origGetMessage;
}

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testTenjiInitFailureTurnsBrailleOff', async function() {
      await this.manager.loadTablesForTest();
      this.manager.setTenjiTranslatorFactoryForTest(
          createTenjiTranslatorFactory([false]));
      await this.manager.refresh('ja-tenji');
      // Braille is turned off rather than falling back on ja-kantenji.
      assertEquals(null, this.manager.getDefaultTranslator());
      assertEquals(null, this.manager.getExpandingTranslator());
      assertEquals(null, this.manager.getUncontractedTranslator());
    });

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testTenjiInitExceptionTurnsBrailleOff', async function() {
      await this.manager.loadTablesForTest();
      this.manager.setTenjiTranslatorFactoryForTest(
          createTenjiTranslatorFactory([new Error('Fake init exception')]));
      await this.manager.refresh('ja-tenji');
      assertEquals(null, this.manager.getDefaultTranslator());
      assertEquals(null, this.manager.getExpandingTranslator());
    });

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testTenjiInitFailureIsNotRetried',
    async function() {
      await this.manager.loadTablesForTest();
      const counter = {initCalls: 0};
      this.manager.setTenjiTranslatorFactoryForTest(
          createTenjiTranslatorFactory([false, true], counter));
      await this.manager.refresh('ja-tenji');
      assertEquals(1, counter.initCalls);

      // Refreshing with the same table, as happens on every focus change,
      // doesn't retry.
      await this.manager.refresh('ja-tenji');
      assertEquals(1, counter.initCalls);

      // Neither does coming back from another table, as happens when leaving
      // an email or url field.
      await this.manager.refresh('en-ueb-g2');
      assertEquals('en-ueb-g2', this.manager.getDefaultTranslator().table.id);
      await this.manager.refresh('ja-tenji');
      assertEquals(1, counter.initCalls);
      assertEquals(null, this.manager.getDefaultTranslator());
    });

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testBrailleTableSettingChangeRetriesTenji', async function() {
      await this.manager.loadTablesForTest();
      const counter = {initCalls: 0};
      this.manager.setTenjiTranslatorFactoryForTest(
          createTenjiTranslatorFactory([false, true], counter));
      await this.manager.refresh('ja-tenji');
      assertEquals(null, this.manager.getDefaultTranslator());

      await this.manager.onBrailleTableChanged_('ja-tenji');
      assertEquals(2, counter.initCalls);
      assertTrue(
          this.manager.getDefaultTranslator().isFakeSuccessfulTenjiTranslator_);
    });

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest',
    'testJapaneseLocaleAndNoTableIdIsStable', async function() {
      await this.manager.loadTablesForTest();
      const restoreLocale = fakeJapaneseLocale();
      try {
        this.manager.setTenjiTranslatorFactoryForTest(
            createTenjiTranslatorFactory([true]));
        await this.manager.refresh('');
        assertTrue(this.manager.getDefaultTranslator()
                       .isFakeSuccessfulTenjiTranslator_);

        // Refreshing again with the same unset table must not switch to the
        // ja-kantenji liblouis table, which matches the Japanese locale.
        let changes = 0;
        this.manager.addChangeListener(() => changes++);
        await this.manager.refresh('');
        assertEquals(0, changes);
        assertTrue(this.manager.getDefaultTranslator()
                       .isFakeSuccessfulTenjiTranslator_);
      } finally {
        restoreLocale();
      }
    });

AX_TEST_F(
    'ChromeVoxBrailleTranslatorManagerTest', 'testLatestRefreshWins',
    async function() {
      await this.manager.loadTablesForTest();
      let resolveInit;
      this.manager.setTenjiTranslatorFactoryForTest(
          () => ({
            init() {
              return new Promise(resolve => resolveInit = resolve);
            },
          }));
      const tenjiRefresh = this.manager.refresh('ja-tenji');
      await this.manager.refresh('en-ueb-g2');
      assertEquals('en-ueb-g2', this.manager.getDefaultTranslator().table.id);

      // The older Tenji refresh finishing late doesn't override the newer one.
      resolveInit(true);
      await tenjiRefresh;
      assertEquals('en-ueb-g2', this.manager.getDefaultTranslator().table.id);
    });
