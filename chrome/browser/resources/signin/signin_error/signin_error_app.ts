// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './strings.m.js';
import './signin_shared.css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './signin_error_app.html.js';


const SigninErrorAppElementBase = WebUiListenerMixin(PolymerElement);

class SigninErrorAppElement extends SigninErrorAppElementBase {
  static get is() {
    return 'signin-error-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      fromProfilePicker_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('fromProfilePicker'),
      },

      switchButtonUnavailable_: {
        type: Boolean,
        value: false,
      },

      hideNormalError_: {
        type: Boolean,
        value: () => loadTimeData.getString('signinErrorMessage').length === 0,
      },

      /**
       * An array of booleans indicating whether profile blocking messages
       * should be hidden. Position 0 corresponds to the
       * #profile-blocking-error-message container, and subsequent positions
       * correspond to each of the 3 related messages respectively.
       */
      hideProfileBlockingErrors_: {
        type: Array,
        value: function() {
          const hide = [
            'profileBlockedMessage',
            'profileBlockedAddPersonSuggestion',
            'profileBlockedRemoveProfileSuggestion',
          ].map(id => loadTimeData.getString(id).length === 0);

          // Hide the container itself if all of each children are also hidden.
          hide.unshift(hide.every(hideEntry => hideEntry));

          return hide;
        },
      },
    };
  }

  private fromProfilePicker_: boolean;
  private switchButtonUnavailable_: boolean;
  private hideNormalError_: boolean;
  private hideProfileBlockingErrors_: boolean[];

  override connectedCallback() {
    super.connectedCallback();

    this.addWebUiListener('switch-button-unavailable', () => {
      this.switchButtonUnavailable_ = true;
      // Move focus to the only displayed button in this case.
      (this.shadowRoot!.querySelector('#confirmButton') as HTMLElement).focus();
    });
  }

  private onConfirm_() {
    chrome.send('confirm');
  }

  private onSwitchToExistingProfile_() {
    chrome.send('switchToExistingProfile');
  }

  private onLearnMore_() {
    chrome.send('learnMore');
  }
}

customElements.define(SigninErrorAppElement.is, SigninErrorAppElement);
