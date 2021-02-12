// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/action_link.js';

import {addWebUIListener} from 'chrome://resources/js/cr.m.js';
import {$} from 'chrome://resources/js/util.m.js';

class BrowserProxy {
  toggleAccessibility(processId, routingId, modeId, shouldRequestTree) {
    chrome.send('toggleAccessibility', [{
                  processId,
                  routingId,
                  modeId,
                  shouldRequestTree,
                }]);
  }

  requestNativeUITree(sessionId, requestType, allow, allowEmpty, deny) {
    chrome.send('requestNativeUITree', [{
                  sessionId,
                  requestType,
                  filters: {allow, allowEmpty, deny},
                }]);
  }

  requestWebContentsTree(
      processId, routingId, requestType, allow, allowEmpty, deny) {
    chrome.send('requestWebContentsTree', [
      {processId, routingId, requestType, filters: {allow, allowEmpty, deny}}
    ]);
  }

  requestWidgetsTree(widgetId, requestType, allow, allowEmpty, deny) {
    chrome.send(
        'requestWidgetsTree',
        [{widgetId, requestType, filters: {allow, allowEmpty, deny}}]);
  }

  requestAccessibilityEvents(processId, routingId, start) {
    chrome.send('requestAccessibilityEvents', [{processId, routingId, start}]);
  }

  setGlobalFlag(flagName, enabled) {
    chrome.send('setGlobalFlag', [{flagName, enabled}]);
  }
}

const browserProxy = new BrowserProxy();

// Note: keep these values in sync with the values in
// ui/accessibility/ax_mode.h
const AXMode = {
  kNativeAPIs: 1 << 0,
  kWebContents: 1 << 1,
  kInlineTextBoxes: 1 << 2,
  kScreenReader: 1 << 3,
  kHTML: 1 << 4,
  kLabelImages: 1 << 5,
  kPDF: 1 << 6,

  get kAXModeBasic() {
    return AXMode.kNativeAPIs | AXMode.kWebContents;
  },

  get kAXModeWebContentsOnly() {
    return AXMode.kWebContents | AXMode.kInlineTextBoxes |
        AXMode.kScreenReader | AXMode.kHTML;
  },

  get kAXModeComplete() {
    return AXMode.kNativeAPIs | AXMode.kWebContents | AXMode.kInlineTextBoxes |
        AXMode.kScreenReader | AXMode.kHTML;
  }
};

function requestData() {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', 'targets-data.json', false);
  xhr.send(null);
  if (xhr.status === 200) {
    console.log(xhr.responseText);
    return JSON.parse(xhr.responseText);
  }
  return [];
}

function getIdFromData(data) {
  if (data.type == 'page') {
    return data.processId + '.' + data.routingId;
  } else if (data.type == 'browser') {
    return 'browser.' + data.sessionId;
  } else if (data.type == 'widget') {
    return 'widget.' + data.widgetId;
  } else {
    console.error('Unknown data type.', data);
    return '';
  }
}

function toggleAccessibility(data, element, mode, globalStateName) {
  if (!data[globalStateName]) {
    return;
  }

  const id = getIdFromData(data);
  const tree = $(id + ':tree');
  // If the tree is visible, request a new tree with the updated mode.
  const shouldRequestTree = !!tree && tree.style.display != 'none';
  browserProxy.toggleAccessibility(
      data.processId, data.routingId, mode, shouldRequestTree);
}

function requestTree(data, element) {
  const allow = $('filter-allow').value;
  const allowEmpty = $('filter-allow-empty').value;
  const deny = $('filter-deny').value;
  window.localStorage['chrome-accessibility-filter-allow'] = allow;
  window.localStorage['chrome-accessibility-filter-allow-empty'] = allowEmpty;
  window.localStorage['chrome-accessibility-filter-deny'] = deny;

  // The calling |element| is a button with an id of the format
  // <treeId>:<requestType>, where requestType is one of 'showOrRefreshTree',
  // 'copyTree'. Send the request type to C++ so is calls the corresponding
  // function with the result.
  const requestType = element.id.split(':')[1];
  if (data.type == 'browser') {
    const delay = $('native-ui-delay').value;
    setTimeout(() => {
      browserProxy.requestNativeUITree(
          data.sessionId, requestType, allow, allowEmpty, deny);
    }, delay);
  } else if (data.type == 'widget') {
    browserProxy.requestWidgetsTree(
        data.widgetId, requestType, allow, allowEmpty, deny);
  } else {
    browserProxy.requestWebContentsTree(
        data.processId, data.routingId, requestType, allow, allowEmpty, deny);
  }
}

function requestEvents(data, element) {
  const start = element.textContent == 'Start recording';
  if (start) {
    element.textContent = 'Stop recording';
    element.setAttribute('aria-expanded', 'true');

    // Disable all other start recording buttons. UI reflects the fact that
    // there can only be one accessibility recorder at once.
    const buttons = document.getElementsByClassName('recordEventsButton');
    for (const button of buttons) {
      if (button != element) {
        button.disabled = true;
      }
    }
  } else {
    element.textContent = 'Start recording';
    element.setAttribute('aria-expanded', 'false');

    // Enable all start recording buttons.
    const buttons = document.getElementsByClassName('recordEventsButton');
    for (const button of buttons) {
      if (button != element) {
        button.disabled = false;
      }
    }
  }
  browserProxy.requestAccessibilityEvents(
      data.processId, data.routingId, start);
}

function initialize() {
  console.log('initialize');
  const data = requestData();

  bindCheckbox('native', data['native']);
  bindCheckbox('web', data['web']);
  bindCheckbox('text', data['text']);
  bindCheckbox('screenreader', data['screenreader']);
  bindCheckbox('html', data['html']);
  bindCheckbox('label_images', data['labelImages']);
  bindCheckbox('internal', data['internal']);

  $('pages').textContent = '';

  const pages = data['pages'];
  for (let i = 0; i < pages.length; i++) {
    addToPagesList(pages[i]);
  }

  const browsers = data['browsers'];
  for (let i = 0; i < browsers.length; i++) {
    addToBrowsersList(browsers[i]);
  }

  if (data['viewsAccessibility']) {
    const widgets = data['widgets'];

    if (widgets.length === 0) {
      // There should always be at least 1 Widget displayed (for the current
      // window). If this is not the case, and Views Accessibility is enabled,
      // the only possibility is that Views Accessibility is not enabled for
      // the current platform. Display a message to the user to indicate this.
      $('widgets-not-supported').style.display = 'block';
    } else {
      for (let i = 0; i < widgets.length; i++) {
        addToWidgetsList(widgets[i]);
      }
    }
  } else {
    $('widgets').style.display = 'none';
    $('widgets-header').style.display = 'none';
  }

  // Cache filters so they're easily accessible on page refresh.
  const allow = window.localStorage['chrome-accessibility-filter-allow'];
  const allowEmpty =
      window.localStorage['chrome-accessibility-filter-allow-empty'];
  const deny = window.localStorage['chrome-accessibility-filter-deny'];
  $('filter-allow').value = allow ? allow : '*';
  $('filter-allow-empty').value = allowEmpty ? allowEmpty : '';
  $('filter-deny').value = deny ? deny : '';

  addWebUIListener('copyTree', copyTree);
  addWebUIListener('showOrRefreshTree', showOrRefreshTree);
  addWebUIListener('startOrStopEvents', startOrStopEvents);
}

function bindCheckbox(name, value) {
  if (value == 'on') {
    $(name).checked = true;
  }
  if (value == 'disabled') {
    $(name).disabled = true;
    $(name).labels[0].classList.add('disabled');
  }
  $(name).addEventListener('change', function() {
    browserProxy.setGlobalFlag(name, $(name).checked);
    document.location.reload();
  });
}

function addToPagesList(data) {
  // TODO: iterate through data and pages rows instead
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  row.processId = data.processId;
  row.routingId = data.routingId;

  const pages = $('pages');
  pages.appendChild(row);
}

function addToBrowsersList(data) {
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  const browsers = $('browsers');
  browsers.appendChild(row);
}

function addToWidgetsList(data) {
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  const widgets = $('widgets');
  widgets.appendChild(row);
}

function formatRow(row, data, requestType) {
  if (!('url' in data)) {
    if ('error' in data) {
      row.appendChild(createErrorMessageElement(data));
      return;
    }
  }

  if (data.type == 'page') {
    const siteInfo = document.createElement('div');
    const properties = ['faviconUrl', 'name', 'url'];
    for (let j = 0; j < properties.length; j++) {
      siteInfo.appendChild(formatValue(data, properties[j]));
    }
    row.appendChild(siteInfo);

    row.appendChild(createModeElement(AXMode.kNativeAPIs, data, 'native'));
    row.appendChild(createModeElement(AXMode.kWebContents, data, 'native'));
    row.appendChild(createModeElement(AXMode.kInlineTextBoxes, data, 'web'));
    row.appendChild(createModeElement(AXMode.kScreenReader, data, 'web'));
    row.appendChild(createModeElement(AXMode.kHTML, data, 'web'));
    row.appendChild(
        createModeElement(AXMode.kLabelImages, data, 'labelImages'));
    row.appendChild(createModeElement(AXMode.kPDF, data, 'pdf'));
  } else {
    const siteInfo = document.createElement('span');
    siteInfo.appendChild(formatValue(data, 'name'));
    row.appendChild(siteInfo);
  }

  row.appendChild(document.createTextNode(' | '));

  const hasTree = 'tree' in data;
  row.appendChild(
      createShowAccessibilityTreeElement(data, row.id, requestType, hasTree));
  if (navigator.clipboard) {
    row.appendChild(createCopyAccessibilityTreeElement(data, row.id));
  }
  if (hasTree) {
    row.appendChild(createHideAccessibilityTreeElement(row.id));
  }
  // The accessibility event recorder currently only works for pages.
  // TODO(abigailbklein): Add event recording for native as well.
  if (data.type == 'page') {
    row.appendChild(
        createStartStopAccessibilityEventRecordingElement(data, row.id));
  }

  if (hasTree) {
    row.appendChild(createAccessibilityOutputElement(data, row.id, 'tree'));
  } else if ('eventLogs' in data) {
    row.appendChild(
        createAccessibilityOutputElement(data, row.id, 'eventLogs'));
  } else if ('error' in data) {
    row.appendChild(createErrorMessageElement(data));
  }
}

function insertHeadingInline(parentElement, headingText, id) {
  const h3 = document.createElement('h3');
  h3.textContent = headingText;
  h3.style.display = 'inline';
  h3.id = id + ':title';
  parentElement.appendChild(h3);
}

function formatValue(data, property) {
  const value = data[property];

  if (property == 'faviconUrl') {
    const faviconElement = document.createElement('img');
    if (value) {
      faviconElement.src = value;
    }
    faviconElement.alt = '';
    return faviconElement;
  }

  let text = value ? String(value) : '';
  if (text.length > 100) {
    text = text.substring(0, 100) + '\u2026';
  }  // ellipsis

  const span = document.createElement('span');
  const content = ' ' + text + ' ';
  if (property == 'name') {
    const id = getIdFromData(data);
    insertHeadingInline(span, content, id);
  } else {
    span.textContent = content;
  }
  span.className = property;
  return span;
}

function getNameForAccessibilityMode(mode) {
  switch (mode) {
    case AXMode.kNativeAPIs:
      return 'Native';
    case AXMode.kWebContents:
      return 'Web';
    case AXMode.kInlineTextBoxes:
      return 'Inline text';
    case AXMode.kScreenReader:
      return 'Screen reader';
    case AXMode.kHTML:
      return 'HTML';
    case AXMode.kLabelImages:
      return 'Label images';
    case AXMode.kPDF:
      return 'PDF';
  }
  return 'unknown';
}

function createModeElement(mode, data, globalStateName) {
  const currentMode = data['a11yMode'];
  const link = document.createElement('a', {is: 'action-link'});
  link.setAttribute('is', 'action-link');
  link.setAttribute('role', 'button');

  const stateText = ((currentMode & mode) != 0) ? 'true' : 'false';
  const isEnabled = data[globalStateName];
  if (isEnabled) {
    link.textContent = getNameForAccessibilityMode(mode) + ': ' + stateText;
  } else {
    link.textContent = getNameForAccessibilityMode(mode) + ': disabled';
    link.classList.add('disabled');
  }
  link.setAttribute('aria-pressed', stateText);
  link.addEventListener(
      'click',
      toggleAccessibility.bind(this, data, link, mode, globalStateName));
  return link;
}

function createShowAccessibilityTreeElement(
    data, id, requestType, opt_refresh) {
  const show = document.createElement('button');
  if (requestType == 'showOrRefreshTree') {
    // Give feedback that the tree has loaded.
    show.textContent = 'Accessibility tree loaded';
    setTimeout(() => {
      show.textContent = 'Refresh accessibility tree';
    }, 5000);
  } else {
    show.textContent =
        opt_refresh ? 'Refresh accessibility tree' : 'Show accessibility tree';
  }
  show.id = id + ':showOrRefreshTree';
  show.setAttribute('aria-expanded', String(opt_refresh));
  show.addEventListener('click', requestTree.bind(this, data, show));
  return show;
}

function createHideAccessibilityTreeElement(id) {
  const hide = document.createElement('button');
  hide.textContent = 'Hide accessibility tree';
  hide.id = id + ':hideTree';
  hide.addEventListener('click', function() {
    const show = $(id + ':showOrRefreshTree');
    show.textContent = 'Show accessibility tree';
    show.setAttribute('aria-expanded', 'false');
    show.focus();
    const elements = ['hideTree', 'tree'];
    for (let i = 0; i < elements.length; i++) {
      const elt = $(id + ':' + elements[i]);
      if (elt) {
        elt.style.display = 'none';
      }
    }
  });
  return hide;
}

function createCopyAccessibilityTreeElement(data, id) {
  const copy = document.createElement('button');
  copy.textContent = 'Copy accessibility tree';
  copy.id = id + ':copyTree';
  copy.addEventListener('click', requestTree.bind(this, data, copy));
  return copy;
}

function createStartStopAccessibilityEventRecordingElement(data, id) {
  const show = document.createElement('button');
  show.classList.add('recordEventsButton');
  show.textContent = 'Start recording';
  show.id = id + ':startOrStopEvents';
  show.setAttribute('aria-expanded', 'false');
  show.addEventListener('click', requestEvents.bind(this, data, show));
  return show;
}

function createErrorMessageElement(data) {
  const errorMessageElement = document.createElement('div');
  const errorMessage = data.error;
  const nbsp = '\u00a0';
  errorMessageElement.textContent = errorMessage + nbsp;
  const closeLink = document.createElement('a');
  closeLink.href = '#';
  closeLink.textContent = '[close]';
  closeLink.addEventListener('click', function() {
    const parentElement = errorMessageElement.parentElement;
    parentElement.removeChild(errorMessageElement);
    if (parentElement.childElementCount == 0) {
      parentElement.parentElement.removeChild(parentElement);
    }
  });
  errorMessageElement.appendChild(closeLink);
  return errorMessageElement;
}

// WebUI listener handler for the 'showOrRefreshTree' event.
function showOrRefreshTree(data) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }

  row.textContent = '';
  formatRow(row, data, 'showOrRefreshTree');
  $(id + ':showOrRefreshTree').focus();
}

// WebUI listener handler for the 'startOrStopEvents' event.
function startOrStopEvents(data) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }

  row.textContent = '';
  formatRow(row, data, null);
  $(id + ':startOrStopEvents').focus();
}

// WebUI listener handler for the 'copyTree' event.
function copyTree(data) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }
  const copy = $(id + ':copyTree');

  if ('tree' in data) {
    navigator.clipboard.writeText(data['tree'])
        .then(() => {
          copy.textContent = 'Copied to clipboard!';
          setTimeout(() => {
            copy.textContent = 'Copy accessibility tree';
          }, 5000);
        })
        .catch(err => {
          console.error('Unable to copy accessibility tree.', err);
        });
  } else if ('error' in data) {
    console.error('Unable to copy accessibility tree.', data.error);
  }

  const tree = $(id + ':tree');
  // If the tree is currently shown, update it since it may have changed.
  if (tree && tree.style.display != 'none') {
    showOrRefreshTree(data);
    $(id + ':copyTree').focus();
  }
}

function createNativeUITreeElement(browser) {
  const id = 'browser.' + browser.id;
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, browser, null);
  return row;
}

// type is either 'tree' or 'eventLogs'
function createAccessibilityOutputElement(data, id, type) {
  let treeElement = $(id + ':' + type);
  if (!treeElement) {
    treeElement = document.createElement('pre');
    treeElement.id = id + ':' + type;
  }
  const dataSplitByLine = data[type].split(/\n/);
  for (let i = 0; i < dataSplitByLine.length; i++) {
    const lineElement = document.createElement('div');
    lineElement.textContent = dataSplitByLine[i];
    treeElement.appendChild(lineElement);
  }
  return treeElement;
}

document.addEventListener('DOMContentLoaded', initialize);
