// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const INITIALIZED_KEY: string = 'cvox_initialized';

// Handles interacting with the Lacros browser via extension APIs
// for ChromeVox in Ash.

async function main() {
  // Inject script into all gdocs pages if not already initialized.
  // For any new tabs that are opened after the ChromeVox helper started
  // running, the manifest will handle content script injection, so there's
  // no need to listen for any changes: This only has to be done one-time.
  // Use session storage to ensure this is done once even if the service
  // worker wakes up again later.
  const initialized = await chrome.storage.session.get([INITIALIZED_KEY]);
  if (initialized && INITIALIZED_KEY in initialized) {
    return;
  }
  const storageUpdate = {[INITIALIZED_KEY]: true};
  chrome.storage.session.set(storageUpdate);

  // Inject into already-active tabs.
  let matches = [];
  try {
    matches = chrome.runtime.getManifest().content_scripts![0]!.matches!;
  } catch (e) {
    throw new Error('Unable to find content script matches entry in manifest.');
  }

  // Build one large regexp.
  const docsRe = new RegExp(matches.join('|'));

  const windows = await chrome.windows.getAll({'populate': true});
  for (const window of windows) {
    if (!window.tabs) {
      continue;
    }
    const tabs = window.tabs.filter(tab => docsRe.test(tab.url!));
    for (const tab of tabs) {
      chrome.scripting
          .executeScript({
            target: {tabId: tab.id!, allFrames: true},
            files: ['chromevox_helper/cvox_gdocs_script.js'],
          })
          .then(() => {
            if (chrome.runtime.lastError) {
              console.error('Could not inject into tab ', tab);
            }
          });
    }
  }
}

main();
