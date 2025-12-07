// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import 'chrome://resources/ash/common/cr_elements/cr_toggle/cr_toggle.js';
import 'chrome://chrome-signin/account_manager_shared.css.js';

import type {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
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

  override ready() {
    super.ready();
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
          link.addEventListener('click', handleClick as EventListener);
          link.addEventListener(
              'auxclick',
              // For middle-click, do the same things as Ctrl+click
              ((event: MouseEvent) => {
                if (event.button === 1) {
                  handleClick();
                }
              }) as EventListener);
        });

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

  private getWelcomeTitle_(): string {
    return loadTimeData.getStringF(
        'accountManagerDialogWelcomeTitle', loadTimeData.getString('userName'));
  }

  private getWelcomeBody_(): TrustedHTML {
    const welcomeBodyKey = 'accountManagerDialogWelcomeBody';
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
