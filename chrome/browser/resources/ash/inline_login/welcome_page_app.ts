// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import './account_manager_shared.css.js';

import {getAccountAdditionOptionsFromJSON} from 'chrome://chrome-signin/arc_account_picker/arc_util.js';
import type {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {InlineLoginBrowserProxyImpl} from './inline_login_browser_proxy.js';
import {getTemplate} from './welcome_page_app.html.js';


export interface WelcomePageAppElement {
  $: {
    checkbox: CrCheckboxElement,
  };
}

export class WelcomePageAppElement extends PolymerElement {
  static get is() {
    return 'welcome-page-app';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* The value of the 'available in ARC' toggle.*/
      isAvailableInArc: {
        type: Boolean,
        notify: true,
      },

      /*
       * True if the 'Add account' flow is opened from ARC.
       * In this case we will hide the toggle and show different welcome
       * message.
       * @private
       */
      isArcFlow_: {
        type: Boolean,
        value: false,
      },

      /*
       * True if `kArcAccountRestrictions` feature is enabled.
       * @private
       */
      isArcAccountRestrictionsEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('isArcAccountRestrictionsEnabled');
        },
        readOnly: true,
      },
    };
  }

  private isAvailableInArc: boolean;
  private isArcFlow_: boolean;
  private isArcAccountRestrictionsEnabled_: boolean;

  override ready() {
    super.ready();

    if (this.isArcAccountRestrictionsEnabled_) {
      const options = getAccountAdditionOptionsFromJSON(
          InlineLoginBrowserProxyImpl.getInstance().getDialogArguments());
      if (!options) {
        // Options are not available during reauthentication.
        return;
      }

      // Set the default value.
      this.isAvailableInArc = options.isAvailableInArc;
      if (options.showArcAvailabilityPicker) {
        this.isArcFlow_ = true;
        assert(this.isAvailableInArc);
      }
    }

    this.setUpLinkCallbacks_();
  }

  isSkipCheckboxChecked(): boolean {
    return !!this.$.checkbox && this.$.checkbox.checked;
  }

  private setUpLinkCallbacks_() {
    [this.shadowRoot!.querySelector('#osSettingsLink'),
     this.shadowRoot!.querySelector('#appsSettingsLink'),
     this.shadowRoot!.querySelector('#newPersonLink')]
        .filter(link => !!link)
        .forEach(link => {
          const handleClick = () =>
              this.dispatchEvent(new CustomEvent('opened-new-window'));
          link!.addEventListener('click', handleClick as EventListener);
          link!.addEventListener(
              'auxclick',
              // For middle-click, do the same things as Ctrl+click
              ((event: MouseEvent) => {
                if (event.button === 1) {
                  handleClick();
                }
              }) as EventListener);
        });

    if (this.isArcAccountRestrictionsEnabled_) {
      const guestModeLink = this.shadowRoot!.querySelector('#guestModeLink');
      if (guestModeLink) {
        const handleClick = (event: MouseEvent) => {
          event.preventDefault();
          this.openGuestLink_();
        };
        guestModeLink.addEventListener('click', handleClick as EventListener);
        guestModeLink.addEventListener(
            'auxclick',
            // For middle-click, do the same things as Ctrl+click
            ((event: MouseEvent) => {
              if (event.button === 1) {
                handleClick(event);
              }
            }) as EventListener);
      }
    } else {
      const incognitoLink = this.shadowRoot!.querySelector('#incognitoLink');
      if (incognitoLink) {
        const handleClick = (event: MouseEvent) => {
          event.preventDefault();
          this.openIncognitoLink_();
        };
        incognitoLink.addEventListener('click', handleClick as EventListener);
        incognitoLink.addEventListener(
            'auxclick',
            // For middle-click, do the same things as Ctrl+click
            ((event: MouseEvent) => {
              if (event.button === 1) {
                handleClick(event);
              }
            }) as EventListener);
      }
    }
  }

  private isArcToggleVisible_(): boolean {
    return this.isArcAccountRestrictionsEnabled_ && !this.isArcFlow_;
  }

  private getWelcomeTitle_(): string {
    return loadTimeData.getStringF(
        'accountManagerDialogWelcomeTitle', loadTimeData.getString('userName'));
  }

  private getWelcomeBody_(): TrustedHTML {
    const welcomeBodyKey =
        (this.isArcAccountRestrictionsEnabled_ && this.isArcFlow_) ?
        'accountManagerDialogWelcomeBodyArc' :
        'accountManagerDialogWelcomeBody';
    return sanitizeInnerHtml(loadTimeData.getString(welcomeBodyKey), {
      attrs: ['id'],
    });
  }

  private openIncognitoLink_() {
    InlineLoginBrowserProxyImpl.getInstance().showIncognito();
    // `showIncognito` will close the dialog.
  }

  private openGuestLink_() {
    InlineLoginBrowserProxyImpl.getInstance().openGuestWindow();
    // `openGuestWindow` will close the dialog.
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'welcome-page-app': WelcomePageAppElement;
  }
}

customElements.define(WelcomePageAppElement.is, WelcomePageAppElement);
