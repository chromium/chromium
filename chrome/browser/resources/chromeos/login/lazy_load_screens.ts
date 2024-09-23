// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { assert } from '//resources/js/assert.js';

import {loadTimeData} from './i18n_setup.js';
import {addScreensToMainContainer} from './login_ui_tools.js';
import {TraceEvent, traceExecution} from './oobe_trace.js';
import {commonScreensList, loginScreensList, oobeScreensList} from './screens.js';

assert(window.scheduler, 'Prioritized Task Scheduling API unavailable');

// Add OOBE or LOGIN screens to the document.
const isOobeFlow = loadTimeData.getBoolean('isOobeFlow');
const flowSpecificScreensList = isOobeFlow ? oobeScreensList : loginScreensList;
const lazyLoadingEnabled = loadTimeData.getBoolean('isOobeLazyLoadingEnabled');

const isBootAnimation = loadTimeData.getBoolean('isBootAnimationEnabled');
const animationTransitionTime = 900;
let aboutToShrink = false;
if (isBootAnimation) {
  document.addEventListener('about-to-shrink', () => {
    aboutToShrink = true;
  }, {once: true});
}

// Right now we have only one priority screen and it is WelcomeScreen, that
// means that there is no effect from async loading of screens on the login
// page.
if (lazyLoadingEnabled) {
  addScreensAsync();
} else {
  addScreensSynchronously();
}

/**
 * Add screens to the document synchronously, blocking the main thread.
 */
function addScreensSynchronously(): void {
  addScreensToMainContainer(commonScreensList);
  traceExecution(TraceEvent.COMMON_SCREENS_ADDED);
  addScreensToMainContainer(flowSpecificScreensList);
  traceExecution(TraceEvent.REMAINING_SCREENS_ADDED);
  document.dispatchEvent(new CustomEvent('oobe-screens-loaded'));
}

/**
 * Add screens to the document asynchronously. Follows the same sequence logical
 * sequence as its synchronous counterpart. However, instead of blocking the
 * main thread, the actual adding of the screens are done via scheduling tasks
 * using the Prioritized Task Scheduling API.
 *
 * Note that  even though using 'setTimeout(..., 0)' provides a similar outcome,
 * 'scheduler.postTask' is more appropriate for this use case since it is not
 * impacted by Tab Throttling like 'setTimeout' is.
 */
function addScreensAsync(): void {
  // Optimization to make the shrink animation smooth by delaying the next
  // screen to be added by 'animationTransitionTime' milliseconds, leaving the
  // renderer solely with the task of animating.
  if (aboutToShrink) {
    aboutToShrink = false;
    window.scheduler.postTask(addScreensAsync,
        { delay: animationTransitionTime });
    return;
  }

  if (commonScreensList.length > 0) {
    const nextScreen = commonScreensList.pop();
    if (nextScreen) {
      addScreensToMainContainer([nextScreen]);
    }
    window.scheduler.postTask(addScreensAsync);

    if (commonScreensList.length === 0) {
      traceExecution(TraceEvent.COMMON_SCREENS_ADDED);
    }
  } else if (flowSpecificScreensList.length > 0) {
    const nextScreen = flowSpecificScreensList.pop();
    if (nextScreen) {
      addScreensToMainContainer([nextScreen]);
    }

    if (flowSpecificScreensList.length > 0) {
      window.scheduler.postTask(addScreensAsync);
    } else {
      traceExecution(TraceEvent.REMAINING_SCREENS_ADDED);
      document.dispatchEvent(new CustomEvent('oobe-screens-loaded'));
      // Finished
    }
  } else {
    assert(false, 'NOTREACHED()');
  }
}

declare global {
  interface Window {
    scheduler: any;
  }
}
