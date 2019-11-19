// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for Snackbar controls, served from chrome://bluetooth-internals/.
 */

cr.define('snackbar', function() {
  /** @typedef {{
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
  const SnackbarType = {
    INFO: 'info',
    SUCCESS: 'success',
    WARNING: 'warning',
    ERROR: 'error',
  };

  /**
   * Notification bar for displaying a simple message with an action link.
   * This element should not be instantiated directly. Instead, users should
   * use the Snackbar.show and Snackbar.dismiss functions to ensure proper
   * queuing of messages.
   * @constructor
   * @extends {HTMLDivElement}
   */
  const Snackbar = cr.ui.define('div');

  Snackbar.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Decorates an element as a UI element class. Creates the message div and
     * action link for the new Snackbar.
     */
    decorate: function() {
      this.classList.add('snackbar');
      this.messageDiv_ = document.createElement('div');
      this.appendChild(this.messageDiv_);
      this.actionLink_ = document.createElement('a', 'action-link');
      this.appendChild(this.actionLink_);

      this.boundStartTimeout_ = this.startTimeout_.bind(this);
      this.boundStopTimeout_ = this.stopTimeout_.bind(this);
      this.addEventListener('mouseleave', this.boundStartTimeout_);
      this.addEventListener('mouseenter', this.boundStopTimeout_);

      this.timeoutId_ = null;
    },

    /**
     * Initializes the content of the Snackbar with the given |options|
     * including the message, action link text, and click action of the link.
     * @param {!SnackbarOptions} options
     */
    initialize: function(options) {
      this.messageDiv_.textContent = options.message;
      this.classList.add(options.type);
      this.actionLink_.textContent = options.actionText || 'Dismiss';

      this.actionLink_.addEventListener('click', function() {
        if (options.action) {
          options.action();
        }
        this.dismiss();
      }.bind(this));
    },

    /**
     * Shows the Snackbar and dispatches the 'showed' event.
     */
    show: function() {
      this.classList.add('open');
      if (Snackbar.hasContentFocus_) {
        this.startTimeout_();
      } else {
        this.stopTimeout_();
      }

      document.addEventListener('contentfocus', this.boundStartTimeout_);
      document.addEventListener('contentblur', this.boundStopTimeout_);
      this.dispatchEvent(new CustomEvent('showed'));
    },

    /**
     * Dismisses the Snackbar. Once the Snackbar is completely hidden, the
     * 'dismissed' event is fired and the returned Promise is resolved. If the
     * snackbar is already hidden, a resolved Promise is returned.
     * @return {!Promise}
     */
    dismiss: function() {
      this.stopTimeout_();

      if (!this.classList.contains('open')) {
        return Promise.resolve();
      }

      return new Promise(function(resolve) {
        listenOnce(this, 'transitionend', function() {
          this.dispatchEvent(new CustomEvent('dismissed'));
          resolve();
        }.bind(this));

        ensureTransitionEndEvent(this, TRANSITION_DURATION);
        this.classList.remove('open');

        document.removeEventListener('contentfocus', this.boundStartTimeout_);
        document.removeEventListener('contentblur', this.boundStopTimeout_);
      }.bind(this));
    },

    /**
     * Starts the timeout for dismissing the Snackbar.
     * @private
     */
    startTimeout_: function() {
      this.timeoutId_ = setTimeout(function() {
        this.dismiss();
      }.bind(this), SHOW_DURATION);
    },

    /**
     * Stops the timeout for dismissing the Snackbar. Only clears the timeout
     * when the Snackbar is open.
     * @private
     */
    stopTimeout_: function() {
      if (this.classList.contains('open')) {
        clearTimeout(this.timeoutId_);
        this.timeoutId_ = null;
      }
    },
  };

  /** @private {?snackbar.Snackbar} */
  Snackbar.current_ = null;

  /** @private {!Array<!snackbar.Snackbar>} */
  Snackbar.queue_ = [];

  /** @private {boolean} */
  Snackbar.hasContentFocus_ = true;

  // There is a chance where the snackbar is shown but the content doesn't have
  // focus. In this case, the current focus state must be tracked so the
  // snackbar can pause the dismiss timeout.
  document.addEventListener('contentfocus', function() {
    Snackbar.hasContentFocus_ = true;
  });
  document.addEventListener('contentblur', function() {
    Snackbar.hasContentFocus_ = false;
  });

  /**
   * TODO(crbug.com/675299): Add ability to specify parent element to Snackbar.
   * Creates a Snackbar and shows it if one is not showing already. If a
   * Snackbar is already active, the next Snackbar is queued.
   * @param {string} message The message to display in the Snackbar.
   * @param {string=} opt_type A string determining the Snackbar type: info,
   *     success, warning, error. If not provided, info type is used.
   * @param {string=} opt_actionText The text to display for the action link.
   * @param {function()=} opt_action A function to be called when the user
   *     presses the action link.
   * @return {!snackbar.Snackbar}
   */
  Snackbar.show = function(message, opt_type, opt_actionText, opt_action) {
    const options = {
      message: message,
      type: opt_type || SnackbarType.INFO,
      actionText: opt_actionText,
      action: opt_action,
    };

    const newSnackbar = new Snackbar();
    newSnackbar.initialize(options);

    if (Snackbar.current_) {
      Snackbar.queue_.push(newSnackbar);
    } else {
      Snackbar.show_(newSnackbar);
    }

    return newSnackbar;
  };

  /**
   * TODO(crbug.com/675299): Add ability to specify parent element to Snackbar.
   * Creates a Snackbar and sets events for queuing the next Snackbar to show.
   * @param {!snackbar.Snackbar} newSnackbar
   * @private
   */
  Snackbar.show_ = function(newSnackbar) {
    $('snackbar-container').appendChild(newSnackbar);

    newSnackbar.addEventListener('dismissed', function() {
      $('snackbar-container').removeChild(Snackbar.current_);

      const newSnackbar = Snackbar.queue_.shift();
      if (newSnackbar) {
        Snackbar.show_(newSnackbar);
        return;
      }

      Snackbar.current_ = null;
    });

    Snackbar.current_ = newSnackbar;

    // Show the Snackbar after a slight delay to allow for a layout reflow.
    setTimeout(function() {
      newSnackbar.show();
    }, 10);
  };

  /**
   * Dismisses the Snackbar currently showing.
   * @param {boolean} clearQueue If true, clears the Snackbar queue before
   *     dismissing.
   */
  Snackbar.dismiss = function(clearQueue) {
    if (clearQueue) {
      Snackbar.queue_ = [];
    }
    if (Snackbar.current_) {
      return Snackbar.current_.dismiss();
    }
    return Promise.resolve();
  };



  return {
    Snackbar: Snackbar,
    SnackbarType: SnackbarType,
  };
});
