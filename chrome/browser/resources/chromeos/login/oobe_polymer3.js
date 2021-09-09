import {loadTimeData} from './i18n_setup.js';
import {DebuggerUI} from './debug/debug.m.js';
import {Oobe} from './cr_ui.m.js';

import 'chrome://oobe/screens/common/marketing_opt_in.m.js';

function initializeDebugger() {
  if (document.readyState === 'loading')
    return;
  document.removeEventListener('DOMContentLoaded', initializeDebugger);
  DebuggerUI.getInstance().register(document.body);
}

// Create the global values attached to `window` that are used
// for accessing OOBE controls from the browser side.
function prepareGlobalValues(globalValue) {
    console.log('Preparing global values.');
    if (globalValue.cr == undefined) {
        globalValue.cr = {};
    }
    if (globalValue.cr.ui == undefined) {
        globalValue.cr.ui = {};
    }

    globalValue.cr.ui.Oobe = Oobe;
    globalValue.Oobe = Oobe;
}

(function (root) {
    prepareGlobalValues(window);
    Oobe.initialize();

    // Initialize debugger.
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', initializeDebugger);
      } else {
        initializeDebugger();
    }

    // Make the WebUI visible.
    chrome.send('loginVisible', ['oobe']);
})(window);
