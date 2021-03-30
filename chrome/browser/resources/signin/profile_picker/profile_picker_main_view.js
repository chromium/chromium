// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './icons.js';
import './profile_card.js';
import './profile_picker_shared_css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl, ProfileState} from './manage_profiles_browser_proxy.js';
import {navigateTo, NavigationBehavior, Routes} from './navigation_behavior.js';
import {isGuestModeEnabled, isProfileCreationAllowed} from './policy_helper.js';

Polymer({
  is: 'profile-picker-main-view',

  _template: html`{__html_template__}`,

  behaviors: [WebUIListenerBehavior, NavigationBehavior],

  properties: {
    /**
     * Profiles list supplied by ManageProfilesBrowserProxy.
     * @type {!Array<!ProfileState>}
     */
    profilesList_: {
      type: Object,
      value: () => [],
    },

    /** @private */
    profilesListLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hideAskOnStartup_: {
      type: Boolean,
      value: true,
      computed: 'computeHideAskOnStartup_(profilesList_.length)',

    },

    /** @private */
    askOnStartup_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('askOnStartup');
      }
    },

    /** @private */
    disableAskOnStartup_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('disableAskOnStartup');
      }
    },
  },

  /** @private {?ManageProfilesBrowserProxy} */
  manageProfilesBrowserProxy_: null,

  /** @type {ResizeObserver} used to observer size changes to this element */
  resizeObserver_: null,

  /** @override */
  ready() {
    if (!isGuestModeEnabled()) {
      this.$.browseAsGuestButton.style.display = 'none';
    }

    if (!isProfileCreationAllowed()) {
      this.$.addProfile.style.display = 'none';
    }

    this.manageProfilesBrowserProxy_ =
        ManageProfilesBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.addResizeObserver_();
    this.addWebUIListener(
        'profiles-list-changed', this.handleProfilesListChanged_.bind(this));
    this.addWebUIListener(
        'profile-removed', this.handleProfileRemoved_.bind(this));
    this.manageProfilesBrowserProxy_.initializeMainView();
  },

  /** @override */
  detached() {
    this.resizeObserver_.disconnect();
  },

  /** @private */
  addResizeObserver_() {
    this.resizeObserver_ = new ResizeObserver(() => {
      const profileContainer =
          /** @type {!HTMLDivElement} */ (this.$$('.profiles-container'));
      if (profileContainer.scrollHeight > profileContainer.clientHeight) {
        this.$$('.footer').classList.add('division-line');
      } else {
        this.$$('.footer').classList.remove('division-line');
      }
    });
    this.resizeObserver_.observe(
        /** @type {!HTMLDivElement} */ (this.$$('.profiles-container')));
  },

  /** @private */
  onProductLogoTap_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  },

  /**
   * Handler for when the profiles list are updated.
   * @param {!Array<!ProfileState>} profilesList
   * @private
   */
  handleProfilesListChanged_(profilesList) {
    this.profilesListLoaded_ = true;
    this.profilesList_ = profilesList;
  },

  /**
   * Called when the user modifies 'Ask on startup' preference.
   * @private
   */
  onAskOnStartupChangedByUser_() {
    if (this.hideAskOnStartup_) {
      return;
    }

    this.manageProfilesBrowserProxy_.askOnStartupChanged(this.askOnStartup_);
  },

  /** @private */
  onAddProfileClick_() {
    if (!isProfileCreationAllowed()) {
      return;
    }
    chrome.metricsPrivate.recordUserAction('ProfilePicker_AddClicked');
    navigateTo(Routes.NEW_PROFILE);
  },

  /** @private */
  onLaunchGuestProfileClick_() {
    if (!isGuestModeEnabled()) {
      return;
    }
    this.manageProfilesBrowserProxy_.launchGuestProfile();
  },

  /** @private */
  handleProfileRemoved_(profilePath) {
    for (let i = 0; i < this.profilesList_.length; i += 1) {
      if (this.profilesList_[i].profilePath === profilePath) {
        // TODO(crbug.com/1063856): Add animation.
        this.splice('profilesList_', i, 1);
        break;
      }
    }
  },

  /**
   * @return boolean
   * @private
   */
  computeHideAskOnStartup_() {
    return this.disableAskOnStartup_ || !this.profilesList_ ||
        this.profilesList_.length < 2;
  },
});
