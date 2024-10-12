// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

import type {LanguagePackInfo} from './on_device_translation_internals.mojom-webui.js';
import {LanguagePackStatus} from './on_device_translation_internals.mojom-webui.js';
import {OnDeviceTranslationInternalsBrowserProxy} from './on_device_translation_internals_browser_proxy.js';

function getProxy(): OnDeviceTranslationInternalsBrowserProxy {
  return OnDeviceTranslationInternalsBrowserProxy.getInstance();
}

function toStatusString(status: LanguagePackStatus): string {
  switch (status) {
    case LanguagePackStatus.kNotInstalled:
      return 'Not installed';
    case LanguagePackStatus.kInstalling:
      return 'Installing';
    case LanguagePackStatus.kInstalled:
      return 'Installed';
    default:
      assertNotReached('Invalid status type.');
  }
}

function onLanguagePackStatus(status: LanguagePackInfo[]) {
  const div = getRequiredElement('lang_packages_table_div');
  div.innerHTML = window.trustedTypes!.emptyHTML;
  const table = document.createElement('table');
  let index: number = 0;
  status.forEach((info: LanguagePackInfo) => {
    const packageIndex = index;
    const tr = document.createElement('tr');
    const th = document.createElement('th');
    const td = document.createElement('td');
    th.appendChild(document.createTextNode(info.name));
    td.appendChild(document.createTextNode(`${toStatusString(info.status)}`));

    const tdForButton = document.createElement('td');
    if (info.status === LanguagePackStatus.kNotInstalled) {
      const button = document.createElement('button');
      button.appendChild(document.createTextNode('Install'));
      button.addEventListener('click', () => {
        getProxy().installLanguagePackage(packageIndex);
      });
      tdForButton.appendChild(button);
    } else {
      const button = document.createElement('button');
      button.appendChild(document.createTextNode('Uninstall'));
      button.addEventListener('click', () => {
        getProxy().uninstallLanguagePackage(packageIndex);
      });
      tdForButton.appendChild(button);
    }

    tr.appendChild(th);
    tr.appendChild(td);
    tr.appendChild(tdForButton);
    table.appendChild(tr);
    ++index;
  });
  div.appendChild(table);
}

async function initialize() {
  getProxy().getCallbackRouter().onLanguagePackStatus.addListener(
      onLanguagePackStatus);
}

document.addEventListener('DOMContentLoaded', initialize);
