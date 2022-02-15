// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PrivacySandboxDialogAction, PrivacySandboxDialogBrowserProxy} from './privacy_sandbox_dialog_browser_proxy.js';

const PrivacySandboxDialogAppElementBase = PolymerElement;

export interface PrivacySandboxDialogAppElement {
  $: {contentArea: HTMLElement; expandSection: HTMLElement;};
}

export class PrivacySandboxDialogAppElement extends
    PrivacySandboxDialogAppElementBase {
  static get is() {
    return 'privacy-sandbox-dialog-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      expanded_: {
        type: Boolean,
        observer: 'onLearnMoreExpandedChanged_',
      },
      isConsent_: {
        type: Boolean,
        value: () => {
          return loadTimeData.getBoolean('isConsent');
        },
      },
      canScrollClass_: String,
      fitIntoDialogClass_: String,
    };
  }

  private expanded_: boolean;
  private isConsent_: boolean;
  private canScrollClass_: String;
  private fitIntoDialogClass_: String;

  connectedCallback() {
    super.connectedCallback();

    afterNextRender(this, () => {
      const proxy = PrivacySandboxDialogBrowserProxy.getInstance();
      // Prefer using |document.body.offsetHeight| instead of
      // |document.body.scrollHeight| as it returns the correct height of the
      // page even when the page zoom in Chrome is different than 100%.
      proxy.resizeDialog(document.body.offsetHeight);

      // After the content was rendered at size it requires, toggle a class
      // to fit the content into dialog bounds.
      this.fitIntoDialogClass_ = 'fit-into-size';
    });
  }

  private onNoticeOpenSettings_() {
    this.dialogActionOccurred(PrivacySandboxDialogAction.NOTICE_OPEN_SETTINGS);
  }

  private onNoticeAcknowledge_() {
    this.dialogActionOccurred(PrivacySandboxDialogAction.NOTICE_ACKNOWLEDGE);
  }

  private onConsentAccepted_() {
    this.dialogActionOccurred(PrivacySandboxDialogAction.CONSENT_ACCEPTED);
  }

  private onConsentDeclined_() {
    this.dialogActionOccurred(PrivacySandboxDialogAction.CONSENT_DECLINED);
  }

  private onLearnMoreExpandedChanged_(newVal: boolean, oldVal: boolean) {
    if (!oldVal && newVal) {
      this.dialogActionOccurred(
          PrivacySandboxDialogAction.CONSENT_MORE_INFO_OPENED);
    }
    if (oldVal && !newVal) {
      this.dialogActionOccurred(
          PrivacySandboxDialogAction.CONSENT_MORE_INFO_CLOSED);
    }
    // TODO(crbug.com/1286276): Add showing the divider if the dialog starts
    // scrollable.
    this.canScrollClass_ = newVal ? 'can-scroll' : '';

    // Wait for collapse section transition to complete 70%.
    const collapseElement = this.$.expandSection.querySelector('iron-collapse');
    if (collapseElement) {
      const computedStyle = window.getComputedStyle(collapseElement);
      const duration = parseFloat(computedStyle.getPropertyValue(
          '--iron-collapse-transition-duration'));
      setTimeout(() => {
        // ...and scroll the content area up to make the section content
        // visible.
        const rect = this.$.expandSection.getBoundingClientRect();
        this.$.contentArea.scrollTo({top: rect.top, behavior: 'smooth'});
      }, duration * 0.7);
    }
  }

  private dialogActionOccurred(action: PrivacySandboxDialogAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().dialogActionOccurred(action);
  }
}

customElements.define(
    PrivacySandboxDialogAppElement.is, PrivacySandboxDialogAppElement);
