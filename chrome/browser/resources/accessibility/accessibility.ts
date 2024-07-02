// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/js/action_link.js';

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {$, getRequiredElement} from 'chrome://resources/js/util.js';

// Note: keep these values in sync with the values in
// ui/accessibility/ax_mode.h
enum AxMode {
  NATIVE_APIS = 1 << 0,
  WEB_CONTENTS = 1 << 1,
  INLINE_TEXT_BOXES = 1 << 2,
  SCREEN_READER = 1 << 3,
  HTML = 1 << 4,
  HTML_METADATA = 1 << 5,
  LABEL_IMAGES = 1 << 6,
  PDF_PRINTING = 1 << 7,
  PDF_OCR = 1 << 8,
  ANNOTATE_MAIN_NODE = 1 << 9,
}

interface Data {
  type: 'browser'|'page'|'widget';
}

type BrowserData = Data&{
  name: string,
  sessionId: number,
};

type PageData = Data&{
  a11yMode: AxMode,
  faviconUrl: string,
  name: string,
  pid: number,
  processId: number,
  routingId: number,
  url?: string,

  // Used for GlobalStateName.
  // Note: Does 'metadata' actually exist? Does not appear anywhere in
  // chrome/browser/accessibility/accessibility_ui.cc.
  metadata: boolean,
  native: boolean,
  pdfPrinting: boolean,
  screenreader: boolean,
  web: boolean,

  tree?: string,
  error?: string,
  eventLogs?: string,
};

type WidgetData = Data&{
  name: string,
  widgetId: number,
};

type EnabledStatus = 'disabled'|'off'|'on';

interface InitData {
  browsers: BrowserData[];
  pages: PageData[];
  viewsAccessibility: boolean;
  widgets: WidgetData[];

  supportedApiTypes: string[];
  apiType: string;
  locked: EnabledStatus;

  html: EnabledStatus;
  native: EnabledStatus;
  pdfPrinting: EnabledStatus;
  screenreader: EnabledStatus;
  text: EnabledStatus;
  web: EnabledStatus;
}

type RequestType = 'showOrRefreshTree';

type GlobalStateName = 'native'|'web'|'metadata'|'pdfPrinting'|'screenreader';

class BrowserProxy {
  toggleAccessibility(
      processId: number, routingId: number, modeId: AxMode,
      shouldRequestTree: boolean) {
    chrome.send('toggleAccessibility', [{
                  processId,
                  routingId,
                  modeId,
                  shouldRequestTree,
                }]);
  }

  requestNativeUiTree(
      sessionId: number, requestType: RequestType, allow: string,
      allowEmpty: string, deny: string) {
    chrome.send('requestNativeUITree', [{
                  sessionId,
                  requestType,
                  filters: {allow, allowEmpty, deny},
                }]);
  }

  requestWebContentsTree(
      processId: number, routingId: number, requestType: RequestType,
      allow: string, allowEmpty: string, deny: string) {
    chrome.send('requestWebContentsTree', [
      {processId, routingId, requestType, filters: {allow, allowEmpty, deny}},
    ]);
  }

  requestWidgetsTree(
      widgetId: number, requestType: RequestType, allow: string,
      allowEmpty: string, deny: string) {
    chrome.send(
        'requestWidgetsTree',
        [{widgetId, requestType, filters: {allow, allowEmpty, deny}}]);
  }

  requestAccessibilityEvents(
      processId: number, routingId: number, start: boolean) {
    chrome.send('requestAccessibilityEvents', [{processId, routingId, start}]);
  }

  setGlobalFlag(flagName: string, enabled: boolean) {
    chrome.send('setGlobalFlag', [{flagName, enabled}]);
  }

  setGlobalString(stringName: string, value: string) {
    chrome.send('setGlobalString', [{stringName, value}]);
  }
}

const browserProxy = new BrowserProxy();

function requestData(): InitData {
  const xhr = new XMLHttpRequest();
  xhr.open('GET', 'targets-data.json', false);
  xhr.send(null);
  assert(xhr.status === 200);
  return JSON.parse(xhr.responseText);
}

function getIdFromData(data: PageData|BrowserData|WidgetData): string {
  if (data.type === 'page') {
    const pageData = data as PageData;
    return 'page_' + pageData.processId + '_' + pageData.routingId;
  } else if (data.type === 'browser') {
    return 'browser_' + (data as BrowserData).sessionId;
  } else if (data.type === 'widget') {
    return 'widget_' + (data as WidgetData).widgetId;
  } else {
    console.error('Unknown data type.', data);
    return '';
  }
}

function toggleAccessibility(
    data: PageData, mode: AxMode, globalStateName: GlobalStateName) {
  if (!(globalStateName in data)) {
    return;
  }

  const id = getIdFromData(data);
  const tree = $(id + '-tree');
  // If the tree is visible, request a new tree with the updated mode.
  const shouldRequestTree = !!tree && tree.style.display !== 'none';
  browserProxy.toggleAccessibility(
      data.processId, data.routingId, mode, shouldRequestTree);
}

function requestTree(data: BrowserData|PageData|WidgetData, element: Element) {
  const allow = getRequiredElement<HTMLInputElement>('filter-allow').value;
  const allowEmpty =
      getRequiredElement<HTMLInputElement>('filter-allow-empty').value;
  const deny = getRequiredElement<HTMLInputElement>('filter-deny').value;
  window.localStorage['chrome-accessibility-filter-allow'] = allow;
  window.localStorage['chrome-accessibility-filter-allow-empty'] = allowEmpty;
  window.localStorage['chrome-accessibility-filter-deny'] = deny;

  // The calling |element| is a button with an id of the format
  // <treeId>-<requestType>, where requestType is one of 'showOrRefreshTree',
  // 'copyTree'. Send the request type to C++ so is calls the corresponding
  // function with the result.
  const requestType = element.id.split('-')[1] as RequestType;
  if (data.type === 'browser') {
    const delay =
        getRequiredElement<HTMLInputElement>('native-ui-delay').valueAsNumber;
    setTimeout(() => {
      browserProxy.requestNativeUiTree(
          (data as BrowserData).sessionId, requestType, allow, allowEmpty,
          deny);
    }, delay);
  } else if (data.type === 'widget') {
    browserProxy.requestWidgetsTree(
        (data as WidgetData).widgetId, requestType, allow, allowEmpty, deny);
  } else {
    const pageData = data as PageData;
    browserProxy.requestWebContentsTree(
        pageData.processId, pageData.routingId, requestType, allow, allowEmpty,
        deny);
  }
}

function requestEvents(data: PageData, element: HTMLElement) {
  const start = element.textContent === 'Start recording';
  if (start) {
    element.textContent = 'Stop recording';
    element.setAttribute('aria-expanded', 'true');

    // Disable all other start recording buttons. UI reflects the fact that
    // there can only be one accessibility recorder at once.
    const buttons = document.body.querySelectorAll<HTMLButtonElement>(
        '.recordEventsButton');
    for (const button of buttons) {
      if (button !== element) {
        button.disabled = true;
      }
    }
  } else {
    element.textContent = 'Start recording';
    element.setAttribute('aria-expanded', 'false');

    // Enable all start recording buttons.
    const buttons = document.body.querySelectorAll<HTMLButtonElement>(
        '.recordEventsButton');
    for (const button of buttons) {
      if (button !== element) {
        button.disabled = false;
      }
    }
  }
  browserProxy.requestAccessibilityEvents(
      data.processId, data.routingId, start);
}

function initialize() {
  const data = requestData();

  bindCheckbox('native', data.native);
  bindCheckbox('web', data.web);
  bindCheckbox('text', data.text);
  bindCheckbox('screenreader', data.screenreader);
  bindCheckbox('html', data.html);
  bindDropdown('apiType', data.supportedApiTypes, data.apiType);
  bindCheckbox('locked', data.locked);

  getRequiredElement('pages').textContent = '';

  const pages = data.pages;
  for (let i = 0; i < pages.length; i++) {
    addToPagesList(pages[i]!);
  }

  const browsers = data.browsers;
  for (let i = 0; i < browsers.length; i++) {
    addToBrowsersList(browsers[i]!);
  }

  if (data.viewsAccessibility) {
    const widgets = data.widgets;
    if (widgets.length === 0) {
      // There should always be at least 1 Widget displayed (for the current
      // window). If this is not the case, and Views Accessibility is enabled,
      // the only possibility is that Views Accessibility is not enabled for
      // the current platform. Display a message to the user to indicate this.
      getRequiredElement('widgets-not-supported').style.display = 'block';
    } else {
      for (let i = 0; i < widgets.length; i++) {
        addToWidgetsList(widgets[i]!);
      }
    }
  } else {
    getRequiredElement('widgets').style.display = 'none';
    getRequiredElement('widgets-header').style.display = 'none';
  }

  // Cache filters so they're easily accessible on page refresh.
  const allow = window.localStorage['chrome-accessibility-filter-allow'];
  const allowEmpty =
      window.localStorage['chrome-accessibility-filter-allow-empty'];
  const deny = window.localStorage['chrome-accessibility-filter-deny'];
  getRequiredElement<HTMLInputElement>('filter-allow').value =
      allow ? allow : '*';
  getRequiredElement<HTMLInputElement>('filter-allow-empty').value =
      allowEmpty ? allowEmpty : '';
  getRequiredElement<HTMLInputElement>('filter-deny').value = deny ? deny : '';

  addWebUiListener('copyTree', copyTree);
  addWebUiListener('showOrRefreshTree', showOrRefreshTree);
  addWebUiListener('startOrStopEvents', startOrStopEvents);
}

function bindCheckbox(name: string, value: EnabledStatus) {
  const checkbox = getRequiredElement<HTMLInputElement>(name);
  if (value === 'on') {
    checkbox.checked = true;
  }
  if (value === 'disabled') {
    checkbox.disabled = true;
    checkbox.labels![0]!.classList.add('disabled');
  }
  checkbox.addEventListener('change', function() {
    browserProxy.setGlobalFlag(name, checkbox.checked);
    document.location.reload();
  });
}

function bindDropdown(name: string, options: string[], value: string) {
  const dropdown = getRequiredElement<HTMLSelectElement>(name);
  // Remove any existing options.
  dropdown.textContent = '';
  // Add options based on the input array.
  for (const optionName of options) {
    const option = document.createElement('option');
    option.textContent = optionName!;
    dropdown.appendChild(option);
  }
  dropdown.value = value;
  dropdown.addEventListener('change', function() {
    // Make sure that the dropdown value is included in options.
    assert(options.includes(dropdown.value));
    browserProxy.setGlobalString(name, dropdown.value);
    document.location.reload();
  });
}

function addToPagesList(data: PageData) {
  // TODO: iterate through data and pages rows instead
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  const pages = getRequiredElement('pages');
  pages.appendChild(row);
}

function addToBrowsersList(data: BrowserData) {
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  const browsers = getRequiredElement('browsers');
  browsers.appendChild(row);
}

function addToWidgetsList(data: WidgetData) {
  const id = getIdFromData(data);
  const row = document.createElement('div');
  row.className = 'row';
  row.id = id;
  formatRow(row, data, null);

  const widgets = getRequiredElement('widgets');
  widgets.appendChild(row);
}

function formatRow(
    row: HTMLElement, data: BrowserData|PageData|WidgetData,
    requestType: RequestType|null) {
  if (!('url' in data)) {
    if ('error' in data) {
      row.appendChild(createErrorMessageElement(data));
      return;
    }
  }

  if (data.type === 'page') {
    const pageData = data as PageData;
    const siteInfo = document.createElement('div');
    const properties = ['faviconUrl', 'name', 'url'];
    for (let j = 0; j < properties.length; j++) {
      siteInfo.appendChild(formatValue(pageData, properties[j]!));
    }
    row.appendChild(siteInfo);

    row.appendChild(createModeElement(AxMode.NATIVE_APIS, pageData, 'native'));
    row.appendChild(createModeElement(AxMode.WEB_CONTENTS, pageData, 'native'));
    row.appendChild(
        createModeElement(AxMode.INLINE_TEXT_BOXES, pageData, 'web'));
    row.appendChild(createModeElement(AxMode.SCREEN_READER, pageData, 'web'));
    row.appendChild(createModeElement(AxMode.HTML, pageData, 'web'));
    row.appendChild(
        createModeElement(AxMode.HTML_METADATA, pageData, 'metadata'));
    row.appendChild(
        createModeElement(AxMode.PDF_PRINTING, pageData, 'pdfPrinting'));
    row.appendChild(createModeElement(
        AxMode.LABEL_IMAGES, pageData, 'screenreader',
        /*readonly=*/ true));
    row.appendChild(createModeElement(
        AxMode.ANNOTATE_MAIN_NODE, pageData, 'screenreader',
        /* readOnly= */ true));
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
    row.appendChild(createHideAccessibilityTreeElement(row.id, data.name));
  }
  // The accessibility event recorder currently only works for pages.
  // TODO(abigailbklein): Add event recording for native as well.
  if (data.type === 'page') {
    row.appendChild(createStartStopAccessibilityEventRecordingElement(
        data as PageData, row.id));
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

function insertHeadingInline(
    parentElement: HTMLElement, headingText: string, id: string) {
  const h3 = document.createElement('h3');
  h3.textContent = headingText;
  h3.style.display = 'inline';
  h3.id = id + '-title';
  parentElement.appendChild(h3);
}

function formatValue(
    data: BrowserData|PageData|WidgetData, property: string): HTMLElement {
  const value = (data as {[k: string]: any})[property];

  if (property === 'faviconUrl') {
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
  let unescapedText = text;
  if (property === 'name') {
    unescapedText = new DOMParser()
                        .parseFromString(
                            sanitizeInnerHtml(text) as unknown as string,
                            'text/html',
                            )
                        .documentElement.textContent ||
        text;
  }
  const content = ' ' + unescapedText + ' ';
  if (property === 'name') {
    const id = getIdFromData(data);
    insertHeadingInline(span, content, id);
  } else {
    span.textContent = content;
  }
  span.className = property;
  return span;
}

function getNameForAccessibilityMode(mode: AxMode): string {
  switch (mode) {
    case AxMode.NATIVE_APIS:
      return 'Native';
    case AxMode.WEB_CONTENTS:
      return 'Web';
    case AxMode.INLINE_TEXT_BOXES:
      return 'Inline text';
    case AxMode.SCREEN_READER:
      return 'Screen reader';
    case AxMode.HTML:
      return 'HTML';
    case AxMode.HTML_METADATA:
      return 'HTML Metadata';
    case AxMode.LABEL_IMAGES:
      return 'Label images';
    case AxMode.PDF_PRINTING:
      return 'PDF printing';
    case AxMode.PDF_OCR:
      return 'PDF OCR';
    case AxMode.ANNOTATE_MAIN_NODE:
      return 'Annotate main node';
    default:
      assertNotReached();
  }
}

function createModeElement(
    mode: AxMode, data: PageData, globalStateName: GlobalStateName,
    readOnly = false) {
  const currentMode = data.a11yMode;
  const element = readOnly ? document.createElement('span') :
                             document.createElement('a', {is: 'action-link'});
  if (readOnly) {
    element.classList.add('readOnlyMode');
  } else {
    element.setAttribute('is', 'action-link');
  }

  element.role = 'button';

  const stateText = ((currentMode & mode) !== 0) ? 'true' : 'false';
  const isEnabled =
      (data as unknown as {[k: string]: boolean})[globalStateName];
  const accessibilityModeName = getNameForAccessibilityMode(mode);

  element.ariaLabel = `${accessibilityModeName} for ${data.name}`;
  element.ariaPressed = stateText;

  if (isEnabled) {
    element.textContent = accessibilityModeName + ': ' + stateText;
  } else {
    element.textContent = accessibilityModeName + ': disabled';
    element.classList.add('disabled');
    element.ariaDisabled = 'true';
  }
  if (readOnly) {
    element.ariaDisabled = 'true';
  } else {
    element.addEventListener(
        'click', toggleAccessibility.bind(null, data, mode, globalStateName));
  }
  return element;
}

function createShowAccessibilityTreeElement(
    data: BrowserData|PageData|WidgetData, id: string,
    requestType: RequestType|null, refresh: boolean) {
  const show = document.createElement('button');
  if (requestType === 'showOrRefreshTree') {
    // Give feedback that the tree has loaded.
    show.textContent = 'Accessibility tree loaded';
    show.ariaLabel = `Accessibility tree loaded for ${data.name}`;
    setTimeout(() => {
      show.textContent = 'Refresh accessibility tree';
      show.ariaLabel = `Refresh accessibility tree for ${data.name}`;
    }, 5000);
  } else {
    const textContent =
        refresh ? 'Refresh accessibility tree' : 'Show accessibility tree';
    show.textContent = textContent;
    show.ariaLabel = `${textContent} for ${data.name}`;
  }
  show.id = id + '-showOrRefreshTree';
  show.setAttribute('aria-expanded', String(refresh));
  show.addEventListener('click', requestTree.bind(null, data, show));
  return show;
}

function createHideAccessibilityTreeElement(id: string, name: string) {
  const hide = document.createElement('button');
  hide.textContent = 'Hide accessibility tree';
  hide.ariaLabel = `Hide accessibility tree for ${name}`;
  hide.id = id + '-hideTree';
  hide.addEventListener('click', function() {
    const show = getRequiredElement(id + '-showOrRefreshTree');
    show.textContent = 'Show accessibility tree';
    show.ariaLabel = `Show accessibility tree for ${name}`;
    show.setAttribute('aria-expanded', 'false');
    show.focus();
    const elements = ['hideTree', 'tree'];
    for (let i = 0; i < elements.length; i++) {
      const elt = $(id + '-' + elements[i]);
      if (elt) {
        elt.style.display = 'none';
      }
    }
  });
  return hide;
}

function createCopyAccessibilityTreeElement(
    data: BrowserData|PageData|WidgetData, id: string): HTMLElement {
  const copy = document.createElement('button');
  copy.textContent = 'Copy accessibility tree';
  copy.ariaLabel = `Copy accessibility tree for ${data.name}`;
  copy.id = id + '-copyTree';
  copy.addEventListener('click', requestTree.bind(null, data, copy));
  return copy;
}

function createStartStopAccessibilityEventRecordingElement(
    data: PageData, id: string): HTMLElement {
  const show = document.createElement('button');
  show.classList.add('recordEventsButton');
  show.textContent = 'Start recording';
  show.ariaLabel = `Start recording for ${data.name}`;
  show.id = id + '-startOrStopEvents';
  show.setAttribute('aria-expanded', 'false');
  show.addEventListener('click', requestEvents.bind(null, data, show));
  return show;
}

function createErrorMessageElement(data: PageData): HTMLElement {
  const errorMessageElement = document.createElement('div');
  const errorMessage = data.error;
  const nbsp = '\u00a0';
  errorMessageElement.textContent = errorMessage + nbsp;
  const closeLink = document.createElement('a');
  closeLink.href = '#';
  closeLink.textContent = '[close]';
  closeLink.addEventListener('click', function() {
    const parentElement = errorMessageElement.parentElement!;
    parentElement.removeChild(errorMessageElement);
    if (parentElement.childElementCount === 0) {
      parentElement.parentElement!.removeChild(parentElement);
    }
  });
  errorMessageElement.appendChild(closeLink);
  return errorMessageElement;
}

// WebUI listener handler for the 'showOrRefreshTree' event.
function showOrRefreshTree(data: PageData) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }

  row.textContent = '';
  formatRow(row, data, 'showOrRefreshTree');
  getRequiredElement(id + '-showOrRefreshTree').focus();
}

// WebUI listener handler for the 'startOrStopEvents' event.
function startOrStopEvents(data: PageData) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }

  row.textContent = '';
  formatRow(row, data, null);
  getRequiredElement(id + '-startOrStopEvents').focus();
}

// WebUI listener handler for the 'copyTree' event.
function copyTree(data: PageData) {
  const id = getIdFromData(data);
  const row = $(id);
  if (!row) {
    return;
  }
  const copy = $(id + '-copyTree');

  if ('tree' in data) {
    navigator.clipboard.writeText(data.tree!)
        .then(() => {
          assert(copy);
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

  const tree = $(id + '-tree');
  // If the tree is currently shown, update it since it may have changed.
  if (tree && tree.style.display !== 'none') {
    showOrRefreshTree(data);
    getRequiredElement(id + '-copyTree').focus();
  }
}

// type is either 'tree' or 'eventLogs'
function createAccessibilityOutputElement(
    data: BrowserData|PageData|WidgetData, id: string,
    type: 'tree'|'eventLogs'): HTMLElement {
  let treeElement = $(id + '-' + type);
  if (!treeElement) {
    treeElement = document.createElement('pre');
    treeElement.id = id + '-' + type;
  }
  const dataSplitByLine =
      (data as unknown as {[k: string]: string})[type]!.split(/\n/);
  for (let i = 0; i < dataSplitByLine.length; i++) {
    const lineElement = document.createElement('div');
    lineElement.textContent = dataSplitByLine[i]!;
    treeElement.appendChild(lineElement);
  }
  return treeElement;
}

document.addEventListener('DOMContentLoaded', initialize);
