// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {decorate} from 'chrome://resources/js/cr/ui.m.js';
import {TabBox} from 'chrome://resources/js/cr/ui/tabs.m.js';
import {Tree, TreeItem} from 'chrome://resources/js/cr/ui/tree.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {requestInfo, triggerStoragePressure} from './message_dispatcher.js';

/**
 * @param {Object} object Object to be checked.
 * @return {boolean} true if |object| is {}.
 * @private
 */
function isEmptyObject_(object) {
  for (const i in object) {
    return false;
  }
  return true;
}

/**
 * Copy properties from |source| to |destination|.
 * @param {Object} source Source of the copy.
 * @param {Object} destination Destination of the copy.
 * @return {Object} |destination|.
 * @private
 */
function copyAttributes_(source, destination) {
  for (const i in source) {
    destination[i] = source[i];
  }
  return destination;
}

/**
 * Returns 'N/A' (Not Available) text if |value| is undefined.
 * @param {*} value Object to print.
 * @return {string} 'N/A' or ''.
 * @private
 */
function checkIfAvailable_(value) {
  return value === undefined ? 'N/A' : '';
}

/**
 * Returns |value| itself if |value| is not undefined,
 * else returns 'N/A' text.
 * @param {?string} value String to print.
 * @return {string} 'N/A' or |value|.
 * @private
 */
function stringToText_(value) {
  return checkIfAvailable_(value) || /** @type {string} */ (value);
}

/**
 * Separates |value| into segments.
 * The length of first segment is at most |maxLength|.
 * Length of other following segments are just |maxLength|.
 * e.g. separateBackward_('abcdefghijk', 4) == ['abc','defg','hijk'];
 * @param {string} value String to be separated.
 * @param {number} maxLength Max length of segments.
 * @return {Array<string>} Array of segments.
 * @private
 */
function separateBackward_(value, maxLength) {
  const result = [];
  while (value.length > maxLength) {
    result.unshift(value.slice(-3));
    value = value.slice(0, -3);
  }
  result.unshift(value);
  return result;
}

/**
 * Returns formatted string from number as number of bytes.
 * e.g. numBytesToText(123456789) = '123.45 MB (123,456,789 B)'.
 * If |value| is undefined, this function returns 'N/A'.
 * @param {?number} value Number to print.
 * @return {string} 'N/A' or formatted |value|.
 * @private
 */
function numBytesToText_(value) {
  let result = checkIfAvailable_(value);
  if (result) {
    return result;
  }

  const segments = separateBackward_(value.toString(), 3);
  result = segments.join(',') + ' B';

  if (segments.length > 1) {
    const UNIT = [' B', ' KB', ' MB', ' GB', ' TB', ' PB'];
    result = segments[0] + '.' + segments[1].slice(0, 2) +
        UNIT[Math.min(segments.length, UNIT.length) - 1] + ' (' + result + ')';
  }

  return result;
}

/**
 * Return formatted date |value| if |value| is not undefined.
 * If |value| is undefined, this function returns 'N/A'.
 * @param {?number} value Number of milliseconds since
 *   UNIX epoch time (0:00, Jan 1, 1970, UTC).
 * @return {string} Formatted text of date or 'N/A'.
 * @private
 */
function dateToText(value) {
  let result = checkIfAvailable_(value);
  if (result) {
    return result;
  }

  const time = new Date(value);
  const now = new Date();
  const delta = Date.now() - value;

  const SECOND = 1000;
  const MINUTE = 60 * SECOND;
  const HOUR = 60 * MINUTE;
  const DAY = 23 * HOUR;
  const WEEK = 7 * DAY;

  const SHOW_SECOND = 5 * MINUTE;
  const SHOW_MINUTE = 5 * HOUR;
  const SHOW_HOUR = 3 * DAY;
  const SHOW_DAY = 2 * WEEK;
  const SHOW_WEEK = 3 * 30 * DAY;

  if (delta < 0) {
    result = 'access from future ';
  } else if (delta < SHOW_SECOND) {
    result = Math.ceil(delta / SECOND) + ' sec ago ';
  } else if (delta < SHOW_MINUTE) {
    result = Math.ceil(delta / MINUTE) + ' min ago ';
  } else if (delta < SHOW_HOUR) {
    result = Math.ceil(delta / HOUR) + ' hr ago ';
  } else if (delta < SHOW_WEEK) {
    result = Math.ceil(delta / DAY) + ' day ago ';
  }

  result += '(' + time.toString() + ')';
  return result;
}

/**
 * Available disk space.
 * @type {number|undefined}
 */
let availableSpace = undefined;

/**
 * Root of the quota data tree,
 * holding userdata as |treeViewObject.detail|.
 * @type {Tree}
 */
let treeViewObject;

/**
 * Key-value styled statistics data.
 * This WebUI does not touch contents, just show.
 * The value is hold as |statistics[key].detail|.
 * @type {Object<string,Element>}
 */
const statistics = {};

/**
 * Initialize and return |treeViewObject|.
 * @return {!Tree} Initialized |treeViewObject|.
 */
function getTreeViewObject() {
  if (!treeViewObject) {
    treeViewObject = /** @type {!Tree} */ ($('tree-view'));
    decorate(treeViewObject, Tree);
    treeViewObject.detail = {payload: {}, children: {}};
    treeViewObject.addEventListener('change', updateDescription);
  }
  return treeViewObject;
}

/**
 * Initialize and return a tree item, that represents specified storage type.
 * @param {!string} type Storage type.
 * @return {TreeItem} Initialized |storageObject|.
 */
function getStorageObject(type) {
  const treeViewObject = getTreeViewObject();
  let storageObject = treeViewObject.detail.children[type];
  if (!storageObject) {
    storageObject =
        new TreeItem({label: type, detail: {payload: {}, children: {}}});
    storageObject.mayHaveChildren_ = true;
    treeViewObject.detail.children[type] = storageObject;
    treeViewObject.add(storageObject);
  }
  return storageObject;
}

/**
 * Initialize and return a tree item, that represents specified
 *  storage type and hostname.
 * @param {!string} type Storage type.
 * @param {!string} host Hostname.
 * @return {TreeItem} Initialized |hostObject|.
 */
function getHostObject(type, host) {
  const storageObject = getStorageObject(type);
  let hostObject = storageObject.detail.children[host];
  if (!hostObject) {
    hostObject =
        new TreeItem({label: host, detail: {payload: {}, children: {}}});
    hostObject.mayHaveChildren_ = true;
    storageObject.detail.children[host] = hostObject;
    storageObject.add(hostObject);
  }
  return hostObject;
}

/**
 * Initialize and return a tree item, that represents specified
 * storage type, hostname and origin url.
 * @param {!string} type Storage type.
 * @param {!string} host Hostname.
 * @param {!string} origin Origin URL.
 * @return {TreeItem} Initialized |originObject|.
 */
function getOriginObject(type, host, origin) {
  const hostObject = getHostObject(type, host);
  let originObject = hostObject.detail.children[origin];
  if (!originObject) {
    originObject =
        new TreeItem({label: origin, detail: {payload: {}, children: {}}});
    originObject.mayHaveChildren_ = false;
    hostObject.detail.children[origin] = originObject;
    hostObject.add(originObject);
  }
  return originObject;
}

/** @param {number} space Total available disk space. */
function handleAvailableSpace(space) {
  availableSpace = space;
  $('diskspace-entry').textContent = numBytesToText_(availableSpace);
}

/**
 * |data| contains a record which has:
 *   |type|:
 *     Storage type, that is either 'temporary' or 'persistent'.
 *   |usage|:
 *     Total storage usage of all hosts.
 *   |unlimitedUsage|:
 *     Total storage usage of unlimited-quota origins.
 *   |quota|:
 *     Total quota of the storage.
 *
 *  |usage|, |unlimitedUsage| and |quota| can be missing,
 *  and some additional fields can be included.
 * @param {!{
 *     type: string,
 *     usage: ?number,
 *     unlimitedUsage: ?number,
 *     quota: ?string
 * }} data
 */
function handleGlobalInfo(data) {
  const storageObject = getStorageObject(data.type);
  copyAttributes_(data, storageObject.detail.payload);
  storageObject.reveal();
  if (getTreeViewObject().selectedItem == storageObject) {
    updateDescription();
  }
}

/**
 * |dataArray| contains records which have:
 *   |host|:
 *     Hostname of the entry. (e.g. 'example.com')
 *   |type|:
 *     Storage type. 'temporary' or 'persistent'
 *   |usage|:
 *     Total storage usage of the host.
 *   |quota|:
 *     Per-host quota.
 *
 * |usage| and |quota| can be missing,
 * and some additional fields can be included.
 * @param {!Array<{
 *     host: string,
 *     type: string,
 *     usage: ?number,
 *     quota: ?number
 * }>} dataArray
 */
function handlePerHostInfo(dataArray) {
  for (let i = 0; i < dataArray.length; ++i) {
    const data = dataArray[i];
    const hostObject = getHostObject(data.type, data.host);
    copyAttributes_(data, hostObject.detail.payload);
    hostObject.reveal();
    if (getTreeViewObject().selectedItem == hostObject) {
      updateDescription();
    }
  }
}

/**
 * |dataArray| contains records which have:
 *   |origin|:
 *     Origin URL of the entry.
 *   |type|:
 *     Storage type of the entry. 'temporary' or 'persistent'.
 *   |host|:
 *     Hostname of the entry.
 *   |inUse|:
 *     true if the origin is in use.
 *   |usedCount|:
 *     Used count of the storage from the origin.
 *   |lastAccessTime|:
 *     Last storage access time from the origin.
 *     Number of milliseconds since UNIX epoch (Jan 1, 1970, 0:00:00 UTC).
 *   |lastModifiedTime|:
 *     Last modified time of the storage from the origin.
 *     Number of milliseconds since UNIX epoch.
 *
 * |inUse|, |usedCount|, |lastAccessTime| and |lastModifiedTime| can be missing,
 * and some additional fields can be included.
 * @param {!Array<!{
 *     origin: string,
 *     type: string,
 *     host: string,
 *     inUse: ?boolean,
 *     usedCount: ?number,
 *     lastAccessTime: ?number,
 *     lastModifiedTime: ?number
 * }>} dataArray
 */
function handlePerOriginInfo(dataArray) {
  for (let i = 0; i < dataArray.length; ++i) {
    const data = dataArray[i];
    const originObject = getOriginObject(data.type, data.host, data.origin);
    copyAttributes_(data, originObject.detail.payload);
    originObject.reveal();
    if (getTreeViewObject().selectedItem == originObject) {
      updateDescription();
    }
  }
}

/**
 * |data| contains misc statistics data as dictionary.
 * @param {!Object} data
 */
function handleStatistics(data) {
  for (const key in data) {
    let entry = statistics[key];
    if (!entry) {
      const template = document.querySelector('#table-row-template');
      entry = template.content.cloneNode(true).querySelector('tr');
      $('stat-entries').appendChild(entry);
      statistics[key] = entry;
    }
    entry.detail = data[key];

    entry.querySelectorAll('td')[0].textContent = stringToText_(key);
    entry.querySelectorAll('td')[1].textContent = stringToText_(entry.detail);
  }
}

/**
 * @param {!{isStoragePressureEnabled: boolean}} data Contains a boolean
 *     representing whether or not to show the storage pressure UI.
 */
function handleStoragePressureFlagInfo(data) {
  $('storage-pressure-loading').hidden = true;
  if (data.isStoragePressureEnabled) {
    $('storage-pressure-outer').hidden = false;
  } else {
    $('storage-pressure-disabled').hidden = false;
  }
}

/**
 * Update description on 'tree-item-description' field with
 * selected item in tree view.
 */
function updateDescription() {
  const item = getTreeViewObject().selectedItem;
  const tbody = $('tree-item-description');
  tbody.innerHTML = trustedTypes.emptyHTML;

  if (item) {
    const keyAndLabel = [
      ['type', 'Storage Type'], ['host', 'Host Name'], ['origin', 'Origin URL'],
      ['usage', 'Total Storage Usage', numBytesToText_],
      ['unlimitedUsage', 'Usage of Unlimited Origins', numBytesToText_],
      ['quota', 'Quota', numBytesToText_], ['inUse', 'Origin is in use?'],
      ['usedCount', 'Used count'],
      ['lastAccessTime', 'Last Access Time', dateToText],
      ['lastModifiedTime', 'Last Modified Time', dateToText]
    ];
    for (let i = 0; i < keyAndLabel.length; ++i) {
      const key = keyAndLabel[i][0];
      const label = keyAndLabel[i][1];
      const entry = item.detail.payload[key];
      if (entry === undefined) {
        continue;
      }

      const normalize = keyAndLabel[i][2] || stringToText_;

      const template = document.querySelector('#table-row-template');
      const row = template.content.cloneNode(true).querySelector('tr');
      row.querySelectorAll('td')[0].textContent = label;
      row.querySelectorAll('td')[1].textContent = normalize(entry);
      tbody.appendChild(row);
    }
  }
}

/**
 * Dump |treeViewObject| or subtree to a object.
 * @param {(Tree|TreeItem)=} opt_treeitem
 * @return {Object} Dump result object from |treeViewObject|.
 */
function dumpTreeToObj(opt_treeitem) {
  const treeitem = opt_treeitem || getTreeViewObject();
  const res = {};
  res.payload = treeitem.detail.payload;
  res.children = [];
  for (const i in treeitem.detail.children) {
    const child = treeitem.detail.children[i];
    res.children.push(dumpTreeToObj(child));
  }

  if (isEmptyObject_(res.payload)) {
    delete res.payload;
  }

  if (res.children.length == 0) {
    delete res.children;
  }
  return res;
}

/**
 * Dump |statistics| to a object.
 * @return {Object} Dump result object from |statistics|.
 */
function dumpStatisticsToObj() {
  const result = {};
  for (const key in statistics) {
    result[key] = statistics[key].detail;
  }
  return result;
}

/**
 * Event handler for 'dump-button' 'click'ed.
 * Dump and show all data from WebUI page to 'dump-field' element.
 */
function dump() {
  const separator = '========\n';

  $('dump-field').textContent = separator + 'Summary\n' + separator +
      JSON.stringify({availableSpace: availableSpace}, null, 2) + '\n' +
      separator + 'Usage And Quota\n' + separator +
      JSON.stringify(dumpTreeToObj(), null, 2) + '\n' + separator +
      'Misc Statistics\n' + separator +
      JSON.stringify(dumpStatisticsToObj(), null, 2);
}

function onLoad() {
  decorate('tabbox', TabBox);

  addWebUIListener('AvailableSpaceUpdated', handleAvailableSpace);
  addWebUIListener('GlobalInfoUpdated', handleGlobalInfo);
  addWebUIListener('PerHostInfoUpdated', handlePerHostInfo);
  addWebUIListener('PerOriginInfoUpdated', handlePerOriginInfo);
  addWebUIListener('StatisticsUpdated', handleStatistics);
  addWebUIListener('StoragePressureFlagUpdated', handleStoragePressureFlagInfo);

  requestInfo();

  $('refresh-button').addEventListener('click', requestInfo, false);
  $('dump-button').addEventListener('click', dump, false);
  $('trigger-notification').addEventListener('click', () => {
    const origin = $('storage-pressure-origin').value;
    triggerStoragePressure(origin);
  }, false);
}

document.addEventListener('DOMContentLoaded', onLoad, false);
