// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Listens for download events and provides corresponding
 * notifications in ChromeVox.
 */

goog.provide('DownloadHandler');

/**
 * Maps download item ID to an object containing its file name and progress
 * update function.
 * @private {Object<number, {fileName: string,
 *                           notifyProgressId: number,
 *                           time: number,
 *                           percentComplete: number}>}
 */
DownloadHandler.downloadItemData_ = {};

/**
 * Threshold value used when determining whether to report an update to user.
 * @const {number}
 * @private
 */
DownloadHandler.UPDATE_THRESHOLD_ = 100;

/**
 * The limit for the number of results we receive when querying for downloads.
 * @const {number}
 * @private
 */
DownloadHandler.FILE_LIMIT_ = 20;

/**
 * The time interval, in milliseconds, for calling
 * DownloadHandler.notifyProgress.
 * @const {number}
 * @private
 */
DownloadHandler.INTERVAL_TIME_MILLISECONDS_ = 10000;

/**
 * Performs initialization. Populates downloadItemData_ object and registers
 * event listener for chrome.downloads.onChanged events.
 */
DownloadHandler.init = function() {
  // Populate downloadItemData_.
  // Retrieve 20 most recent downloads sorted by most recent start time.
  chrome.downloads.search(
      {orderBy: ['-startTime'], limit: DownloadHandler.FILE_LIMIT_},
      function(results) {
        if (!results || results.length == 0) {
          return;
        }

        for (var i = 0; i < results.length; ++i) {
          var item = results[i];
          var state = item.state;
          if (!state) {
            continue;
          }
          // If download is in progress, start tracking it.
          if (state === chrome.downloads.State.IN_PROGRESS) {
            DownloadHandler.startTrackingDownload(item);
          }
        }
      });

  // Note: No event listener for chrome.downloads.onCreated because
  // onCreated does not actually correspond to when the download starts;
  // it corresponds to when the user clicks the download button, which sometimes
  // leads to a screen where the user can decide where to save the download.

  // Fired when any of a DownloadItem's properties, except bytesReceived and
  // estimatedEndTime, change. Only contains properties that changed.
  chrome.downloads.onChanged.addListener(function(item) {
    // The type of notification ChromeVox reports can be inferred based on the
    // available properties, as they have been observed to be mutually
    // exclusive.
    var name = item.filename;
    var state = item.state;
    var paused = item.paused;
    // The item ID is always set no matter what.
    var id = item.id;

    var storedItem = DownloadHandler.downloadItemData_[id];

    // New download if we're not tracking the item and if the filename was
    // previously empty.
    if (!storedItem && name && (name.previous === '')) {
      DownloadHandler.startTrackingDownload(
          /** @type {!chrome.downloads.DownloadItem} */ (item));

      // Speech and braille output.
      var optSubs = [DownloadHandler.downloadItemData_[id].fileName];
      DownloadHandler.speechAndBrailleOutput(
          'download_started', QueueMode.FLUSH, optSubs);
    } else if (state) {
      var currentState = state.current;
      var msgId = '';
      // Only give notification for COMPLETE and INTERRUPTED.
      // IN_PROGRESS notifications are given by notifyProgress function.
      if (currentState === chrome.downloads.State.COMPLETE) {
        msgId = 'download_completed';
      } else if (currentState === chrome.downloads.State.INTERRUPTED) {
        msgId = 'download_stopped';
      } else {
        return;
      }

      var optSubs = [storedItem.fileName];
      clearInterval(storedItem.notifyProgressId);
      delete DownloadHandler.downloadItemData_[id];
      // Speech and braille output.
      DownloadHandler.speechAndBrailleOutput(msgId, QueueMode.FLUSH, optSubs);
    } else if (paused) {
      // Will be either resumed or paused.
      var msgId = 'download_resumed';
      var optSubs = [storedItem.fileName];
      if (paused.current === true) {
        // Download paused.
        msgId = 'download_paused';
        clearInterval(storedItem.notifyProgressId);
      } else {
        // Download resumed.
        storedItem.notifyProgressId = setInterval(
            DownloadHandler.notifyProgress.bind(DownloadHandler, item.id),
            10000);
        storedItem.time = Date.now();
      }
      // Speech and braille output.
      DownloadHandler.speechAndBrailleOutput(msgId, QueueMode.FLUSH, optSubs);
    }
  });
};

/**
 * Notifies user of download progress for file.
 * @param {number} id The ID of the file we are providing an update for.
 */
DownloadHandler.notifyProgress = function(id) {
  chrome.downloads.search({id: id}, function(results) {
    if (!results || (results.length != 1)) {
      return;
    }
    // Results should have only one item because IDs are unique.
    var updatedItem = results[0];
    var storedItem = DownloadHandler.downloadItemData_[updatedItem.id];

    var percentComplete =
        Math.round((updatedItem.bytesReceived / updatedItem.totalBytes) * 100);
    var percentDelta = percentComplete - storedItem.percentComplete;
    // Convert time delta from milliseconds to seconds.
    var timeDelta = Math.round((Date.now() - storedItem.time) / 1000);

    // Calculate notification score for this download.
    // This equation was determined by targeting 30 seconds and 50% complete
    // as reasonable milestones before giving an update.
    var score = percentDelta + (5 / 3) * timeDelta;
    // Only report downloads that have scores above the threshold value.
    if (score > DownloadHandler.UPDATE_THRESHOLD_) {
      // Update state.
      storedItem.time = Date.now();
      storedItem.percentComplete = percentComplete;

      // Determine time remaining and units.
      if (!updatedItem.estimatedEndTime) {
        return;
      }
      var endTime = new Date(updatedItem.estimatedEndTime);
      var timeRemaining = Math.round((endTime.getTime() - Date.now()) / 1000);
      var timeUnit = '';

      if (!timeRemaining || (timeRemaining < 0)) {
        return;
      } else if (timeRemaining < 60) {
        // Seconds. Use up until 1 minute remaining.
        timeUnit = new goog.i18n.MessageFormat(Msgs.getMsg('seconds')).format({
          COUNT: timeRemaining
        });
      } else if (timeRemaining < 3600) {
        // Minutes. Use up until 1 hour remaining.
        timeRemaining = Math.floor(timeRemaining / 60);
        timeUnit = new goog.i18n.MessageFormat(Msgs.getMsg('minutes')).format({
          COUNT: timeRemaining
        });
      } else if (timeRemaining < 36000) {
        // Hours. Use up until 10 hours remaining.
        timeRemaining = Math.floor(timeRemaining / 3600);
        timeUnit = new goog.i18n.MessageFormat(Msgs.getMsg('hours')).format({
          COUNT: timeRemaining
        });
      } else {
        // If 10+ hours remaining, do not report progress.
        return;
      }

      var optSubs = [
        storedItem.percentComplete, storedItem.fileName, timeRemaining, timeUnit
      ];
      DownloadHandler.speechAndBrailleOutput(
          'download_progress', QueueMode.FLUSH, optSubs);
    }
  });
};

/**
 * Store item data.
 * @param{!chrome.downloads.DownloadItem} item The download item we want to
 * track.
 */
DownloadHandler.startTrackingDownload = function(item) {
  var id = item.id;
  // Don't add if we are already tracking file.
  if (DownloadHandler.downloadItemData_[id]) {
    return;
  }

  var fullPath = (item.filename.current || item.filename);
  var fileName = fullPath.substring(fullPath.lastIndexOf('/') + 1);
  var notifyProgressId = setInterval(
      DownloadHandler.notifyProgress.bind(DownloadHandler, id),
      DownloadHandler.INTERVAL_TIME_MILLISECONDS_);
  var percentComplete = 0;
  if (item.bytesReceived && item.totalBytes) {
    percentComplete = Math.round((item.bytesReceived / item.totalBytes) * 100);
  }

  DownloadHandler.downloadItemData_[id] = {
    fileName: fileName,
    notifyProgressId: notifyProgressId,
    time: Date.now(),
    percentComplete: percentComplete
  };
};

/**
 * Output download notification as speech and braille.
 * @param{string} msgId The msgId for Output.
 * @param{QueueMode} queueMode The queue mode.
 * @param{Array<string>} optSubs Substitution strings.
 */
DownloadHandler.speechAndBrailleOutput = function(msgId, queueMode, optSubs) {
  if (localStorage['announceDownloadNotifications'] == 'true') {
    var msg = Msgs.getMsg(msgId, optSubs);
    new Output().withString(msg).withQueueMode(queueMode).go();
  }
};
