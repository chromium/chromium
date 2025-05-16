// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-progress/paper-progress.js';
import '/components/oobe_cr_lottie.js';
import '/components/oobe_icons.html.js';
import '/components/common_styles/oobe_dialog_host_styles.css.js';
import '/components/dialogs/oobe_adaptive_dialog.js';

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {OobeAdaptiveDialog} from '/components/dialogs/oobe_adaptive_dialog.js';
import {I18nMixin} from 'chrome://resources/ash/common/cr_elements/i18n_mixin.js';

import {getTemplate} from './app.html.js';

const EXPECTED_LOAD_TIME_MILLISEC = 5000;

export interface FloatingWorkspace {
  $: {
    floatingDialog: OobeAdaptiveDialog,
  };
}

const FloatingWorkspaceBase = I18nMixin(PolymerElement);

export class FloatingWorkspace extends FloatingWorkspaceBase {
  static get is() {
    return 'floating-workspace' as const;
  }

  static get template(): HTMLTemplateElement {
    return getTemplate();
  }

  // Main string of the dialog. It is changed after
  // EXPECTED_LOAD_TIME_MILLISEC of time.
  private titleString: string|'';


  constructor() {
    super();
    this.titleString = this.i18n('floatingWorkspaceStartupDialogTitle');
  }

  override ready(): void {
    super.ready();

    // Needed to set the right size of <oobe-adaptive-dialog/> when the dialog
    // is shown and when the window resolution changes (e.g. switching to
    // landscape mode).
    window.addEventListener('orientationchange', () => {
      this.onWindowResolutionChange_();
    });
    window.addEventListener('resize', () => {
      this.onWindowResolutionChange_();
    });
    this.onWindowResolutionChange_();

    // Change title when floating workspace takes too long.
    setTimeout(this.onNoResponse.bind(this), EXPECTED_LOAD_TIME_MILLISEC);
    this.$.floatingDialog.onBeforeShow();
    this.$.floatingDialog.show();
  }

  private onNoResponse(): void {
    this.titleString =
        this.i18n('floatingWorkspaceStartupDialogLongResponseTitle');
  }

  private onWindowResolutionChange_(): void {
    if (!document.documentElement.hasAttribute('screen')) {
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-height-base', window.innerHeight + 'px');
      document.documentElement.style.setProperty(
          '--oobe-oobe-dialog-width-base', window.innerWidth + 'px');
      if (window.innerWidth > window.innerHeight) {
        document.documentElement.setAttribute('orientation', 'horizontal');
      } else {
        document.documentElement.setAttribute('orientation', 'vertical');
      }
    }
  }

  private onCancelButtonClick_(): void {
    chrome.send('dialogClose', ['stopRestoringSession']);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [FloatingWorkspace.is]: FloatingWorkspace;
  }
}

customElements.define(FloatingWorkspace.is, FloatingWorkspace);
