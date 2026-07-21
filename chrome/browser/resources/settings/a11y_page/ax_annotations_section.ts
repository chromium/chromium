// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-ax-annotations-section' is a section holding the toggle for main
 * node accessibility annotations. It appears on the accessibility page
 * (chrome://settings/accessibility) on Windows, macOS, and Linux.
 */

import '../controls/settings_toggle_button.js';

import type {AxAnnotationsBrowserProxy} from '/shared/settings/a11y_page/ax_annotations_browser_proxy.js';
import {AxAnnotationsBrowserProxyImpl, ScreenAiInstallStatus} from '/shared/settings/a11y_page/ax_annotations_browser_proxy.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {WebUiListenerMixinLit} from 'chrome://resources/cr_elements/web_ui_listener_mixin_lit.js';
import {assert, assertNotReached} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {loadTimeData} from '../i18n_setup.js';
import {getCss as getSettingsSharedCss} from '../settings_shared_lit.css.js';

import {getHtml} from './ax_annotations_section.html.js';

const SettingsAxAnnotationsSectionBaseElement =
    WebUiListenerMixinLit(I18nMixinLit(CrLitElement));

export class SettingsAxAnnotationsSectionElement extends
    SettingsAxAnnotationsSectionBaseElement {
  static get is() {
    return 'settings-ax-annotations-section' as const;
  }

  static override get styles() {
    return [
      getSettingsSharedCss(),
    ];
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * `screenAIProgress_` stores the downloading progress in percentage of
       * the ScreenAI library, which ranges from 0.0 to 100.0.
       */
      screenAIProgress_: {type: Number},

      /**
       * `screenAIStatus_` stores the ScreenAI library install state.
       */
      screenAIStatus_: {type: Number},
    };
  }

  private browserProxy_: AxAnnotationsBrowserProxy =
      AxAnnotationsBrowserProxyImpl.getInstance();

  protected accessor screenAIProgress_: number = 0;
  protected accessor screenAIStatus_: ScreenAiInstallStatus =
      ScreenAiInstallStatus.NOT_DOWNLOADED;

  override connectedCallback() {
    super.connectedCallback();

    assert(loadTimeData.getBoolean('mainNodeAnnotationsEnabled'));

    const updateScreenAIState = (screenAIState: ScreenAiInstallStatus) => {
      this.screenAIStatus_ = screenAIState;
    };
    this.browserProxy_.getScreenAiInstallState().then(updateScreenAIState);
    this.addWebUiListener('screen-ai-state-changed', updateScreenAIState);
    this.addWebUiListener(
        'screen-ai-downloading-progress-changed', (progress: number) => {
          this.screenAIProgress_ = progress;
        });
  }

  protected getMainNodeAnnotationsToggleSublabel_(): string {
    switch (this.screenAIStatus_) {
      case ScreenAiInstallStatus.DOWNLOADING:
        return this.screenAIProgress_ > 0 && this.screenAIProgress_ < 100 ?
            this.i18n(
                'mainNodeAnnotationsDownloadProgressLabel',
                this.screenAIProgress_) :
            this.i18n('mainNodeAnnotationsDownloadingLabel');
      case ScreenAiInstallStatus.DOWNLOAD_FAILED:
        return this.i18n('mainNodeAnnotationsDownloadErrorLabel');
      // Show the default subtitle if downloading is done.
      // fallthrough
      case ScreenAiInstallStatus.DOWNLOADED:
      case ScreenAiInstallStatus.NOT_DOWNLOADED:
        // No subtitle update, so show a generic subtitle describing main node
        // annotations.
        return this.i18n('mainNodeAnnotationsSubtitle');
      default:
        assertNotReached();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-ax-annotations-section': SettingsAxAnnotationsSectionElement;
  }
}

customElements.define(
    SettingsAxAnnotationsSectionElement.is,
    SettingsAxAnnotationsSectionElement);
