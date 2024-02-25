// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {addWebUiListener} from 'chrome://resources/js/cr.js';

interface WebApkInfo {
  name: string;
  shortName: string;
  packageName: string;
  id: string;
  shellApkVersion: number;
  versionCode: number;
  uri: string;
  scope: string;
  manifestUrl: string;
  manifestStartUrl: string;
  manifestId: string;
  displayMode: string;
  orientation: string;
  themeColor: string;
  backgroundColor: string;
  darkThemeColor: string;
  darkBackgroundColor: string;
  lastUpdateCheckTimeMs: number;
  lastUpdateCompletionTimeMs: number;
  relaxUpdates: boolean;
  backingBrowser: string;
  isBackingBrowser: boolean;
  updateStatus: string;
}

/**
 * Creates and returns an element (with |text| as content) assigning it the
 * |className| class.
 *
 * @param text Text to be shown in the span.
 * @param type Type of element to be added such as 'div'.
 * @param className Class to be assigned to the new element.
 * @return The created element.
 */
function createElementWithTextAndClass(
    text: string, type: string, className: string): HTMLElement {
  const element = document.createElement(type);
  element.className = className;
  element.textContent = text;
  return element;
}

/**
 * @param webApkList List of elements which contain WebAPK attributes.
 * @param label Text that identifies the new element.
 * @param value Text to set in the new element.
 */
function addWebApkField(webApkList: HTMLElement, label: string, value: string) {
  const divElement =
      createElementWithTextAndClass(label, 'div', 'app-property-label');
  divElement.appendChild(
      createElementWithTextAndClass(value, 'span', 'app-property-value'));
  webApkList.appendChild(divElement);
}

/**
 * @param webApkList List of elements which contain WebAPK attributes.
 * @param text For the button.
 * @param callback Invoked on click.
 * @return The button that was created.
 */
function addWebApkButton(
    webApkList: HTMLElement, text: string, callback: () => void): HTMLElement {
  const divElement =
      createElementWithTextAndClass(text, 'button', 'update-button');
  divElement.onclick = callback;
  webApkList.appendChild(divElement);
  return divElement;
}

/**
 * Adds a new entry to the page with the information of a WebAPK.
 *
 * @param webApkInfo Information about an installed WebAPK.
 */
function addWebApk(webApkInfo: WebApkInfo) {
  const webApkList = document.body.querySelector<HTMLElement>('#webapk-list');
  assert(webApkList);

  webApkList.appendChild(
      createElementWithTextAndClass(webApkInfo.name, 'span', 'app-name'));

  webApkList.appendChild(createElementWithTextAndClass(
      'Short name: ', 'span', 'app-property-label'));
  webApkList.appendChild(document.createTextNode(webApkInfo.shortName));

  addWebApkField(webApkList, 'Package name: ', webApkInfo.packageName);
  addWebApkField(
      webApkList, 'Shell APK version: ', '' + webApkInfo.shellApkVersion);
  addWebApkField(webApkList, 'Version code: ', '' + webApkInfo.versionCode);
  addWebApkField(webApkList, 'URI: ', webApkInfo.uri);
  addWebApkField(webApkList, 'Scope: ', webApkInfo.scope);
  addWebApkField(webApkList, 'Manifest URL: ', webApkInfo.manifestUrl);
  addWebApkField(
      webApkList, 'Manifest Start URL: ', webApkInfo.manifestStartUrl);
  addWebApkField(webApkList, 'Manifest Id: ', webApkInfo.manifestId);
  addWebApkField(webApkList, 'Display Mode: ', webApkInfo.displayMode);
  addWebApkField(webApkList, 'Orientation: ', webApkInfo.orientation);
  addWebApkField(webApkList, 'Theme color: ', webApkInfo.themeColor);
  addWebApkField(webApkList, 'Background color: ', webApkInfo.backgroundColor);
  addWebApkField(webApkList, 'Dark theme color: ', webApkInfo.darkThemeColor);
  addWebApkField(
      webApkList, 'Dark background color: ', webApkInfo.darkBackgroundColor);
  addWebApkField(
      webApkList, 'Last Update Check Time: ',
      new Date(webApkInfo.lastUpdateCheckTimeMs).toString());
  addWebApkField(
      webApkList, 'Last Update Completion Time: ',
      new Date(webApkInfo.lastUpdateCompletionTimeMs).toString());
  addWebApkField(
      webApkList, 'Check for Updates Less Frequently: ',
      webApkInfo.relaxUpdates.toString());
  addWebApkField(webApkList, 'Owning Browser: ', webApkInfo.backingBrowser);
  addWebApkField(
      webApkList, 'Update Status (Reload page to get new status): ',
      webApkInfo.updateStatus);

  // TODO(ckitagawa): Convert to an enum using mojom handlers.
  if (webApkInfo.updateStatus === 'Not updatable' ||
      !webApkInfo.isBackingBrowser) {
    return;
  }

  addWebApkButton(webApkList, 'Update ' + webApkInfo.name, () => {
    alert(
        'The WebAPK will check for an update the next time it launches. ' +
        'If an update is available, the "Update Status" on this page ' +
        'will switch to "Scheduled". The update will be installed once ' +
        'the WebAPK is closed (this may take a few minutes). Update ' +
        'requests don\'t work if they are requested right after (< 1 ' +
        'minute) the completion of the previous update request.');
    chrome.send('requestWebApkUpdate', [webApkInfo.id]);
    window.location.reload();
  });
}

document.addEventListener('DOMContentLoaded', function() {
  // Add a WebUI listener for the 'web-apk-info' event emitted from the
  // backend. This will be triggered once per WebAPK.
  addWebUiListener('web-apk-info', addWebApk);
  chrome.send('requestWebApksInfo');
});
