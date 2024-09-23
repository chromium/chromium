// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.js';
import 'chrome://resources/cr_elements/cr_collapse/cr_collapse.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './privacy_sandbox_dialog_app.html.js';
import {PrivacySandboxDialogBrowserProxy, PrivacySandboxPromptAction} from './privacy_sandbox_dialog_browser_proxy.js';

export interface PrivacySandboxDialogAppElement {
  $: {
    contentArea: HTMLElement,
    expandSection: HTMLElement,
  };
}

export class PrivacySandboxDialogAppElement extends PolymerElement {
  static get is() {
    return 'privacy-sandbox-dialog-app';
  }

  static get template() {
    return getTemplate();
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
  private canScrollClass_: string;
  private fitIntoDialogClass_: string;
  private didStartWithScrollbar_: boolean;

  override connectedCallback() {
    super.connectedCallback();

    afterNextRender(this, async () => {
      const proxy = PrivacySandboxDialogBrowserProxy.getInstance();
      // Prefer using |document.body.offsetHeight| instead of
      // |document.body.scrollHeight| as it returns the correct height of the
      // page even when the page zoom in Chrome is different than 100%.
      await proxy.resizeDialog(document.body.offsetHeight);

      // After the content was rendered at size it requires, toggle a class
      // to fit the content into dialog bounds.
      this.fitIntoDialogClass_ = 'fit-into-size';

      // After the layout is adjusted to fit into the dialog, save if the
      // dialog is scrollable and add a divider if needed.
      this.didStartWithScrollbar_ =
          this.$.contentArea.offsetHeight < this.$.contentArea.scrollHeight;
      this.canScrollClass_ = this.didStartWithScrollbar_ ? 'can-scroll' : '';

      proxy.showDialog();
    });

    window.addEventListener('keydown', event => {
      // Only notice dialog can be dismissed by pressing "Esc".
      if (event.key === 'Escape' && !this.isConsent_) {
        this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_DISMISS);
      }
    });
  }

  private onNoticeOpenSettings_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_OPEN_SETTINGS);
  }

  private onNoticeAcknowledge_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.NOTICE_ACKNOWLEDGE);
  }

  private onConsentAccepted_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.CONSENT_ACCEPTED);
  }

  private onConsentDeclined_() {
    this.promptActionOccurred(PrivacySandboxPromptAction.CONSENT_DECLINED);
  }

  private onLearnMoreExpandedChanged_(newVal: boolean, oldVal: boolean) {
    if (!oldVal && newVal) {
      this.promptActionOccurred(
          PrivacySandboxPromptAction.CONSENT_MORE_INFO_OPENED);
    }
    if (oldVal && !newVal) {
      this.promptActionOccurred(
          PrivacySandboxPromptAction.CONSENT_MORE_INFO_CLOSED);
    }
    // Show divider if the dialog was scrollable from the beginning or became
    // scrollable because the section is expanded. Otherwise, hide the
    // scrollbar to avoid animating it out. Without it, when the section is
    // collapsing, the scrollbar thumb would grow until it fills the track and
    // disappears.
    this.canScrollClass_ =
        this.didStartWithScrollbar_ || newVal ? 'can-scroll' : 'hide-scrollbar';

    // Wait for collapse section transition to complete 70%.
    const collapseElement = this.$.expandSection.querySelector('cr-collapse');
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

  private promptActionOccurred(action: PrivacySandboxPromptAction) {
    PrivacySandboxDialogBrowserProxy.getInstance().promptActionOccurred(action);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'privacy-sandbox-dialog-app': PrivacySandboxDialogAppElement;
  }
}

customElements.define(
    PrivacySandboxDialogAppElement.is, PrivacySandboxDialogAppElement);
