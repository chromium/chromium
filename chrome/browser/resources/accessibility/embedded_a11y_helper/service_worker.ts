// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INITIALIZED_KEY: string = 'sts_initialized';
// During development, chrome.accessibilityServicePrivate is behind a feature
// flag.
const SHOW_CONTEXT_MENU = chrome.accessibilityServicePrivate !== undefined;

// Matches one of the known GSuite apps which need the clipboard to find and
// read selected text. Includes sandbox and non-sandbox versions.
const GSUITE_APP_REGEXP =
    /^https:\/\/docs\.(?:sandbox\.)?google\.com\/(?:(?:presentation)|(?:document)|(?:spreadsheets)|(?:drawings)|(?:scenes)){1}\//;

async function selectToSpeakContextMenusCallback() {
  // Inform Lacros of the context menu click.
  if (SHOW_CONTEXT_MENU) {
    chrome.accessibilityServicePrivate.speakSelectedText();
  }
}

async function clipboardCopyInActiveGoogleDoc(url: string) {
  const queryOptions = {active: true, currentWindow: true};
  const [tab] = await chrome.tabs.query(queryOptions);
  if (tab?.id && tab.url && tab.url === url &&
      GSUITE_APP_REGEXP.exec(tab.url)) {
    chrome.scripting.executeScript({
      target: {tabId: tab.id, allFrames: true},
      files: ['embedded_a11y_helper/clipboard_copy.js'],
    });
  }
}

async function onSelectToSpeakChanged(
    details: chrome.accessibilityFeatures.ChromeSettingsResponse) {
  if (details.value) {
    const initialized = await chrome.storage.session.get([INITIALIZED_KEY]);
    if (initialized && initialized[INITIALIZED_KEY] === true) {
      return;
    }
    const storageUpdate = {[INITIALIZED_KEY]: true};
    chrome.storage.session.set(storageUpdate);

    //  Add a context menu item to selection contexts.
    if (SHOW_CONTEXT_MENU) {
      await chrome.contextMenus.create({
        title: chrome.i18n.getMessage(
            'select_to_speak_listen_context_menu_option_text'),
        contexts: [chrome.contextMenus.ContextType.SELECTION],
        id: 'embedded_a11y_helper',
      });
    }
    return;
  }

  // Clear the context menu if there was one.
  chrome.contextMenus.removeAll();
  const storageUpdate = {[INITIALIZED_KEY]: false};
  chrome.storage.session.set(storageUpdate);
}

async function main() {
  chrome.contextMenus.onClicked.addListener(selectToSpeakContextMenusCallback);
  chrome.accessibilityServicePrivate.clipboardCopyInActiveGoogleDoc.addListener(
      url => clipboardCopyInActiveGoogleDoc(url));

  // Set up based on current state.
  const currentDetails =
      await chrome.accessibilityFeatures.selectToSpeak.get({});
  await onSelectToSpeakChanged(currentDetails);

  // Add a listener for future changes.
  chrome.accessibilityFeatures.selectToSpeak.onChange.addListener(
      details => onSelectToSpeakChanged(details));
}

main();
