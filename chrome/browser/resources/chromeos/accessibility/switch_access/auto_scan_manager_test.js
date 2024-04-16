// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

UNDEFINED_INTERVAL_DELAY = -1;

/** Test fixture for auto scan manager. */
SwitchAccessAutoScanManagerTest = class extends SwitchAccessE2ETest {
  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    AutoScanManager.instance.primaryScanTime_ = 1000;
    // Use intervalCount and intervalDelay to check how many intervals are
    // currently running (should be no more than 1) and the current delay.
    globalThis.intervalCount = 0;
    globalThis.intervalDelay = UNDEFINED_INTERVAL_DELAY;
    globalThis.defaultSetInterval = setInterval;
    globalThis.defaultClearInterval = clearInterval;
    this.defaultMoveForward =
        Navigator.byItem.moveForward.bind(Navigator.byItem);
    this.moveForwardCount = 0;

    setInterval = function(func, delay) {
      globalThis.intervalCount++;
      globalThis.intervalDelay = delay;

      // Override the delay for testing.
      return globalThis.defaultSetInterval(func, 0);
    };

    clearInterval = function(intervalId) {
      if (intervalId) {
        globalThis.intervalCount--;
      }
      globalThis.defaultClearInterval(intervalId);
    };

    Navigator.byItem.moveForward = () => {
      this.moveForwardCount++;
      this.onMoveForward_ && this.onMoveForward_();
      this.defaultMoveForward();
    };

    this.onMoveForward_ = null;
  }
};

// https://crbug.com/1452024: Flaky on linux-chromeos-rel/linux-chromeos-dbg
TEST_F('SwitchAccessAutoScanManagerTest', 'DISABLED_SetEnabled', function() {
  this.runWithLoadedDesktop(() => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        0, this.moveForwardCount,
        'Incorrect initialization of moveForwardCount');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    this.onMoveForward_ = this.newCallback(() => {
      assertTrue(
          AutoScanManager.instance.isRunning_(),
          'Auto scan manager has stopped running');
      assertGT(this.moveForwardCount, 0, 'Switch Access has not moved forward');
      assertEquals(
          1, intervalCount, 'The number of intervals is no longer exactly 1');
    });

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');
  });
});

// https://crbug.com/1408940: Flaky on linux-chromeos-dbg
GEN('#ifndef NDEBUG');
GEN('#define MAYBE_SetEnabledMultiple DISABLED_SetEnabledMultiple');
GEN('#else');
GEN('#define MAYBE_SetEnabledMultiple SetEnabledMultiple');
GEN('#endif');
TEST_F(
    'SwitchAccessAutoScanManagerTest', 'MAYBE_SetEnabledMultiple', function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running prematurely');
        assertEquals(
            0, intervalCount, 'Incorrect initialization of intervalCount');

        AutoScanManager.setEnabled(true);
        AutoScanManager.setEnabled(true);
        AutoScanManager.setEnabled(true);

        assertTrue(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is not running');
        assertEquals(1, intervalCount, 'There is not exactly 1 interval');
      });
    });

// TODO(crbug.com/40888769): Test is flaky.
TEST_F(
    'SwitchAccessAutoScanManagerTest', 'DISABLED_EnableAndDisable', function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running prematurely');
        assertEquals(
            0, intervalCount, 'Incorrect initialization of intervalCount');

        AutoScanManager.setEnabled(true);
        assertTrue(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is not running');
        assertEquals(1, intervalCount, 'There is not exactly 1 interval');

        AutoScanManager.setEnabled(false);
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager did not stop running');
        assertEquals(0, intervalCount, 'Interval was not removed');
      });
    });

// https://crbug.com/1408940: Flaky on linux-chromeos-dbg
GEN('#ifndef NDEBUG');
GEN('#define MAYBE_RestartIfRunningMultiple DISABLED_RestartIfRunningMultiple');
GEN('#else');
GEN('#define MAYBE_RestartIfRunningMultiple RestartIfRunningMultiple');
GEN('#endif');

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'MAYBE_RestartIfRunningMultiple',
    function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running prematurely');
        assertEquals(
            0, this.moveForwardCount,
            'Incorrect initialization of moveForwardCount');
        assertEquals(
            0, intervalCount, 'Incorrect initialization of intervalCount');

        AutoScanManager.setEnabled(true);
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();

        assertTrue(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is not running');
        assertEquals(1, intervalCount, 'There is not exactly 1 interval');
      });
    });

// https://crbug.com/1408940: Flaky on linux-chromeos-dbg
GEN('#ifndef NDEBUG');
GEN('#define MAYBE_RestartIfRunningWhenOff DISABLED_RestartIfRunningWhenOff');
GEN('#else');
GEN('#define MAYBE_RestartIfRunningWhenOff RestartIfRunningWhenOff');
GEN('#endif');

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'MAYBE_RestartIfRunningWhenOff',
    function() {
      this.runWithLoadedDesktop(() => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running at start.');
        AutoScanManager.restartIfRunning();
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager enabled by restartIfRunning');
      });
    });

// https://crbug.com/1408940: Flaky on linux-chromeos-dbg
GEN('#ifndef NDEBUG');
GEN('#define MAYBE_SetPrimaryScanTime DISABLED_SetPrimaryScanTime');
GEN('#else');
GEN('#define MAYBE_SetPrimaryScanTime SetPrimaryScanTime');
GEN('#endif');

TEST_F('SwitchAccessAutoScanManagerTest', 'SetPrimaryScanTime', function() {
  this.runWithLoadedDesktop(() => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay improperly initialized');

    AutoScanManager.setPrimaryScanTime(2);
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Setting default scan time started auto-scanning');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time set improperly');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay set prematurely');

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(), 'Auto scan did not start');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time has changed');
    assertEquals(2, intervalDelay, 'Interval delay not set');

    AutoScanManager.setPrimaryScanTime(5);
    assertTrue(AutoScanManager.instance.isRunning_(), 'Auto scan stopped');
    assertEquals(
        5, AutoScanManager.instance.primaryScanTime_,
        'Default scan time did not change when set a second time');
    assertEquals(5, intervalDelay, 'Interval delay did not update');
  });
});
