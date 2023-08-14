// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INITIALIZED_KEY: string = 'sts_initialized';
// During development, chrome.accessibilityServicePrivate is behind a feature
// flag.
const SHOW_CONTEXT_MENU = chrome.accessibilityServicePrivate !== undefined;

async function selectToSpeakContextMenusCallback() {
  // Inform Lacros of the context menu click.
  if (SHOW_CONTEXT_MENU) {
    chrome.accessibilityServicePrivate.speakSelectedText();
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

  // Set up based on current state.
  const currentDetails =
      await chrome.accessibilityFeatures.selectToSpeak.get({});
  await onSelectToSpeakChanged(currentDetails);

  // Add a listener for future changes.
  chrome.accessibilityFeatures.selectToSpeak.onChange.addListener(
      details => onSelectToSpeakChanged(details));
}

main();
