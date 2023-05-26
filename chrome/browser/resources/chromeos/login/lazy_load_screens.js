// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { assert } from '//resources/ash/common/assert.js';

import {loadTimeData} from './i18n_setup.js';
import {addScreensToMainContainer} from './login_ui_tools.js';
import {TraceEvent, traceExecution} from './oobe_trace.js';
import {commonScreensList, loginScreensList, oobeScreensList} from './screens.js';

// Add OOBE or LOGIN screens to the document.
const isOobeFlow = loadTimeData.getBoolean('isOobeFlow');
const flowSpecificScreensList = isOobeFlow ? oobeScreensList : loginScreensList;
const lazyLoadingEnabled = loadTimeData.getBoolean('isOobeLazyLoadingEnabled');

const isOobeSimon = loadTimeData.getBoolean('isOobeSimonEnabled');
const animationTransitionTime = 900;
let aboutToShrink = false;
if (isOobeSimon) {
  document.addEventListener('about-to-shrink', () => {
    aboutToShrink = true;
  }, {once: true});
}


// Right now we have only one priority screen and it is WelcomeScreen, that
// means that there is no effect from async loading of screens on the login
// page.
if (lazyLoadingEnabled && isOobeFlow) {
  addScreensAsync();
} else {
  addScreensSynchronously();
}

/**
 * Add screens to the document synchronously, blocking the main thread.
 */
function addScreensSynchronously() {
  addScreensToMainContainer(commonScreensList);
  traceExecution(TraceEvent.COMMON_SCREENS_ADDED);
  addScreensToMainContainer(flowSpecificScreensList);
  traceExecution(TraceEvent.REMAINING_SCREENS_ADDED);
  document.dispatchEvent(new CustomEvent('oobe-screens-loaded'));
}

/**
 * Add screens to the document asynchronously. Follows the same sequence logical
 * sequence as its synchronous counterpart. However, instead of blocking the
 * main thread, the actual adding of the screens are done via scheduling tasks.
 */
function addScreensAsync() {
  // Optimization to make the shrink animation smooth.
  if (aboutToShrink) {
    aboutToShrink = false;
    setTimeout(addScreensAsync, animationTransitionTime);
    return;
  }
  if (commonScreensList.length > 0) {
    const nextScreens = commonScreensList.pop();
    addScreensToMainContainer([nextScreens]);
    setTimeout(addScreensAsync, 0);

    if (commonScreensList.length == 0) {
      traceExecution(TraceEvent.COMMON_SCREENS_ADDED);
    }
  } else if (flowSpecificScreensList.length > 0) {
    const nextScreens = flowSpecificScreensList.pop();
    addScreensToMainContainer([nextScreens]);

    if (flowSpecificScreensList.length > 0) {
      setTimeout(addScreensAsync, 0);
    } else {
      traceExecution(TraceEvent.REMAINING_SCREENS_ADDED);
      document.dispatchEvent(new CustomEvent('oobe-screens-loaded'));
      // Finished
    }
  } else {
    assert(false, 'NOTREACHED()');
  }
}
