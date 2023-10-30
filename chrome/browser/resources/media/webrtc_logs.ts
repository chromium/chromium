// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {appendParam} from 'chrome://resources/js/util.js';


interface EventLogEntry {
  [key: string]: number|string;
}

/**
 * Requests the list of WebRTC logs from the backend.
 */
function requestWebRtcLogsList() {
  sendWithPromise('requestWebRtcLogsList').then(updateWebRtcLogsList);
}

/**
 * Callback from backend with the list of WebRTC logs. Builds the UI.
 */
function updateWebRtcLogsList(results: {
  textLogs: EventLogEntry[],
  eventLogs: EventLogEntry[],
  version: string,
}) {
  updateWebRtcTextLogsList(results.textLogs, results.version);
  updateWebRtcEventLogsList(results.eventLogs);
}

function updateWebRtcTextLogsList(
    textLogsList: EventLogEntry[], version: string) {
  const banner = document.querySelector<HTMLElement>('#text-log-banner');
  assert(banner);
  banner.textContent =
      loadTimeData.getStringF('webrtcTextLogCountFormat', textLogsList.length);

  const textLogSection = document.querySelector<HTMLElement>('#text-log-list');
  assert(textLogSection);

  // Clear any previous list.
  textLogSection.textContent = '';

  for (let i = 0; i < textLogsList.length; i++) {
    const textLog = textLogsList[i];

    const logBlock = document.createElement('div');

    const title = document.createElement('h3');
    title.textContent = loadTimeData.getStringF(
        'webrtcLogHeaderFormat', textLog['capture_time']);
    logBlock.appendChild(title);

    const localFileLine = document.createElement('p');
    const localFile = textLog['local_file'] as string;
    if (localFile.length === 0) {
      localFileLine.textContent =
          loadTimeData.getString('noLocalLogFileMessage');
    } else {
      localFileLine.textContent =
          loadTimeData.getString('webrtcLogLocalFileLabelFormat') + ' ';
      const localFileLink = document.createElement('a');
      localFileLink.href = 'file://' + localFile;
      localFileLink.textContent = localFile;
      localFileLine.appendChild(localFileLink);
    }
    logBlock.appendChild(localFileLine);

    const uploadLine = document.createElement('p');
    const id = textLog['id'] as string;
    if (id.length === 0) {
      uploadLine.textContent =
          loadTimeData.getString('webrtcLogNotUploadedMessage');
    } else {
      uploadLine.textContent =
          loadTimeData.getStringF(
              'webrtcLogUploadTimeFormat', textLog['upload_time']) +
          '. ' +
          loadTimeData.getStringF('webrtcLogReportIdFormat', textLog['id']) +
          '. ';
      const link = document.createElement('a');
      const commentLines = [
        'Chrome Version: ' + version,
        // TODO(tbreisacher): fill in the OS automatically?
        'Operating System: e.g., "Windows 7", "Mac OSX 10.6"',
        '',
        'URL (if applicable) where the problem occurred:',
        '',
        'Can you reproduce this problem?',
        '',
        'What steps will reproduce this problem? (or if it\'s not ' +
            'reproducible, what were you doing just before the problem)?',
        '',
        '1.',
        '2.',
        '3.',
        '',
        '*Please note that issues filed with no information filled in ' +
            'above will be marked as WontFix*',
        '',
        '****DO NOT CHANGE BELOW THIS LINE****',
        'report_id:' + id,
      ];
      const params = {
        template: 'Defect report from user',
        comment: commentLines.join('\n'),
      };
      let href = 'http://code.google.com/p/chromium/issues/entry';
      for (const param in params) {
        href = appendParam(
            href, param, (params as {[key: string]: string})[param]);
      }
      link.href = href;
      link.target = '_blank';
      link.textContent = loadTimeData.getString('bugLinkText');
      uploadLine.appendChild(link);
    }
    logBlock.appendChild(uploadLine);

    textLogSection.appendChild(logBlock);
  }

  const noLogs = document.querySelector<HTMLElement>('#text-no-logs');
  assert(noLogs);
  noLogs.hidden = (textLogsList.length !== 0);
}

function updateWebRtcEventLogsList(eventLogsList: EventLogEntry[]) {
  const eventLogSection =
      document.querySelector<HTMLElement>('#event-log-list');
  assert(eventLogSection);

  eventLogSection.textContent = '';  // Clear any previous list.

  let entries = 0;

  for (let i = 0; i < eventLogsList.length; i++) {
    const entry = createEventLogEntryElement(eventLogsList[i]);
    if (entry) {
      eventLogSection.appendChild(entry);
      entries += 1;
    }
  }

  const banner = document.querySelector<HTMLElement>('#event-log-banner');
  assert(banner);
  banner.textContent =
      loadTimeData.getStringF('webrtcEventLogCountFormat', entries);

  const noLogs = document.querySelector<HTMLElement>('#event-no-logs');
  assert(noLogs);
  noLogs.hidden = (entries !== 0);
}

function createEventLogEntryElement(eventLogEntry: EventLogEntry): HTMLElement|
    null {
  // See LogHistory in webrtc_event_log_manager_remote.cc for an explanation
  // of the various states.
  const state = eventLogEntry['state'];
  if (!state) {
    console.error('Unknown state.');
    return null;
  } else if (state === 'pending' || state === 'actively_uploaded') {
    return createPendingOrActivelyUploadedEventLogEntryElement(eventLogEntry);
  } else if (state === 'not_uploaded') {
    return createNotUploadedEventLogEntryElement(eventLogEntry);
  } else if (state === 'upload_unsuccessful') {
    return createUploadUnsuccessfulEventLogEntryElement(eventLogEntry);
  } else if (state === 'upload_successful') {
    return createUploadSuccessfulEventLogEntryElement(eventLogEntry);
  } else {
    assertNotReached();
  }
}

function createPendingOrActivelyUploadedEventLogEntryElement(
    eventLogEntry: EventLogEntry): HTMLElement|null {
  const expectedFields = ['capture_time', 'local_file'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return null;
  }

  const logBlock = document.createElement('div');

  appendCaptureTime(logBlock, eventLogEntry);
  appendLocalFile(logBlock, eventLogEntry);

  const uploadLine = document.createElement('p');
  if (eventLogEntry['state'] === 'pending') {
    uploadLine.textContent = loadTimeData.getString('webrtcLogPendingMessage');
  } else {
    uploadLine.textContent =
        loadTimeData.getString('webrtcLogActivelyUploadedMessage');
  }
  logBlock.appendChild(uploadLine);

  return logBlock;
}

function createNotUploadedEventLogEntryElement(eventLogEntry: EventLogEntry):
    HTMLElement|null {
  const expectedFields = ['capture_time', 'local_id'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return null;
  }

  const logBlock = document.createElement('div');

  appendCaptureTime(logBlock, eventLogEntry);
  appendLocalLogId(logBlock, eventLogEntry);

  const uploadLine = document.createElement('p');
  uploadLine.textContent =
      loadTimeData.getString('webrtcLogNotUploadedMessage');
  logBlock.appendChild(uploadLine);

  return logBlock;
}

function createUploadUnsuccessfulEventLogEntryElement(
    eventLogEntry: EventLogEntry): HTMLElement|null {
  const expectedFields = ['capture_time', 'local_id', 'upload_time'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return null;
  }

  const logBlock = document.createElement('div');

  appendCaptureTime(logBlock, eventLogEntry);
  appendLocalLogId(logBlock, eventLogEntry);

  const uploadLine = document.createElement('p');
  uploadLine.textContent = loadTimeData.getStringF(
      'webrtcLogFailedUploadTimeFormat', eventLogEntry['upload_time']);
  logBlock.appendChild(uploadLine);

  return logBlock;
}

function createUploadSuccessfulEventLogEntryElement(
    eventLogEntry: EventLogEntry): HTMLElement|null {
  const expectedFields =
      ['capture_time', 'local_id', 'upload_id', 'upload_time'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return null;
  }

  const logBlock = document.createElement('div');

  appendCaptureTime(logBlock, eventLogEntry);
  appendLocalLogId(logBlock, eventLogEntry);

  const uploadLine = document.createElement('p');
  uploadLine.textContent =
      loadTimeData.getStringF(
          'webrtcLogUploadTimeFormat', eventLogEntry['upload_time']) +
      '. ' +
      loadTimeData.getStringF(
          'webrtcLogReportIdFormat', eventLogEntry['upload_id']) +
      '. ';
  logBlock.appendChild(uploadLine);

  return logBlock;
}

function verifyExpectedFields(
    entry: {[key: string]: string|number}, expectedFields: string[]): boolean {
  for (const fieldIdx in expectedFields) {
    const field = expectedFields[fieldIdx];
    if (!entry[field]) {
      console.error('|' + field + '| expected.');
      return false;
    }
  }
  return true;
}

function appendCaptureTime(
    logBlock: HTMLElement, eventLogEntry: EventLogEntry) {
  const title = document.createElement('h3');
  title.textContent = loadTimeData.getStringF(
      'webrtcLogHeaderFormat', eventLogEntry['capture_time']);
  logBlock.appendChild(title);
}

function appendLocalFile(logBlock: HTMLElement, eventLogEntry: EventLogEntry) {
  // Local file on disk, if still on disk.
  const localFileLine = document.createElement('p');
  localFileLine.textContent =
      loadTimeData.getString('webrtcLogLocalFileLabelFormat') + ' ';
  const localFileLink = document.createElement('a');
  const fileName = eventLogEntry['local_file'] as string;
  localFileLink.href = 'file://' + fileName;
  localFileLink.textContent = fileName;
  localFileLine.appendChild(localFileLink);
  logBlock.appendChild(localFileLine);
}

function appendLocalLogId(logBlock: HTMLElement, eventLogEntry: EventLogEntry) {
  const localIdLine = document.createElement('p');
  localIdLine.textContent =
      loadTimeData.getStringF(
          'webrtcEventLogLocalLogIdFormat', eventLogEntry['local_id']) +
      '';
  logBlock.appendChild(localIdLine);
}

document.addEventListener('DOMContentLoaded', requestWebRtcLogsList);
