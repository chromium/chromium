// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';
import {sendWithPromise} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {$} from 'chrome://resources/js/util.m.js';

/**
 * Requests the list of WebRTC logs from the backend.
 */
function requestWebRtcLogsList() {
  sendWithPromise('requestWebRtcLogsList').then(updateWebRtcLogsList);
}

/**
 * Callback from backend with the list of WebRTC logs. Builds the UI.
 * @param {!{textLogs: !Array, eventLogs: !Array, version: string}} results
 */
function updateWebRtcLogsList({textLogs, eventLogs, version}) {
  updateWebRtcTextLogsList(textLogs, version);
  updateWebRtcEventLogsList(eventLogs);
}

function updateWebRtcTextLogsList(textLogsList, version) {
  $('text-log-banner').textContent =
      loadTimeData.getStringF('webrtcTextLogCountFormat', textLogsList.length);

  const textLogSection = $('text-log-list');

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
    if (textLog['local_file'].length == 0) {
      localFileLine.textContent =
          loadTimeData.getString('noLocalLogFileMessage');
    } else {
      localFileLine.textContent =
          loadTimeData.getString('webrtcLogLocalFileLabelFormat') + ' ';
      const localFileLink = document.createElement('a');
      localFileLink.href = 'file://' + textLog['local_file'];
      localFileLink.textContent = textLog['local_file'];
      localFileLine.appendChild(localFileLink);
    }
    logBlock.appendChild(localFileLine);

    const uploadLine = document.createElement('p');
    if (textLog['id'].length == 0) {
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
        'Operating System: e.g., "Windows 7", "Mac OSX 10.6"', '',
        'URL (if applicable) where the problem occurred:', '',
        'Can you reproduce this problem?', '',
        'What steps will reproduce this problem? (or if it\'s not ' +
            'reproducible, what were you doing just before the problem)?',
        '', '1.', '2.', '3.', '',
        '*Please note that issues filed with no information filled in ' +
            'above will be marked as WontFix*',
        '', '****DO NOT CHANGE BELOW THIS LINE****', 'report_id:' + textLog.id
      ];
      const params = {
        template: 'Defect report from user',
        comment: commentLines.join('\n'),
      };
      let href = 'http://code.google.com/p/chromium/issues/entry';
      for (const param in params) {
        href = appendParam(href, param, params[param]);
      }
      link.href = href;
      link.target = '_blank';
      link.textContent = loadTimeData.getString('bugLinkText');
      uploadLine.appendChild(link);
    }
    logBlock.appendChild(uploadLine);

    textLogSection.appendChild(logBlock);
  }

  $('text-no-logs').hidden = (textLogsList.length != 0);
}

function updateWebRtcEventLogsList(eventLogsList) {
  const eventLogSection = $('event-log-list');

  eventLogSection.textContent = '';  // Clear any previous list.

  let entries = 0;

  for (let i = 0; i < eventLogsList.length; i++) {
    const entry = createEventLogEntryElement(eventLogsList[i]);
    if (entry) {
      eventLogSection.appendChild(entry);
      entries += 1;
    }
  }

  $('event-log-banner').textContent =
      loadTimeData.getStringF('webrtcEventLogCountFormat', entries);

  $('event-no-logs').hidden = (entries != 0);
}

function createEventLogEntryElement(eventLogEntry) {
  // See LogHistory in webrtc_event_log_manager_remote.cc for an explanation
  // of the various states.
  const state = eventLogEntry['state'];
  if (!state) {
    console.error('Unknown state.');
    return;
  } else if (state == 'pending' || state == 'actively_uploaded') {
    return createPendingOrActivelyUploadedEventLogEntryElement(eventLogEntry);
  } else if (state == 'not_uploaded') {
    return createNotUploadedEventLogEntryElement(eventLogEntry);
  } else if (state == 'upload_unsuccessful') {
    return createUploadUnsuccessfulEventLogEntryElement(eventLogEntry);
  } else if (state == 'upload_successful') {
    return createUploadSuccessfulEventLogEntryElement(eventLogEntry);
  } else {
    console.error('Unrecognized state.');
  }
}

function createPendingOrActivelyUploadedEventLogEntryElement(eventLogEntry) {
  const expectedFields = ['capture_time', 'local_file'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return;
  }

  const logBlock = document.createElement('div');

  appendCaptureTime(logBlock, eventLogEntry);
  appendLocalFile(logBlock, eventLogEntry);

  const uploadLine = document.createElement('p');
  if (eventLogEntry['state'] == 'pending') {
    uploadLine.textContent = loadTimeData.getString('webrtcLogPendingMessage');
  } else {
    uploadLine.textContent =
        loadTimeData.getString('webrtcLogActivelyUploadedMessage');
  }
  logBlock.appendChild(uploadLine);

  return logBlock;
}

function createNotUploadedEventLogEntryElement(eventLogEntry) {
  const expectedFields = ['capture_time', 'local_id'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return;
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

function createUploadUnsuccessfulEventLogEntryElement(eventLogEntry) {
  const expectedFields = ['capture_time', 'local_id', 'upload_time'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return;
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

function createUploadSuccessfulEventLogEntryElement(eventLogEntry) {
  const expectedFields =
      ['capture_time', 'local_id', 'upload_id', 'upload_time'];
  if (!verifyExpectedFields(eventLogEntry, expectedFields)) {
    return;
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

function verifyExpectedFields(entry, expectedFields) {
  for (const fieldIdx in expectedFields) {
    const field = expectedFields[fieldIdx];
    if (!entry[field]) {
      console.error('|' + field + '| expected.');
      return false;
    }
  }
  return true;
}

function appendCaptureTime(logBlock, eventLogEntry) {
  const title = document.createElement('h3');
  title.textContent = loadTimeData.getStringF(
      'webrtcLogHeaderFormat', eventLogEntry['capture_time']);
  logBlock.appendChild(title);
}

function appendLocalFile(logBlock, eventLogEntry) {
  // Local file on disk, if still on disk.
  const localFileLine = document.createElement('p');
  localFileLine.textContent =
      loadTimeData.getString('webrtcLogLocalFileLabelFormat') + ' ';
  const localFileLink = document.createElement('a');
  localFileLink.href = 'file://' + eventLogEntry['local_file'];
  localFileLink.textContent = eventLogEntry['local_file'];
  localFileLine.appendChild(localFileLink);
  logBlock.appendChild(localFileLine);
}

function appendLocalLogId(logBlock, eventLogEntry) {
  const localIdLine = document.createElement('p');
  localIdLine.textContent =
      loadTimeData.getStringF(
          'webrtcEventLogLocalLogIdFormat', eventLogEntry['local_id']) +
      '';
  logBlock.appendChild(localIdLine);
}

document.addEventListener('DOMContentLoaded', requestWebRtcLogsList);
