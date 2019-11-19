// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Active Directory password change screen implementation.
 */
login.createScreen(
    'ActiveDirectoryPasswordChangeScreen', 'ad-password-change', function() {
      /**
       * Horizontal padding for the error bubble.
       * @type {number}
       * @const
       */
      var BUBBLE_HORIZONTAL_PADDING = 65;

      /**
       * Vertical padding for the error bubble.
       * @type {number}
       * @const
       */
      var BUBBLE_VERTICAL_PADDING = -144;

      return {
        EXTERNAL_API: [],

        adPasswordChanged_: null,

        /** @override */
        decorate: function() {
          this.adPasswordChanged_ = $('active-directory-password-change');
          this.adPasswordChanged_.addEventListener(
              'cancel', this.cancel.bind(this));

          this.adPasswordChanged_.addEventListener(
              'authCompleted', function(e) {
                chrome.send('completeActiveDirectoryPasswordChange', [
                  e.detail.username, e.detail.oldPassword, e.detail.newPassword
                ]);
              });
        },

        /**
         * Cancels password changing and drops the user back to the login
         * screen.
         */
        cancel: function() {
          chrome.send('cancelActiveDirectoryPasswordChange');
          Oobe.showUserPods();
        },

        /**
         * @override
         * Event handler that is invoked just before the frame is shown.
         * @param {Object} data Screen init payload
         */
        onBeforeShow: function(data) {
          // Active Directory password change screen is similar to Active
          // Directory login screen. So we restore bottom bar controls.
          this.adPasswordChanged_.reset();
          if ('username' in data)
            this.adPasswordChanged_.username = data.username;
          if ('error' in data)
            this.adPasswordChanged_.setInvalid(data.error);
        },

        /**
         * Shows sign-in error bubble.
         * @param {number} loginAttempts Number of login attemps tried.
         * @param {HTMLElement} content Content to show in bubble.
         */
        showErrorBubble: function(loginAttempts, error) {
          $('bubble').showContentForElement(
              $('ad-password-change'), cr.ui.Bubble.Attachment.BOTTOM, error,
              BUBBLE_HORIZONTAL_PADDING, BUBBLE_VERTICAL_PADDING);
        },

        /**
         * Updates localized content of the screen that is not updated via
         * template.
         */
        updateLocalizedContent: function() {
          $('active-directory-password-change').i18nUpdateLocale();
        },

      };
    });
