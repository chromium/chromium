// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="../../../../ui/login/screen.js">
// <include src="../../../../ui/login/bubble.js">
// <include src="../../../../ui/login/login_ui_tools.js">
// <include src="../../../../ui/login/display_manager.js">
// <include src="../../../../ui/login/account_picker/screen_account_picker.js">
// <include src="../../../../ui/login/account_picker/user_pod_row.js">


cr.define('cr.ui', function() {
  const DisplayManager = cr.ui.login.DisplayManager;

  /**
   * Maximum possible height of the #login-header-bar, including the padding
   * and the border.
   * @type {number}
   */
  const MAX_LOGIN_HEADER_BAR_HEIGHT = 57;

  /**
   * Manages initialization of screens, transitions, and error messages.
   * @constructor
   * @extends {DisplayManager}
   */
  function UserManager() {}

  cr.addSingletonGetter(UserManager);

  UserManager.prototype = {
    __proto__: DisplayManager.prototype,

    /**
     * Indicates that this is the Material Design Desktop User Manager.
     * @type {boolean}
     */
    newDesktopUserManager: true,

    /**
     * Indicates whether the user pods page is visible.
     * @type {boolean}
     */
    userPodsPageVisible: true,

    /**
     * @override
     * Overrides clientAreaSize in DisplayManager. When a new profile is created
     * the user pods page may not be visible yet, so user-pods cannot be
     * placed correctly. Therefore, we use dimensions of the #animated-pages.
     * @type {{width: number, height: number}}
     */
    get clientAreaSize() {
      const userManagerPages = document.querySelector('user-manager-pages');
      const width = userManagerPages.offsetWidth;
      // Deduct the maximum possible height of the #login-header-bar from the
      // height of #animated-pages. Result is the remaining visible height.
      const height =
          userManagerPages.offsetHeight - MAX_LOGIN_HEADER_BAR_HEIGHT;
      return {width: width, height: height};
    }
  };

  /**
   * Listens for the page change event to see if the user pods page is visible.
   * Updates userPodsPageVisible property accordingly and if the page is visible
   * re-arranges the user pods.
   * @param {!Event} event The event containing ID of the selected page.
   */
  UserManager.onPageChanged_ = function(event) {
    const userPodsPageVisible = event.detail.page == 'user-pods-page';
    cr.ui.UserManager.getInstance().userPodsPageVisible = userPodsPageVisible;
    if (userPodsPageVisible) {
      $('pod-row').rebuildPods();
    }
  };

  /**
   * Initializes the UserManager.
   */
  UserManager.initialize = function() {
    cr.ui.login.DisplayManager.initialize();
    login.AccountPickerScreen.register();
    cr.ui.Bubble.decorate($('bubble'));

    signin.ProfileBrowserProxyImpl.getInstance().initializeUserManager(
        window.location.hash);
    cr.addWebUIListener('show-error-dialog', cr.ui.UserManager.showErrorDialog);
  };

  /**
   * Shows the given screen.
   * @param {boolean} showGuest True if 'Browse as Guest' button should be
   *     displayed.
   * @param {boolean} showAddPerson True if 'Add Person' button should be
   *     displayed.
   */
  UserManager.showUserManagerScreen = function(showGuest, showAddPerson) {
    UserManager.getInstance().showScreen({id: 'account-picker', data: {}});
    // Hide control options if the user does not have the right permissions.
    const controlBar = document.querySelector('control-bar');
    controlBar.showGuest = showGuest;
    controlBar.showAddPerson = showAddPerson;

    // Disable the context menu, as the Print/Inspect element items don't
    // make sense when displayed as a widget.
    document.addEventListener('contextmenu', function(e) {
      e.preventDefault();
    });

    if (window.location.hash == '#tutorial') {
      document.querySelector('user-manager-tutorial').startTutorial();
    } else if (window.location.hash == '#create-user') {
      document.querySelector('user-manager-pages')
          .setSelectedPage('create-user-page');
    }
  };

  /**
   * Open a new browser for the given profile.
   * @param {string} profilePath The profile's path.
   */
  UserManager.launchUser = function(profilePath) {
    signin.ProfileBrowserProxyImpl.getInstance().launchUser(profilePath);
  };

  /**
   * Disables signin UI.
   */
  UserManager.disableSigninUI = function() {
    DisplayManager.disableSigninUI();
  };

  /**
   * Shows signin UI.
   * @param {string=} opt_email An optional email for signin UI.
   */
  UserManager.showSigninUI = function(opt_email) {
    DisplayManager.showSigninUI(opt_email);
  };

  /**
   * Shows sign-in error bubble.
   * @param {number} loginAttempts Number of login attempts tried.
   * @param {string} message Error message to show.
   * @param {string} link Text to use for help link.
   * @param {number} helpId Help topic Id associated with help link.
   */
  UserManager.showSignInError = function(loginAttempts, message, link, helpId) {
    DisplayManager.showSignInError(loginAttempts, message, link, helpId);
  };

  /**
   * Clears error bubble as well as optional menus that could be open.
   */
  UserManager.clearErrors = function() {
    DisplayManager.clearErrors();
  };

  /**
   * Shows the error dialog populated with the given message.
   * @param {string} message Error message to show.
   */
  UserManager.showErrorDialog = function(message) {
    document.querySelector('error-dialog').show(message);
  };

  // Export
  return {UserManager: UserManager};
});

// Alias to Oobe for use in src/ui/login/account_picker/user_pod_row.js
const Oobe = cr.ui.UserManager;

// Allow selection events on components with editable text (password field)
// bug (http://code.google.com/p/chromium/issues/detail?id=125863)
disableTextSelectAndDrag(function(e) {
  const src = e.target;
  return src instanceof HTMLTextAreaElement ||
      src instanceof HTMLInputElement && /text|password|search/.test(src.type);
});

document.addEventListener('DOMContentLoaded', cr.ui.UserManager.initialize);

document.addEventListener('change-page', cr.ui.UserManager.onPageChanged_);
