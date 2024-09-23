// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {CustomElement} from 'chrome://resources/js/custom_element.js';
import {listenOnce} from 'chrome://resources/js/util.js';

import {getTemplate} from './snackbar.html.js';

/**
 * Javascript for Snackbar controls, served from chrome://bluetooth-internals/.
 */

/**
 * @typedef {{
 *    message: string,
 *    type: string,
 *    actionText: (string|undefined),
 *    action: (function()|undefined)
 *  }}
 */
let SnackbarOptions;

/** @type {number} */ const SHOW_DURATION = 5000;
/** @type {number} */ const TRANSITION_DURATION = 225;

/**
 * Enum of Snackbar types. Used by Snackbar to determine the styling for the
 * Snackbar.
 * @enum {string}
 */
export const SnackbarType = {
  INFO: 'info',
  SUCCESS: 'success',
  WARNING: 'warning',
  ERROR: 'error',
};

/**
 * Notification bar for displaying a simple message with an action link.
 * This element should not be instantiated directly. Instead, users should
 * use the showSnackbar and dismissSnackbar functions to ensure proper
 * queuing of messages.
 */
class BluetoothSnackbarElement extends CustomElement {
  static get template() {
    return getTemplate();
  }

  static get is() {
    return 'bluetooth-snackbar';
  }

  constructor() {
    super();

    /** @type {?Function} */
    this.boundStartTimeout_ = null;

    /** @type {?Function} */
    this.boundStopTimeout_ = null;

    /** @type {?number} */
    this.timeoutId_ = null;

    /** @type {?SnackbarOptions} */
    this.options_ = null;
  }

  connectedCallback() {
    assert(this.options_);

    this.shadowRoot.querySelector('#message').textContent =
        this.options_.message;
    this.classList.add(this.options_.type);
    const actionLink = this.shadowRoot.querySelector('a');
    actionLink.textContent = this.options_.actionText || 'Dismiss';

    actionLink.addEventListener('click', () => {
      if (this.options_.action) {
        this.options_.action();
      }
      this.dismiss();
    });

    this.boundStartTimeout_ = this.startTimeout_.bind(this);
    this.boundStopTimeout_ = this.stopTimeout_.bind(this);
    this.addEventListener('mouseleave', this.boundStartTimeout_);
    this.addEventListener('mouseenter', this.boundStopTimeout_);

    this.timeoutId_ = null;
  }

  /**
   * Initializes the content of the Snackbar with the given |options|
   * including the message, action link text, and click action of the link.
   * This must be called before the element is added to the DOM.
   * @param {!SnackbarOptions} options
   */
  initialize(options) {
    this.options_ = options;
  }

  /**
   * Shows the Snackbar and dispatches the 'showed' event.
   */
  show() {
    this.classList.add('open');
    if (hasContentFocus) {
      this.startTimeout_();
    } else {
      this.stopTimeout_();
    }

    document.addEventListener('contentfocus', this.boundStartTimeout_);
    document.addEventListener('contentblur', this.boundStopTimeout_);
    this.dispatchEvent(
        new CustomEvent('showed', {bubbles: true, composed: true}));
  }
  /**
   * transitionend does not always fire (e.g. when animation is aborted
   * or when no paint happens during the animation). This function sets up
   * a timer and emulate the event if it is not fired when the timer expires.
   * @private
   */
  ensureTransitionEndEvent_() {
    let fired = false;
    this.addEventListener('transitionend', function f(e) {
      this.removeEventListener('transitionend', f);
      fired = true;
    }.bind(this));
    window.setTimeout(() => {
      if (!fired) {
        this.dispatchEvent(new CustomEvent('transitionend',
              {bubbles: true, composed: true}));
      }
    }, TRANSITION_DURATION);
  }

  /**
   * Dismisses the Snackbar. Once the Snackbar is completely hidden, the
   * 'dismissed' event is fired and the returned Promise is resolved. If the
   * snackbar is already hidden, a resolved Promise is returned.
   * @return {!Promise}
   */
  dismiss() {
    this.stopTimeout_();

    if (!this.classList.contains('open')) {
      return Promise.resolve();
    }

    return new Promise(function(resolve) {
      listenOnce(this, 'transitionend', function() {
        this.dispatchEvent(new CustomEvent('dismissed'));
        resolve();
      }.bind(this));

      this.ensureTransitionEndEvent_();
      this.classList.remove('open');

      document.removeEventListener('contentfocus', this.boundStartTimeout_);
      document.removeEventListener('contentblur', this.boundStopTimeout_);
    }.bind(this));
  }

  /**
   * Starts the timeout for dismissing the Snackbar.
   * @private
   */
  startTimeout_() {
    this.timeoutId_ = setTimeout(function() {
      this.dismiss();
    }.bind(this), SHOW_DURATION);
  }

  /**
   * Stops the timeout for dismissing the Snackbar. Only clears the timeout
   * when the Snackbar is open.
   * @private
   */
  stopTimeout_() {
    if (this.classList.contains('open')) {
      clearTimeout(this.timeoutId_);
      this.timeoutId_ = null;
    }
  }
}

customElements.define('bluetooth-snackbar', BluetoothSnackbarElement);

/** @type {?BluetoothSnackbarElement} */
let current = null;

/** @type {!Array<!BluetoothSnackbarElement>} */
let queue = [];

/** @type {boolean} */
let hasContentFocus = true;

// There is a chance where the snackbar is shown but the content doesn't have
// focus. In this case, the current focus state must be tracked so the
// snackbar can pause the dismiss timeout.
document.addEventListener('contentfocus', function() {
  hasContentFocus = true;
});
document.addEventListener('contentblur', function() {
  hasContentFocus = false;
});

/**
 * @return {{current: ?BluetoothSnackbarElement, numPending: number}}
 */
export function getSnackbarStateForTest() {
  return {
    current: current,
    numPending: queue.length,
  };
}

/**
 * TODO(crbug.com/40498702): Add ability to specify parent element to Snackbar.
 * Creates a Snackbar and shows it if one is not showing already. If a
 * Snackbar is already active, the next Snackbar is queued.
 * @param {string} message The message to display in the Snackbar.
 * @param {string=} opt_type A string determining the Snackbar type: info,
 *     success, warning, error. If not provided, info type is used.
 * @param {string=} opt_actionText The text to display for the action link.
 * @param {function()=} opt_action A function to be called when the user
 *     presses the action link.
 * @return {!BluetoothSnackbarElement}
 */
export function showSnackbar(message, opt_type, opt_actionText, opt_action) {
  const options = {
    message: message,
    type: opt_type || SnackbarType.INFO,
    actionText: opt_actionText,
    action: opt_action,
  };
  const newSnackbar = document.createElement('bluetooth-snackbar');
  newSnackbar.initialize(options);

  if (current) {
    queue.push(newSnackbar);
  } else {
    show(newSnackbar);
  }

  return newSnackbar;
}

window.showSnackbar = showSnackbar;

/**
 * TODO(crbug.com/40498702): Add ability to specify parent element to Snackbar.
 * Creates a Snackbar and sets events for queuing the next Snackbar to show.
 * @param {!BluetoothSnackbarElement} snackbar
 */
function show(snackbar) {
  document.body.querySelector('#snackbar-container').appendChild(snackbar);

  snackbar.addEventListener('dismissed', function() {
    document.body.querySelector('#snackbar-container').removeChild(current);

    const newSnackbar = queue.shift();
    if (newSnackbar) {
      show(newSnackbar);
      return;
    }

    current = null;
  });

  current = snackbar;

  // Show the Snackbar after a slight delay to allow for a layout reflow.
  setTimeout(function() {
    snackbar.show();
  }, 10);
}

/**
 * Dismisses the Snackbar currently showing.
 * @param {boolean} clearQueue If true, clears the Snackbar queue before
 *     dismissing.
 * @return {!Promise}
 */
export function dismissSnackbar(clearQueue) {
  if (clearQueue) {
    queue = [];
  }
  if (current) {
    return current.dismiss();
  }
  return Promise.resolve();
}
