// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/jstemplate_compiled.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {$} from 'chrome://resources/js/util_ts.js';
import {loadTestModule} from './test_loader_util.js';

/**
 * Local variable where we maintain a count of the invalidations received
 * and of every ObjectId that has ever been updated (note that this doesn't
 * log any invalidations occurred prior to opening the about:invalidation
 * page).
 */
const tableObjects = {};

/**
 * Local variable that contains the detailed information in an object form.
 * This was done this way as to allow multiple calls to updateDetailedStatus
 * to keep adding new items.
 */
let cachedDetails = {};

function quote(str) {
  return '\"' + str + '\"';
}

function nowTimeString() {
  return '[' + new Date().getTime() + '] ';
}

/**
 * Appends a string to a textarea log.
 * @param {string} logMessage The string to be appended.
 */
function appendToLog(logMessage) {
  const invalidationsLog = $('invalidations-log');
  invalidationsLog.value += logMessage + '\n';
}
/**
 *  Updates the jstemplate with the latest ObjectIds, ordered by registrar.
 */
function repaintTable() {
  const keys = [];
  for (const key in tableObjects) {
    keys.push(key);
  }
  keys.sort();
  const sortedInvalidations = [];
  for (let i = 0; i < keys.length; i++) {
    sortedInvalidations.push(tableObjects[keys[i]]);
  }
  const wrapped = {objectsidtable: sortedInvalidations};
  jstProcess(new JsEvalContext(wrapped), $('objectsid-table-div'));
}

/**
 * Shows the current state of the InvalidatorService.
 * @param {string} newState The string to be displayed and logged.
 * @param {number} lastChangedTime The time in epoch when the state was last
 *     changed.
 */
function updateInvalidatorState(newState, lastChangedTime) {
  const logMessage = nowTimeString() +
      'Invalidations service state changed to ' + quote(newState);

  appendToLog(logMessage);
  $('invalidations-state').textContent =
      newState + ' (since ' + new Date(lastChangedTime) + ')';
}

/**
 * Adds to the log the latest invalidations received
 * @param {!Array<!Object>} allInvalidations The array of ObjectId
 *     that contains the invalidations received by the InvalidatorService.
 */
function logInvalidations(allInvalidations) {
  for (let i = 0; i < allInvalidations.length; i++) {
    const inv = allInvalidations[i];
    if (inv.hasOwnProperty('objectId')) {
      const logMessage = nowTimeString() + 'Received Invalidation with type ' +
          quote(inv.objectId.name) + ' version ' +
          quote((inv.isUnknownVersion ? 'Unknown' : inv.version)) +
          ' with payload ' + quote(inv.payload);

      appendToLog(logMessage);
      const isInvalidation = true;
      logToTable(inv, isInvalidation);
    }
  }
  repaintTable();
}

/**
 * Marks a change in the table whether a new invalidation has arrived
 * or a new ObjectId is currently being added or updated.
 * @param {!Object} oId The ObjectId being added or updated.
 * @param {!boolean} isInvaldation A flag that says that an invalidation
 *     for this ObjectId has arrived or we just need to add it to the table
 *     as it was just updated its state.
 */
function logToTable(oId, isInvalidation) {
  const registrar = oId.registrar;
  const name = oId.objectId.name;
  const source = oId.objectId.source;
  const totalCount = oId.objectId.totalCount || 0;
  const key = source + '-' + name;
  const time = new Date();
  const version = oId.isUnknownVersion ? '?' : oId.version;
  let payload = '';
  if (oId.hasOwnProperty('payload')) {
    payload = oId.payload;
  }
  if (!(key in tableObjects)) {
    tableObjects[key] = {
      name: name,
      source: source,
      totalCount: totalCount,
      sessionCount: 0,
      registrar: registrar,
      time: '',
      version: '',
      payload: '',
      type: 'content',
    };
  }
  // Refresh the type to be a content because it might have been
  // greyed out.
  tableObjects[key].type = 'content';
  if (isInvalidation) {
    tableObjects[key].totalCount = tableObjects[key].totalCount + 1;
    tableObjects[key].sessionCount = tableObjects[key].sessionCount + 1;
    tableObjects[key].time = time.toTimeString();
    tableObjects[key].version = version;
    tableObjects[key].payload = payload;
  }
}

/**
 * Shows the handlers that are currently registered for invalidations
 * (but might not have objects ids registered yet).
 * @param {!Array<string>} allHandlers An array of Strings that are
 *     the names of all the handlers currently registered in the
 *     InvalidatorService.
 */
function updateHandlers(allHandlers) {
  const allHandlersFormatted = allHandlers.join(', ');
  $('registered-handlers').textContent = allHandlersFormatted;
  const logMessage = nowTimeString() +
      'InvalidatorHandlers currently registered: ' + allHandlersFormatted;
  appendToLog(logMessage);
}

/**
 * Updates the table with the objects ids registered for invalidations
 * @param {string} registrar The name of the owner of the InvalidationHandler
 *     that is registered for invalidations
 * @param {Array of Object} allIds An array of ObjectsIds that are currently
 *     registered for invalidations. It is not differential (as in, whatever
 *     is not registered now but was before, it mean it was taken out the
 *     registered objects)
 */
function updateIds(registrar, allIds) {
  // Grey out every datatype assigned to this registrar
  // (and reenable them later in case they are still registered).
  for (const key in tableObjects) {
    if (tableObjects[key]['registrar'] === registrar) {
      tableObjects[key].type = 'greyed';
    }
  }
  // Reenable those ObjectsIds still registered with this registrar.
  for (let i = 0; i < allIds.length; i++) {
    const oId = {objectId: allIds[i], registrar: registrar};
    const isInvalidation = false;
    logToTable(oId, isInvalidation);
  }
  repaintTable();
}

/**
 * Update the internal status display, merging new detailed information.
 * @param {!Object} newDetails The dictionary containing assorted debugging
 *      details (e.g. Network Channel information).
 */
function updateDetailedStatus(newDetails) {
  for (const key in newDetails) {
    cachedDetails[key] = newDetails[key];
  }
  $('internal-display').value = JSON.stringify(cachedDetails, null, 2);
}

/**
 * Function that notifies the InvalidationsMessageHandler that the UI is
 * ready to receive real-time notifications.
 */
function onLoadWork() {
  addWebUiListener('handlers-updated', handlers => updateHandlers(handlers));
  addWebUiListener(
      'state-updated',
      (state, lastChanged) => updateInvalidatorState(state, lastChanged));
  addWebUiListener(
      'ids-updated', (registrar, ids) => updateIds(registrar, ids));
  addWebUiListener(
      'log-invalidations', invalidations => logInvalidations(invalidations));
  addWebUiListener(
      'detailed-status-updated',
      networkDetails => updateDetailedStatus(networkDetails));
  $('request-detailed-status').onclick = function() {
    cachedDetails = {};
    chrome.send('requestDetailedStatus');
  };
  if (loadTestModule()) {
    return;
  }
  chrome.send('doneLoading');
}

document.addEventListener('DOMContentLoaded', onLoadWork);
