// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Skeleton test framework that adapts Files SWA to the testing requirement of
// Files.app. It is the SWA equivalent of
// ui/file_manager/file_manager/background/js/test_util_base.js. We cannot
// directly load the Files.app's test_util_base.js due to its dependencies on
// Chrome APIs not available in SWAs.

import {test} from 'chrome://file-manager/background/js/test_util.js';
import {ScriptLoader} from './script_loader.js';

delete test.util.registerRemoteTestUtils;

/**
 * Remote call API handler. This function handles messages coming from the test
 * harness to execute known functions and return results. This is a dummy
 * implementation that is replaced by a real one once the test harness is fully
 * loaded.
 * @type {function(*, function(*): void)}
 */
test.util.executeTestMessage = (request, callback) => {
  throw new Error('executeTestMessage not implemented');
};

/**
 * Called when all test utility functions (in Files.app's test_util.js) have
 * been installed in the test.util namespace. It exists here just to get handle
 * the call from test_utils.
 */
test.util.registerRemoteTestUtils = () => {
  console.assert(window.IN_TEST);
  console.assert(
      window.domAutomationController, 'domAutomationController not present');
};

/**
 * Handles a direct call from the integration test harness. We execute
 * swaTestMessageListener call directly from the FileManagerBrowserTest.
 * This method avoids enabling external callers to Files SWA. We forward
 * the response back to the caller, as a serialized JSON string.
 * @param {!Object} request
 */
test.swaTestMessageListener = (request) => {
  request.contentWindow = window.contentWindow || window;
  test.util.executeTestMessage(request, (response) => {
    response = response === undefined ? '@undefined@' : response;
    window.domAutomationController.send(JSON.stringify(response));
  });
};

let testUtilsLoaded = false;

test.swaLoadTestUtils = async (sendResults=true) => {
  const scriptUrl = './runtime_loaded_test_util_swa.js';
  let result = false;
  try {
    console.log('Loading ' + scriptUrl);
    await new ScriptLoader(scriptUrl, {type: 'module'}).load();
    testUtilsLoaded = true;
    result = true;
  } finally {
    if (sendResults) {
      window.domAutomationController.send(result);
    }
  }

  return result;
};

test.getSwaAppId = async () => {
  if (!testUtilsLoaded) {
    await test.swaLoadTestUtils(false);
  }

  window.domAutomationController.send(String(window.appID));
};
