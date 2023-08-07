// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INITIALIZED_KEY: string = 'sts_initialized';

async function selectToSpeakContextMenusCallback() {
  // TODO(b/271633121): Inform Lacros of the context menu click.
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

    // TODO(b/271633121): Add a context menu item to selection contexts.
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
