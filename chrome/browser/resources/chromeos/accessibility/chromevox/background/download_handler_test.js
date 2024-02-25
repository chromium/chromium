// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include test fixture.
GEN_INCLUDE(['../testing/chromevox_e2e_test_base.js']);

GEN_INCLUDE(['../testing/fake_objects.js']);

/**
 * Test fixture for Download_Handler.
 */
ChromeVoxDownloadTest = class extends ChromeVoxE2ETest {
  addFakeApi(timeRemainingUnits) {
    // Fake out Chrome Downloads API namespace.
    chrome.downloads = {};
    chrome.downloads.search = (query, callback) => {
      callback([{
        id: query.id,
        fileName: 'test.pdf',
        bytesReceived: 9,
        totalBytes: 10,
        estimatedEndTime: this.getTimeRemaining(timeRemainingUnits),
      }]);
    };
    chrome.downloads.onChanged = new FakeChromeEvent();

    chrome.downloads.State = {
      IN_PROGRESS: 'in_progress',
      COMPLETE: 'complete',
      INTERRUPTED: 'interrupted',
    };
  }

  /** @override */
  async setUpDeferred() {
    await super.setUpDeferred();
    globalThis.simulateEvent = item => this.simulateEvent(item);
  }

  /**
   * Simulates a chrome.downloads.onChanged event with the given parameters.
   */
  simulateEvent(item) {
    return function() {
      const listener = chrome.downloads.onChanged.getListener();
      assertNotEquals(null, listener);
      listener(item);
    };
  }

  getTimeRemaining(units) {
    if (!units) {
      console.error('Must specify time units before calling this function');
    } else if (units === 'second') {
      // 1 second.
      return new Date(new Date().getTime() + 1000).toISOString();
    } else if (units === 'seconds') {
      // 30 seconds.
      return new Date(new Date().getTime() + 30000).toISOString();
    } else if (units === 'minute') {
      // 1 minute.
      return new Date(new Date().getTime() + 60000).toISOString();
    } else if (units === 'minutes') {
      // 30 minutes.
      return new Date(new Date().getTime() + 1800000).toISOString();
    } else if (units === 'hour') {
      // 1 hour.
      return new Date(new Date().getTime() + 3600000).toISOString();
    } else if (units === 'hours') {
      // 3 hours.
      return new Date(new Date().getTime() + 10800000).toISOString();
    } else {
      console.error('Did not specify a valid unit type');
    }
  }
};


TEST_F('ChromeVoxDownloadTest', 'DownloadStartedTest', function() {
  SettingsManager.set('announceDownloadNotifications', true);
  this.addFakeApi('hours');
  const mockFeedback = this.createMockFeedback();
  DownloadHandler.init();
  // Simulate download started.
  mockFeedback.call(simulateEvent({
    id: 1,
    filename: {current: 'test.pdf', previous: ''},
  }));

  mockFeedback.expectSpeech('Download started test.pdf')
      .expectBraille(
          'Download started test.pdf', {startIndex: -1, endIndex: -1})
      .replay();
});

TEST_F('ChromeVoxDownloadTest', 'DownloadCompletedTest', function() {
  SettingsManager.set('announceDownloadNotifications', true);
  this.addFakeApi('hours');
  const mockFeedback = this.createMockFeedback();
  DownloadHandler.init();
  // Simulate download started.
  mockFeedback.call(
      simulateEvent({id: 1, filename: {current: 'test.pdf', previous: ''}}));

  // Simulate download completed.
  mockFeedback.call(simulateEvent({
    id: 1,
    state: {
      current: chrome.downloads.State.COMPLETE,
      previous: chrome.downloads.State.IN_PROGRESS,
    },
  }));

  mockFeedback.expectSpeech('Download started test.pdf')
      .expectSpeech('Download completed test.pdf')
      .expectBraille(
          'Download started test.pdf', {startIndex: -1, endIndex: -1})
      .expectBraille(
          'Download completed test.pdf', {startIndex: -1, endIndex: -1})
      .replay();
});

TEST_F('ChromeVoxDownloadTest', 'DownloadInterruptedTest', function() {
  SettingsManager.set('announceDownloadNotifications', true);
  this.addFakeApi('hours');
  const mockFeedback = this.createMockFeedback();
  DownloadHandler.init();
  // Simulate download started.
  mockFeedback.call(
      simulateEvent({id: 1, filename: {current: 'test.pdf', previous: ''}}));

  // Simulate download interrupted.
  mockFeedback.call(simulateEvent({
    id: 1,
    state: {
      current: chrome.downloads.State.INTERRUPTED,
      previous: chrome.downloads.State.IN_PROGRESS,
    },
  }));
  mockFeedback.expectSpeech('Download started test.pdf')
      .expectSpeech('Download stopped test.pdf')
      .expectBraille(
          'Download started test.pdf', {startIndex: -1, endIndex: -1})
      .expectBraille(
          'Download stopped test.pdf', {startIndex: -1, endIndex: -1})
      .replay();
});

TEST_F('ChromeVoxDownloadTest', 'DownloadPausedTest', function() {
  SettingsManager.set('announceDownloadNotifications', true);
  this.addFakeApi('hours');
  const mockFeedback = this.createMockFeedback();
  DownloadHandler.init();
  // Simulate download started.
  mockFeedback.call(
      simulateEvent({id: 1, filename: {current: 'test.pdf', previous: ''}}));

  // Simulate download paused.
  mockFeedback.call(simulateEvent({
    id: 1,
    paused: {
      current: true,
      previous: false,
    },
  }));
  mockFeedback.expectSpeech('Download started test.pdf')
      .expectSpeech('Download paused test.pdf')
      .expectBraille(
          'Download started test.pdf', {startIndex: -1, endIndex: -1})
      .expectBraille('Download paused test.pdf', {startIndex: -1, endIndex: -1})
      .replay();
});

TEST_F('ChromeVoxDownloadTest', 'DownloadResumedTest', function() {
  SettingsManager.set('announceDownloadNotifications', true);
  this.addFakeApi('hours');
  const mockFeedback = this.createMockFeedback();
  DownloadHandler.init();
  // Simulate download started.
  mockFeedback.call(
      simulateEvent({id: 1, filename: {current: 'test.pdf', previous: ''}}));

  // Simulate download resumed.
  mockFeedback.call(simulateEvent({
    id: 1,
    paused: {
      current: false,
      previous: true,
    },
  }));
  mockFeedback.expectSpeech('Download started test.pdf')
      .expectSpeech('Download resumed test.pdf')
      .expectBraille(
          'Download started test.pdf', {startIndex: -1, endIndex: -1})
      .expectBraille(
          'Download resumed test.pdf', {startIndex: -1, endIndex: -1})
      .replay();
});

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadOneSecondRemainingTest', function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('second');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 1 second remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 1 second remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadMultipleSecondsRemainingTest',
    function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('seconds');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 30 seconds remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 30 seconds remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadOneMinuteRemainingTest', function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('minute');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 1 minute remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 1 minute remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadMultipleMinutesRemainingTest',
    function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('minutes');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 30 minutes remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 30 minutes remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadOneHourRemainingTest', function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('hour');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 1 hour remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 1 hour remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });

TEST_F(
    'ChromeVoxDownloadTest', 'DownloadMultipleHoursRemainingTest', function() {
      SettingsManager.set('announceDownloadNotifications', true);
      this.addFakeApi('hours');
      const mockFeedback = this.createMockFeedback();
      DownloadHandler.init();
      DownloadHandler.intervalTimeMilliseconds = 1000;
      // Simulate download started.
      mockFeedback.call(simulateEvent(
          {id: 1, filename: {current: 'test.pdf', previous: ''}}));

      setTimeout(function() {
        mockFeedback.expectSpeech('Download started test.pdf')
            .expectSpeech(
                'Download 90% complete test.pdf. About 3 hours remaining.')
            .expectBraille(
                'Download started test.pdf', {startIndex: -1, endIndex: -1})
            .expectBraille(
                'Download 90% complete test.pdf. About 3 hours remaining.',
                {startIndex: -1, endIndex: -1})
            .replay();
      }, 2000);
    });
