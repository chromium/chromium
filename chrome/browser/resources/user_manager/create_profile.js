// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'create-profile' is a page that contains controls for creating
 * a profile, including choosing a name, and an avatar.
 */

Polymer({
  is: 'create-profile',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The current profile name.
     * @private {string}
     */
    profileName_: {type: String, value: ''},

    /**
     * The list of available profile icon Urls and labels.
     * @private {!Array<!AvatarIcon>}
     */
    availableIcons_: {
      type: Array,
      value: function() {
        return [];
      }
    },

    /**
     * The currently selected profile avatar, if any.
     * @private {?AvatarIcon}
     */
    selectedAvatar_: Object,

    /**
     * True if a profile is being created or imported.
     * @private {boolean}
     */
    createInProgress_: {type: Boolean, value: false},

    /**
     * True if the error/warning message is displaying.
     * @private {boolean}
     */
    isMessageVisble_: {type: Boolean, value: false},

    /**
     * The current error/warning message.
     * @private {string}
     */
    message_: {type: String, value: ''},

    /**
     * if true, a desktop shortcut will be created for the new profile.
     * @private {boolean}
     */
    createShortcut_: {type: Boolean, value: true},

    /** @private {!signin.ProfileBrowserProxy} */
    browserProxy_: Object,

    /**
     * True if the profile shortcuts feature is enabled.
     * @private
     */
    isProfileShortcutsEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('profileShortcutsEnabled');
      },
      readOnly: true
    },

    /**
     * True if the force sign in policy is enabled.
     * @private {boolean}
     */
    isForceSigninEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isForceSigninEnabled');
      },
    },
  },

  /** @override */
  created: function() {
    this.browserProxy_ = signin.ProfileBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'create-profile-success', this.handleSuccess_.bind(this));
    this.addWebUIListener(
        'create-profile-warning', this.handleMessage_.bind(this));
    this.addWebUIListener(
        'create-profile-error', this.handleMessage_.bind(this));
    this.addWebUIListener('profile-icons-received', icons => {
      this.availableIcons_ = icons;
    });

    this.browserProxy_.getAvailableIcons();
  },

  /** @override */
  attached: function() {
    // cr-input's focusable element isn't defined until after it's attached.
    Polymer.RenderStatus.afterNextRender(this, () => this.$.nameInput.focus());
  },

  /**
   * Handles tap events from:
   * - links within dynamic warning/error messages pushed from the browser.
   * @param {!Event} event
   * @private
   */
  onTap_: function(event) {
    const element = Polymer.dom(event).rootTarget;

    if (element.id == 'sign-in-to-chrome') {
      this.browserProxy_.openUrlInLastActiveProfileBrowser(element.href);
      event.preventDefault();
    } else if (element.id == 'reauth') {
      const elementData = /** @type {{userEmail: string}} */ (element.dataset);
      this.browserProxy_.authenticateCustodian(elementData.userEmail);
      this.hideMessage_();
      event.preventDefault();
    }
  },

  /**
   * Handler for the 'Save' button tap event.
   * @param {!Event} event
   * @private
   */
  onSaveTap_: function(event) {
    this.createProfile_();
  },

  /**
   * Creates the new profile.
   * @private
   */
  createProfile_: function() {
    this.hideMessage_();
    this.createInProgress_ = true;
    const createShortcut =
        this.isProfileShortcutsEnabled_ && this.createShortcut_;
    // Select the 1st avatar if none selected.
    const selectedAvatar = this.selectedAvatar_ || this.availableIcons_[0];
    this.browserProxy_.createProfile(
        this.profileName_, selectedAvatar.url, createShortcut);
  },

  /**
   * Handler for the 'Cancel' button tap event.
   * @param {!Event} event
   * @private
   */
  onCancelTap_: function(event) {
    this.fire('change-page', {page: 'user-pods-page'});
  },

  /**
   * Handles profile create/import success message pushed by the browser.
   * @param {!ProfileInfo} profileInfo Details of the created/imported profile.
   * @private
   */
  handleSuccess_: function(profileInfo) {
    this.createInProgress_ = false;
    this.fire('change-page', {page: 'user-pods-page'});
  },

  /**
   * Hides the warning/error message.
   * @private
   */
  hideMessage_: function() {
    this.isMessageVisble_ = false;
  },

  /**
   * Handles warning/error messages when a profile is being created/imported.
   * @param {*} message An HTML warning/error message.
   * @private
   */
  handleMessage_: function(message) {
    this.createInProgress_ = false;
    this.message_ = '' + message;
    this.isMessageVisble_ = true;
  },

  /**
   * Returns a translated message that contains link elements with the 'id'
   * attribute.
   * @param {string} id The ID of the string to translate.
   * @private
   */
  i18nAllowIDAttr_: function(id) {
    const opts = {
      'attrs': {
        'id': function(node, value) {
          return node.tagName == 'A';
        }
      }
    };

    return this.i18nAdvanced(id, opts);
  },

  /**
   * Computed binding determining whether the paper-spinner is active.
   * @param {boolean} createInProgress Is create in progress?
   * @return {boolean}
   * @private
   */
  isSpinnerActive_: function(createInProgress) {
    return createInProgress;
  },

  /**
   * Computed binding determining whether 'Save' button is disabled.
   * @param {boolean} createInProgress Is create in progress?
   * @param {string} profileName Profile Name.
   * @return {boolean}
   * @private
   */
  isSaveDisabled_: function(createInProgress, profileName) {
    /** @type {CrInputElement} */
    const nameInput = this.$.nameInput;
    return createInProgress || !profileName || !nameInput.validate();
  },
});
