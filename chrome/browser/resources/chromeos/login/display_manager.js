// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Display manager for WebUI OOBE and login.
 */

// <include src="components/display_manager_types.js">

// TODO(xiyuan): Find a better to share those constants.
/** @const */ var SCREEN_WELCOME = 'connect';
/** @const */ var SCREEN_OOBE_NETWORK = 'network-selection';
/** @const */ var SCREEN_OOBE_HID_DETECTION = 'hid-detection';
/** @const */ var SCREEN_OOBE_ENABLE_DEBUGGING = 'debugging';
/** @const */ var SCREEN_OOBE_UPDATE = 'oobe-update';
/** @const */ var SCREEN_OOBE_RESET = 'reset';
/** @const */ var SCREEN_OOBE_ENROLLMENT = 'enterprise-enrollment';
/** @const */ var SCREEN_OOBE_DEMO_SETUP = 'demo-setup';
/** @const */ var SCREEN_OOBE_DEMO_PREFERENCES = 'demo-preferences';
/** @const */ var SCREEN_OOBE_KIOSK_ENABLE = 'kiosk-enable';
/** @const */ var SCREEN_PACKAGED_LICENSE = 'packaged-license';
/** @const */ var SCREEN_GAIA_SIGNIN = 'gaia-signin';
/** @const */ var SCREEN_ERROR_MESSAGE = 'error-message';
/** @const */ var SCREEN_PASSWORD_CHANGED = 'gaia-password-changed';
/** @const */ var SCREEN_APP_LAUNCH_SPLASH = 'app-launch-splash';
/** @const */ var SCREEN_CONFIRM_PASSWORD = 'saml-confirm-password';
/** @const */ var SCREEN_FATAL_ERROR = 'fatal-error';
/** @const */ var SCREEN_KIOSK_ENABLE = 'kiosk-enable';
/** @const */ var SCREEN_TERMS_OF_SERVICE = 'terms-of-service';
/** @const */ var SCREEN_ARC_TERMS_OF_SERVICE = 'arc-tos';
/** @const */ var SCREEN_DEVICE_DISABLED = 'device-disabled';
/** @const */ var SCREEN_UPDATE_REQUIRED = 'update-required';
/** @const */ var SCREEN_ACTIVE_DIRECTORY_PASSWORD_CHANGE =
    'ad-password-change';
/** @const */ var SCREEN_SYNC_CONSENT = 'sync-consent';
/** @const */ var SCREEN_FINGERPRINT_SETUP = 'fingerprint-setup';
/** @const */ var SCREEN_RECOMMEND_APPS = 'recommend-apps';
/** @const */ var SCREEN_APP_DOWNLOADING = 'app-downloading';
/** @const */ var SCREEN_PIN_SETUP = 'pin-setup';
/** @const */ var SCREEN_MARKETING_OPT_IN = 'marketing-opt-in';

/* Accelerator identifiers.
 * Must be kept in sync with webui_accelerator_mapping.cc.
 */
/** @const */ var ACCELERATOR_CANCEL = 'cancel';
/** @const */ var ACCELERATOR_VERSION = 'version';
/** @const */ var ACCELERATOR_RESET = 'reset';
/** @const */ var ACCELERATOR_APP_LAUNCH_BAILOUT = 'app_launch_bailout';
/** @const */ var ACCELERATOR_APP_LAUNCH_NETWORK_CONFIG =
    'app_launch_network_config';

/** @const */ var USER_ACTION_ROLLBACK_TOGGLED = 'rollback-toggled';

cr.define('cr.ui.login', function() {
  var Bubble = cr.ui.Bubble;

  /**
   * Maximum time in milliseconds to wait for step transition to finish.
   * The value is used as the duration for ensureTransitionEndEvent below.
   * It needs to be inline with the step screen transition duration time
   * defined in css file. The current value in css is 200ms. To avoid emulated
   * transitionend fired before real one, 250ms is used.
   * @const
   */
  var MAX_SCREEN_TRANSITION_DURATION = 250;

  /**
   * Group of screens (screen IDs) where factory-reset screen invocation is
   * available. Newer screens using Polymer use the attribute
   * `resetAllowed` in their `ready()` method.
   * @type Array<string>
   * @const
   */
  var RESET_AVAILABLE_SCREEN_GROUP = [
    SCREEN_OOBE_NETWORK,
    SCREEN_GAIA_SIGNIN,
    SCREEN_KIOSK_ENABLE,
    SCREEN_ERROR_MESSAGE,
    SCREEN_PASSWORD_CHANGED,
    SCREEN_ARC_TERMS_OF_SERVICE,
    SCREEN_CONFIRM_PASSWORD,
    SCREEN_UPDATE_REQUIRED,
    SCREEN_SYNC_CONSENT,
    SCREEN_APP_DOWNLOADING,
    SCREEN_PIN_SETUP,
    SCREEN_MARKETING_OPT_IN,
  ];

  /**
   * As Polymer behaviors do not provide true inheritance, when two behaviors
   * would declare same method one of them will be hidden. Also, if element
   * re-declares the method it needs explicitly iterate over behaviors and call
   * method on them. This function simplifies such interaction by calling
   * method on element and all behaviors in the same order as lifecycle
   * callbacks are called.
   * @param {Element} element
   * @param {string} name function name
   * @param {...*} arguments arguments for the function
   */
  function invokePolymerMethod(element, name, ...args) {
    let method = element[name];
    if (!method || typeof method !== 'function')
      return;
    method.apply(element, args);
    if (!element.behaviors)
      return;

    // If element has behaviors call functions on them in reverse order,
    // ignoring case when method on element was derived from behavior.
    for (var i = element.behaviors.length - 1; i >= 0; i--) {
      let behavior = element.behaviors[i];
      let behaviorMethod = behavior[name];
      if (!behaviorMethod || typeof behaviorMethod !== 'function')
        continue;
      if (behaviorMethod == method)
        continue;
      behaviorMethod.apply(element, args);
    }
  }

  /**
   * Constructor a display manager that manages initialization of screens,
   * transitions, error messages display.
   *
   * @constructor
   */
  function DisplayManager() {}

  DisplayManager.prototype = {
    /**
     * Registered screens.
     */
    screens_: [],

    /**
     * Attributes of the registered screens.
     * @type {Array<DisplayManagerScreenAttributes>}
     */
    screensAttributes_: [],

    /**
     * Current OOBE step, index in the screens array.
     * @type {number}
     */
    currentStep_: 0,

    /**
     * Whether version label can be toggled by ACCELERATOR_VERSION.
     * @type {boolean}
     */
    allowToggleVersion_: false,

    /**
     * Whether keyboard navigation flow is enforced.
     * @type {boolean}
     */
    forceKeyboardFlow_: false,

    /**
     * Whether the virtual keyboard is displayed.
     * @type {boolean}
     */
    virtualKeyboardShown_: false,

    get virtualKeyboardShown() {
      return this.virtualKeyboardShown_;
    },

    set virtualKeyboardShown(shown) {
      this.virtualKeyboardShown_ = shown;
      document.documentElement.setAttribute('virtual-keyboard', shown);
    },

    /**
     * Type of UI.
     * @type {string}
     */
    displayType_: DISPLAY_TYPE.UNKNOWN,

    /**
     * Number of users in the login screen UI. This is used by the views login
     * screen, and is always 0 for WebUI login screen.
     * TODO(crbug.com/808271): WebUI and views implementation should return the
     * same user list.
     * @type {number}
     */
    userCount_: 0,

    /**
     * Stored OOBE configuration for newly registered screens.
     * @type {!OobeTypes.OobeConfiguration}
     */
    oobe_configuration_: undefined,

    /**
     * Detects multi-tap gesture that invokes demo mode setup in OOBE.
     * @type {?MultiTapDetector}
     * @private
     */
    demoModeStartListener_: null,

    /**
     * Error message (bubble) was shown. This is checked in tests.
     */
    errorMessageWasShownForTesting_: false,

    get displayType() {
      return this.displayType_;
    },

    set displayType(displayType) {
      this.displayType_ = displayType;
      document.documentElement.setAttribute('screen', displayType);
    },

    /**
     * Returns dimensions of screen excluding header bar.
     * @type {Object}
     */
    get clientAreaSize() {
      var container = $('outer-container');
      return {width: container.offsetWidth, height: container.offsetHeight};
    },

    /**
     * Gets current screen element.
     * @type {HTMLElement}
     */
    get currentScreen() {
      return $(this.screens_[this.currentStep_]);
    },

    /**
     * Returns true if we are showing views based login screen.
     * @return {boolean}
     */
    get showingViewsLogin() {
      return this.displayType_ == DISPLAY_TYPE.GAIA_SIGNIN;
    },

    /**
     * Returns true if the login screen has user pods.
     * @return {boolean}
     */
    get hasUserPods() {
      return this.showingViewsLogin && this.userCount_ > 0;
    },

    /**
     * Sets the current size of the client area (display size).
     * @param {number} width client area width
     * @param {number} height client area height
     */
    setClientAreaSize: function(width, height) {
      if (!cr.isChromeOS) {
        var clientArea = $('outer-container');
        var bottom = parseInt(window.getComputedStyle(clientArea).bottom);
        clientArea.style.minHeight = cr.ui.toCssPx(height - bottom);
      }
    },

    /**
     * Sets the current height of the shelf area.
     * @param {number} height current shelf height
     */
    setShelfHeight: function(height) {
      document.documentElement.style.setProperty(
          '--shelf-area-height-base', height + 'px');
    },

    setOrientation: function(isHorizontal) {
      if (isHorizontal) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    },

    setDialogSize: function(width, height) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', height + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', width + 'px');
    },

    /**
     * Sets the hint for calculating OOBE dialog inner padding.
     * @param {OobeTypes.DialogPaddingMode} mode.
     */
    setDialogPaddingMode: function(mode) {
      document.documentElement.setAttribute('dialog-padding', mode);
    },

    /**
     * Toggles background of main body between transparency and solid.
     * @param {boolean} solid Whether to show a solid background.
     */
    set solidBackground(solid) {
      if (solid)
        document.body.classList.add('solid');
      else
        document.body.classList.remove('solid');
    },

    /**
     * Forces keyboard based OOBE navigation.
     * @param {boolean} value True if keyboard navigation flow is forced.
     */
    set forceKeyboardFlow(value) {
      this.forceKeyboardFlow_ = value;
      if (value) {
        keyboard.initializeKeyboardFlow(false);
        for (var i = 0; i < this.screens_.length; ++i) {
          var screen = $(this.screens_[i]);
          if (screen.enableKeyboardFlow)
            screen.enableKeyboardFlow();
        }
      }
    },

    /**
     * Returns true if keyboard flow is enabled.
     * @return {boolean}
     */
    get forceKeyboardFlow() {
      return this.forceKeyboardFlow_;
    },

    /**
     * Returns current OOBE configuration.
     * @return {!OobeTypes.OobeConfiguration}
     */
    getOobeConfiguration: function() {
      return this.oobe_configuration_;
    },

    /**
     * Shows/hides version labels.
     * @param {boolean} show Whether labels should be visible by default. If
     *     false, visibility can be toggled by ACCELERATOR_VERSION.
     */
    showVersion: function(show) {
      $('version-labels').hidden = !show;
      this.allowToggleVersion_ = !show;
    },

    /**
     * Sets the number of users on the views login screen.
     * @param {number} userCount The number of users.
     */
    setLoginUserCount: function(userCount) {
      this.userCount_ = userCount;
    },

    /**
     * Handle accelerators.
     * @param {string} name Accelerator name.
     */
    handleAccelerator: function(name) {
      if (this.currentScreen && this.currentScreen.ignoreAccelerators) {
        return;
      }
      var currentStepId = this.screens_[this.currentStep_];
      var attributes = this.screensAttributes_[this.currentStep_] || {};
      if (name == ACCELERATOR_CANCEL) {
        if (this.currentScreen && this.currentScreen.cancel) {
          this.currentScreen.cancel();
        }
      } else if (name == ACCELERATOR_VERSION) {
        if (this.allowToggleVersion_)
          $('version-labels').hidden = !$('version-labels').hidden;
      } else if (name == ACCELERATOR_RESET) {
        if (currentStepId == SCREEN_OOBE_RESET) {
          $('reset').userActed(USER_ACTION_ROLLBACK_TOGGLED);
        } else if (
            attributes.resetAllowed ||
            RESET_AVAILABLE_SCREEN_GROUP.indexOf(currentStepId) != -1) {
          chrome.send('toggleResetScreen');
        }
      } else if (name == ACCELERATOR_APP_LAUNCH_BAILOUT) {
        if (currentStepId == SCREEN_APP_LAUNCH_SPLASH)
          chrome.send('cancelAppLaunch');
      } else if (name == ACCELERATOR_APP_LAUNCH_NETWORK_CONFIG) {
        if (currentStepId == SCREEN_APP_LAUNCH_SPLASH)
          chrome.send('networkConfigRequest');
      }
    },

    /**
     * Switches to the next OOBE step.
     * @param {number} nextStepIndex Index of the next step.
     */
    toggleStep_: function(nextStepIndex, screenData) {
      var currentStepId = this.screens_[this.currentStep_];
      var nextStepId = this.screens_[nextStepIndex];
      var oldStep = $(currentStepId);
      var newStep = $(nextStepId);

      invokePolymerMethod(oldStep, 'onBeforeHide');

      if (oldStep.defaultControl)
        invokePolymerMethod(oldStep.defaultControl, 'onBeforeHide');

      $('oobe').className = nextStepId;

      // Need to do this before calling newStep.onBeforeShow() so that new step
      // is back in DOM tree and has correct offsetHeight / offsetWidth.
      newStep.hidden = false;

      if (newStep.getOobeUIInitialState) {
        this.setOobeUIState(newStep.getOobeUIInitialState());
      } else {
        this.setOobeUIState(OOBE_UI_STATE.HIDDEN);
      }

      invokePolymerMethod(newStep, 'onBeforeShow', screenData);

      // We still have several screens that are not implemented as a single
      // Polymer-element, so we need to explicitly inform all oobe-dialogs.
      //
      // TODO(alemate): make every screen a single Polymer element, so that
      // we could simply use OobeDialogHostBehavior in stead of this.
      for (let dialog of newStep.getElementsByTagName('oobe-dialog'))
        invokePolymerMethod(dialog, 'onBeforeShow', screenData);

      if (newStep.defaultControl)
        invokePolymerMethod(newStep.defaultControl, 'onBeforeShow', screenData);

      newStep.classList.remove('hidden');

      // Start fading animation for login display or reset screen.
      oldStep.classList.add('faded');
      newStep.classList.remove('faded');

      // Adjust inner container height based on new step's height.
      this.updateScreenSize(newStep);

      // Default control to be focused (if specified).
      var defaultControl = newStep.defaultControl;

      var innerContainer = $('inner-container');
      if (this.currentStep_ != nextStepIndex &&
          !oldStep.classList.contains('hidden')) {
        oldStep.classList.add('hidden');
        oldStep.hidden = true;
        if (defaultControl)
          defaultControl.focus();
      } else {
        // First screen on OOBE launch.
        if (this.isOobeUI() && innerContainer.classList.contains('down')) {
          innerContainer.classList.remove('down');
          innerContainer.addEventListener('transitionend', function f(e) {
            innerContainer.removeEventListener('transitionend', f);
            chrome.send('loginVisible', ['oobe']);
            // Refresh defaultControl. It could have changed.
            var defaultControl = newStep.defaultControl;
            if (defaultControl)
              defaultControl.focus();
          });
          ensureTransitionEndEvent(
              innerContainer, MAX_SCREEN_TRANSITION_DURATION);
        } else {
          if (defaultControl)
            defaultControl.focus();
          chrome.send('loginVisible', ['oobe']);
        }
      }
      this.currentStep_ = nextStepIndex;

      // Call onAfterShow after currentStep_ so that the step can have a
      // post-set hook.
      invokePolymerMethod(newStep, 'onAfterShow', screenData);

      $('oobe').dispatchEvent(
          new CustomEvent('screenchanged', {detail: this.currentScreen.id}));
      chrome.send('updateCurrentScreen', [this.currentScreen.id]);
    },

    /**
     * Show screen of given screen id.
     * @param {Object} screen Screen params dict, e.g. {id: screenId, data: {}}.
     */
    showScreen: function(screen) {
      // Do not allow any other screen to clobber the device disabled screen.
      if (this.currentScreen.id == SCREEN_DEVICE_DISABLED)
        return;

      // Prevent initial GAIA signin load from interrupting the kiosk splash
      // screen.
      // TODO: remove this special case when a better fix is found for the race
      // condition. This if statement was introduced to fix http://b/113786350.
      if (this.currentScreen.id == SCREEN_APP_LAUNCH_SPLASH &&
          screen.id == SCREEN_GAIA_SIGNIN) {
        console.log(
            this.currentScreen.id +
            ' screen showing. Ignoring switch to Gaia screen.');
        return;
      }

      var screenId = screen.id;

      var data = screen.data;
      var index = this.getScreenIndex_(screenId);
      if (index >= 0)
        this.toggleStep_(index, data);
    },

    /**
     * Gets index of given screen id in screens_.
     * @param {string} screenId Id of the screen to look up.
     * @private
     */
    getScreenIndex_: function(screenId) {
      for (let i = 0; i < this.screens_.length; ++i) {
        if (this.screens_[i] == screenId)
          return i;
      }
      return -1;
    },

    /**
     * Register an oobe screen.
     * @param {Element} el Decorated screen element.
     * @param {DisplayManagerScreenAttributes} attributes
     */
    registerScreen: function(el, attributes) {
      var screenId = el.id;
      assert(screenId);
      assert(!this.screens_.includes(screenId), 'Duplicate screen ID.');

      this.screens_.push(screenId);
      this.screensAttributes_.push(attributes);

      if (el.updateOobeConfiguration && this.oobe_configuration_)
        el.updateOobeConfiguration(this.oobe_configuration_);
    },

    /**
     * Updates inner container size based on the size of the current screen and
     * other screens in the same group.
     * Should be executed on screen change / screen size change.
     * @param {!HTMLElement} screen Screen that is being shown.
     */
    updateScreenSize: function(screen) {
      if (!cr.isChromeOS) {
        // Have to reset any previously predefined screen size first
        // so that screen contents would define it instead.
        $('inner-container').style.height = '';
        $('inner-container').style.width = '';
        screen.style.width = '';
        screen.style.height = '';
      }

      $('outer-container')
          .classList.toggle(
              'fullscreen', screen.classList.contains('fullscreen'));

      var width = screen.getPreferredSize().width;
      var height = screen.getPreferredSize().height;

      if (!cr.isChromeOS) {
        if (screen.classList.contains('fullscreen')) {
          $('inner-container').style.height = '100%';
          $('inner-container').style.width = '100%';
        } else {
          $('inner-container').style.height = height + 'px';
          $('inner-container').style.width = width + 'px';
        }
        // This requires |screen| to have 'box-sizing: border-box'.
        screen.style.width = width + 'px';
        screen.style.height = height + 'px';
        screen.style.margin = 'auto';
      }
    },

    /**
     * Updates localized content of the screens like headers, buttons and links.
     * Should be executed on language change.
     */
    updateLocalizedContent_: function() {
      for (let i = 0; i < this.screens_.length; ++i) {
        let screenId = this.screens_[i];
        var screen = $(screenId);
        if (screen.updateLocalizedContent)
          screen.updateLocalizedContent();
      }
      var dynamicElements = document.getElementsByClassName('i18n-dynamic');
      for (var child of dynamicElements) {
        if (typeof (child.i18nUpdateLocale) === 'function') {
          child.i18nUpdateLocale();
        }
      }
      var isInTabletMode = loadTimeData.getBoolean('isInTabletMode');
      this.setTabletModeState_(isInTabletMode);

      var currentScreenId = this.screens_[this.currentStep_];
      var currentScreen = $(currentScreenId);
      this.updateScreenSize(currentScreen);
    },

    /**
     * Updates Oobe configuration for screens.
     * @param {!OobeTypes.OobeConfiguration} configuration OOBE configuration.
     */
    updateOobeConfiguration_: function(configuration) {
      this.oobe_configuration_ = configuration;
      for (let i = 0; i < this.screens_.length; ++i) {
        let screenId = this.screens_[i];
        var screen = $(screenId);
        if (screen.updateOobeConfiguration)
          screen.updateOobeConfiguration(configuration);
      }
    },

    /**
     * Updates "device in tablet mode" state when tablet mode is changed.
     * @param {Boolean} isInTabletMode True when in tablet mode.
     */
    setTabletModeState_: function(isInTabletMode) {
      for (let i = 0; i < this.screens_.length; ++i) {
        let screenId = this.screens_[i];
        var screen = $(screenId);
        if (screen.setTabletModeState)
          screen.setTabletModeState(isInTabletMode);
      }
    },

    /** Initializes demo mode start listener. */
    initializeDemoModeMultiTapListener: function() {
      if (this.displayType_ == DISPLAY_TYPE.OOBE) {
        this.demoModeStartListener_ =
            new MultiTapDetector($('outer-container'), 10, () => {
              let currentScreen = Oobe.getInstance().currentScreen;
              if (currentScreen.id === SCREEN_WELCOME) {
                currentScreen.onSetupDemoModeGesture();
              }
            });
      }
    },

    /**
     * Prepares screens to use in login display.
     */
    prepareForLoginDisplay_: function() {
      if (this.showingViewsLogin) {
        $('top-header-bar').hidden = true;
      }
    },

    /**
     * Called when window size changed. Notifies current screen about
     * change.
     * @private
     */
    onWindowResize_: function() {
      for (var i = 0, screenId; screenId = this.screens_[i]; ++i) {
        var screen = $(screenId);
        if (screen.onWindowResize)
          screen.onWindowResize();
      }
    },

    /**
     * Returns true if Oobe UI is shown.
     */
    isOobeUI: function() {
      return document.body.classList.contains('oobe-display');
    },

    /**
     * Sets or unsets given |className| for top-level container. Useful
     * for customizing #inner-container with CSS rules. All classes set
     * with with this method will be removed after screen change.
     * @param {string} className Class to toggle.
     * @param {boolean} enabled Whether class should be enabled or disabled.
     */
    toggleClass: function(className, enabled) {
      $('oobe').classList.toggle(className, enabled);
    },

    /**
     * Notifies the C++ handler in views login that the OOBE signin state has
     * been updated. This information is primarily used by the login shelf to
     * update button visibility state.
     * @param {number} state The state (see OOBE_UI_STATE) of the OOBE UI.
     */
    setOobeUIState: function(state) {
      chrome.send('updateOobeUIState', [state]);
    },

  };

  /**
   * Initializes display manager.
   */
  DisplayManager.initialize = function() {
    var givenDisplayType = DISPLAY_TYPE.UNKNOWN;
    if (document.documentElement.hasAttribute('screen')) {
      // Display type set in HTML property.
      givenDisplayType = document.documentElement.getAttribute('screen');
    } else {
      // Extracting display type from URL.
      givenDisplayType = window.location.pathname.substr(1);
    }
    var instance = Oobe.getInstance();
    Object.getOwnPropertyNames(DISPLAY_TYPE).forEach(function(type) {
      if (DISPLAY_TYPE[type] == givenDisplayType) {
        instance.displayType = givenDisplayType;
      }
    });
    if (instance.displayType == DISPLAY_TYPE.UNKNOWN) {
      console.error(
          'Unknown display type "' + givenDisplayType + '". Setting default.');
      instance.displayType = DISPLAY_TYPE.LOGIN;
    }

    instance.initializeDemoModeMultiTapListener();

    window.addEventListener('resize', instance.onWindowResize_.bind(instance));
  };

  /**
   * Returns offset (top, left) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} The offset (top, left).
   */
  DisplayManager.getOffset = function(element) {
    var x = 0;
    var y = 0;
    while (element && !isNaN(element.offsetLeft) && !isNaN(element.offsetTop)) {
      x += element.offsetLeft - element.scrollLeft;
      y += element.offsetTop - element.scrollTop;
      element = element.offsetParent;
    }
    return {top: y, left: x};
  };

  /**
   * Returns position (top, left, right, bottom) of the element.
   * @param {!Element} element HTML element.
   * @return {!Object} Element position (top, left, right, bottom).
   */
  DisplayManager.getPosition = function(element) {
    var offset = DisplayManager.getOffset(element);
    return {
      top: offset.top,
      right: window.innerWidth - element.offsetWidth - offset.left,
      bottom: window.innerHeight - element.offsetHeight - offset.top,
      left: offset.left
    };
  };

  /**
   * Shows signin UI.
   * @param {string} opt_email An optional email for signin UI.
   */
  DisplayManager.showSigninUI = function(opt_email) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;
    if (currentScreenId == SCREEN_GAIA_SIGNIN)
      Oobe.getInstance().setOobeUIState(OOBE_UI_STATE.GAIA_SIGNIN);
    chrome.send('showAddUser', [opt_email]);
  };

  /**
   * Resets sign-in input fields.
   * @param {boolean} forceOnline Whether online sign-in should be forced.
   *     If |forceOnline| is false previously used sign-in type will be used.
   */
  DisplayManager.resetSigninUI = function(forceOnline) {
    var currentScreenId = Oobe.getInstance().currentScreen.id;

    if ($(SCREEN_GAIA_SIGNIN)) {
      $(SCREEN_GAIA_SIGNIN)
          .reset(currentScreenId == SCREEN_GAIA_SIGNIN, forceOnline);
    }
  };

  /**
   * Creates a div element used to display error message in an error bubble.
   *
   * @param {string} message The error message.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   * @return {!HTMLElement} The error bubble content.
   */
  DisplayManager.createErrorElement_ = function(message, link, helpId) {
    var error = document.createElement('div');

    var messageDiv = document.createElement('div');
    messageDiv.className = 'error-message-bubble';
    messageDiv.textContent = message;
    error.appendChild(messageDiv);

    if (link) {
      messageDiv.classList.add('error-message-bubble-padding');

      var helpLink = document.createElement('a');
      helpLink.href = '#';
      helpLink.textContent = link;
      helpLink.addEventListener('click', function(e) {
        chrome.send('launchHelpApp', [helpId]);
        e.preventDefault();
      });
      error.appendChild(helpLink);
    }

    error.setAttribute('aria-live', 'assertive');
    return error;
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attemps tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  DisplayManager.showSignInError = function(
      loginAttempts, message, link, helpId) {
    var error = DisplayManager.createErrorElement_(message, link, helpId);

    var currentScreen = Oobe.getInstance().currentScreen;
    if (currentScreen && typeof currentScreen.showErrorBubble === 'function') {
      currentScreen.showErrorBubble(loginAttempts, error);
      this.errorMessageWasShownForTesting_ = true;
    }
  };

  /**
   * Clears error bubble.
   */
  DisplayManager.clearErrors = function() {
    $('bubble').hide();
    this.errorMessageWasShownForTesting_ = false;
  };

  /**
   * Sets text content for a div with |labelId|.
   * @param {string} labelId Id of the label div.
   * @param {string} labelText Text for the label.
   */
  DisplayManager.setLabelText = function(labelId, labelText) {
    $(labelId).textContent = labelText;
  };

  /**
   * Sets the text content of the enterprise info message and asset ID.
   * @param {string} messageText The message text.
   * @param {string} assetId The device asset ID.
   */
  DisplayManager.setEnterpriseInfo = function(messageText, assetId) {
    $('asset-id').textContent =
        ((assetId == '') ? '' :
                           loadTimeData.getStringF('assetIdLabel', assetId));
  };

  /**
   * Sets the text content of the Bluetooth device info message.
   * @param {string} bluetoothName The Bluetooth device name text.
   */
  DisplayManager.setBluetoothDeviceInfo = function(bluetoothName) {
    $('bluetooth-name').hidden = false;
    $('bluetooth-name').textContent = bluetoothName;
  };

  // Export
  return {
    DisplayManager: DisplayManager,
    invokePolymerMethod: invokePolymerMethod,
  };
});
