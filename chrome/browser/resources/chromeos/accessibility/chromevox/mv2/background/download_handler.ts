// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Listens for download events and provides corresponding
 * notifications in ChromeVox.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {Msgs} from '../common/msgs.js';
import {SettingsManager} from '../common/settings_manager.js';
import {QueueMode} from '../common/tts_types.js';

import {Output} from './output/output.js';

type DownloadDelta = chrome.downloads.DownloadDelta;
type DownloadItem = chrome.downloads.DownloadItem;
import DownloadState = chrome.downloads.State;

interface DownloadInfo {
  fileName: string;
  notifyProgressId: number;
  time: number;
  percentComplete: number;
}

/** Handles all download events and notifications for ChromeVox. */
export class DownloadHandler {
  /**
   * Maps download item ID to an object containing its file name and progress
   * update function.
   */
  private downloadItemData_: Record<number, DownloadInfo> = {};

  static instance: DownloadHandler;

  /**
   * Performs initialization. Populates downloadItemData_ object and registers
   * event listener for chrome.downloads.onChanged events.
   */
  static init() {
    DownloadHandler.instance = new DownloadHandler();

    // Populate downloadItemData_.
    // Retrieve 20 most recent downloads sorted by most recent start time.
    chrome.downloads.search(
        {orderBy: ['-startTime'], limit: FILE_LIMIT},
        (results: DownloadItem[]) =>
            DownloadHandler.instance.populateDownloadItemData_(results));

    // Note: No event listener for chrome.downloads.onCreated because
    // onCreated does not actually correspond to when the download starts;
    // it corresponds to when the user clicks the download button, which
    // sometimes leads to a screen where the user can decide where to save the
    // download.

    // Fired when any of a DownloadItem's properties, except bytesReceived and
    // estimatedEndTime, change. Only contains properties that changed.
    chrome.downloads.onChanged.addListener(
        (item: DownloadDelta) => DownloadHandler.instance.onChanged_(item));
  }

  /**
   * Notifies user of download progress for file.
   * @param id The ID of the file we are providing an update for.
   */
  private notifyProgress_(id: number): void {
    chrome.downloads.search(
        {id},
        (results: DownloadItem[]) => this.notifyProgressResults_(results));
  }

  notifyProgressResults_(results: DownloadItem[]) {
    if (results?.length !== 1) {
      return;
    }
    // Results should have only one item because IDs are unique.
    const updatedItem = results[0];
    const storedItem = this.downloadItemData_[updatedItem.id];

    const percentComplete =
        Math.round((updatedItem.bytesReceived / updatedItem.totalBytes) * 100);
    const percentDelta = percentComplete - storedItem.percentComplete;
    // Convert time delta from milliseconds to seconds.
    const timeDelta = Math.round((Date.now() - storedItem.time) / 1000);

    // Calculate notification score for this download.
    // This equation was determined by targeting 30 seconds and 50% complete
    // as reasonable milestones before giving an update.
    const score = percentDelta + (5 / 3) * timeDelta;
    // Only report downloads that have scores above the threshold value.
    if (score > UPDATE_THRESHOLD) {
      // Update state.
      storedItem.time = Date.now();
      storedItem.percentComplete = percentComplete;

      // Determine time remaining and units.
      if (!updatedItem.estimatedEndTime) {
        return;
      }
      const endTime = new Date(updatedItem.estimatedEndTime);
      let timeRemaining = Math.round((endTime.getTime() - Date.now()) / 1000);
      let timeUnit = '';

      if (!timeRemaining || (timeRemaining < 0)) {
        return;
      } else if (timeRemaining < 60) {
        // Seconds. Use up until 1 minute remaining.
        timeUnit = Msgs.getMsgWithCount('seconds', timeRemaining);
      } else if (timeRemaining < 3600) {
        // Minutes. Use up until 1 hour remaining.
        timeRemaining = Math.floor(timeRemaining / 60);
        timeUnit = Msgs.getMsgWithCount('minutes', timeRemaining);
      } else if (timeRemaining < 36000) {
        // Hours. Use up until 10 hours remaining.
        timeRemaining = Math.floor(timeRemaining / 3600);
        timeUnit = Msgs.getMsgWithCount('hours', timeRemaining);
      } else {
        // If 10+ hours remaining, do not report progress.
        return;
      }

      const optSubs: string[] = [
        storedItem.percentComplete.toString(),
        storedItem.fileName,
        timeRemaining.toString(),
        timeUnit.toString(),
      ];
      this.speechAndBrailleOutput_(
          'download_progress', QueueMode.FLUSH, optSubs);
    }
  }

  private onChanged_(delta: DownloadDelta): void {
    // The type of notification ChromeVox reports can be inferred based on the
    // available properties, as they have been observed to be mutually
    // exclusive.
    const name = delta.filename;
    const state = delta.state;
    const paused = delta.paused;
    // The ID is always set no matter what.
    const id = delta.id;

    const storedItem = this.downloadItemData_[id];

    // New download if we're not tracking the item and if the filename was
    // previously empty.
    if (!storedItem && name?.previous === '') {
      this.startTrackingDownloadDelta_(delta);

      // Speech and braille output.
      const optSub = this.downloadItemData_[id].fileName;
      this.speechAndBrailleOutput_(
          'download_started', QueueMode.FLUSH, [optSub]);
    } else if (state) {
      const currentState = state.current;
      let msgId = '';
      // Only give notification for COMPLETE and INTERRUPTED.
      // IN_PROGRESS notifications are given by notifyProgress function.
      if (currentState === DownloadState.COMPLETE) {
        msgId = 'download_completed';
      } else if (currentState === DownloadState.INTERRUPTED) {
        msgId = 'download_stopped';
      } else {
        return;
      }

      const optSubs = [storedItem.fileName];
      clearInterval(storedItem.notifyProgressId);
      delete this.downloadItemData_[id];
      // Speech and braille output.
      this.speechAndBrailleOutput_(msgId, QueueMode.FLUSH, optSubs);
    } else if (paused) {
      // Will be either resumed or paused.
      let msgId = 'download_resumed';
      const optSubs = [storedItem.fileName];
      if (paused.current === true) {
        // Download paused.
        msgId = 'download_paused';
        clearInterval(storedItem.notifyProgressId);
      } else {
        // Download resumed.
        storedItem.notifyProgressId = setInterval(
            () => this.notifyProgress_(id), INTERVAL_TIME_MILLISECONDS);
        storedItem.time = Date.now();
      }
      // Speech and braille output.
      this.speechAndBrailleOutput_(msgId, QueueMode.FLUSH, optSubs);
    }
  }

  private populateDownloadItemData_(results: DownloadItem[]): void {
    if (!results || results.length === 0) {
      return;
    }

    for (const item of results) {
      // If download is in progress, start tracking it.
      if (item.state === DownloadState.IN_PROGRESS) {
        this.startTrackingDownloadItem_(item);
      }
    }
  }

  /**
   * Output download notification as speech and braille.
   * @param msgId The msgId for Output.
   * @param queueMode The queue mode.
   * @param optSubs Substitution strings.
   */
  private speechAndBrailleOutput_(
      msgId: string, queueMode: QueueMode, optSubs: string[]): void {
    if (SettingsManager.get('announceDownloadNotifications')) {
      const msg = Msgs.getMsg(msgId, optSubs);
      new Output().withString(msg).withQueueMode(queueMode).go();
    }
  }


  /**
   * Store item data.
   * @param item The download item to track.
   */
  private startTrackingDownloadItem_(item: DownloadItem): void {
    const id = item.id;
    // Don't add if we are already tracking file.
    if (this.downloadItemData_[id]) {
      return;
    }

    const fullPath = item.filename;
    const fileName = fullPath.substring(fullPath.lastIndexOf('/') + 1);
    const notifyProgressId =
        setInterval(() => this.notifyProgress_(id), INTERVAL_TIME_MILLISECONDS);
    let percentComplete = 0;
    if (item.bytesReceived && item.totalBytes) {
      percentComplete =
          Math.round((item.bytesReceived / item.totalBytes) * 100);
    }

    this.downloadItemData_[id] =
        {fileName, notifyProgressId, time: Date.now(), percentComplete};
  }

  /**
   * Store item data.
   * @param item The download item to track.
   */
  private startTrackingDownloadDelta_(item: DownloadDelta) {
    const id = item.id;
    // Don't add if we are already tracking file.
    if (this.downloadItemData_[id]) {
      return;
    }

    const fullPath = item.filename?.current;
    const fileName =
        fullPath ? fullPath.substring(fullPath.lastIndexOf('/') + 1) : '';
    const notifyProgressId =
        setInterval(() => this.notifyProgress_(id), INTERVAL_TIME_MILLISECONDS);
    this.downloadItemData_[id] =
        {fileName, notifyProgressId, time: Date.now(), percentComplete: 0};
  }
}

// Local to module.

/**
 * Threshold value used when determining whether to report an update to user.
 */
const UPDATE_THRESHOLD = 100;

/** The limit for the number of download results to receive when querying. */
const FILE_LIMIT = 20;

/** The time interval, in milliseconds, for calling notifyProgress. */
const INTERVAL_TIME_MILLISECONDS = 10000;

TestImportManager.exportForTesting(DownloadHandler);
