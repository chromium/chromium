// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';
import '//resources/cr_elements/cr_button/cr_button.js';

import {assert, assertNotReached} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import {OpenWindowProxyImpl} from 'chrome://resources/js/open_window_proxy.js';

import {getCss} from './error_page.css.js';
import {getHtml} from './error_page.html.js';
import {SkillsManagementAction, SkillsManagementPage} from './skill_metrics.mojom-webui.js';
import {SkillsPageBrowserProxy} from './skills_page_browser_proxy.js';

export enum ErrorType {
  GLIC_NOT_ENABLED = 'glic-not-enabled',
  SKILLS_DISABLED = 'skills-disabled',
  NO_SEARCH_RESULTS = 'no-search-results',
  REMOTE_AUTHORITY_UNREACHABLE = 'remote-authority-unreachable',
}

export class ErrorPageElement extends CrLitElement {
  static get is() {
    return 'error-page';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      errorType: {type: String},
    };
  }
  accessor errorType: ErrorType = ErrorType.GLIC_NOT_ENABLED;
  // In the V2 path, we will not use the V1 BrowserProxy.
  private proxy_?: SkillsPageBrowserProxy;

  private getProxy_(): SkillsPageBrowserProxy {
    assert(!loadTimeData.getBoolean('isSkillsWebViewV2Enabled'));
    if (!this.proxy_) {
      this.proxy_ = SkillsPageBrowserProxy.getInstance();
    }
    return this.proxy_;
  }

  override connectedCallback() {
    super.connectedCallback();

    if (!loadTimeData.getBoolean('isSkillsWebViewV2Enabled') &&
        (this.isGlicDisabled_() || this.isSkillsDisabled_())) {
      this.getProxy_().handler.recordSkillsManagementAction(
          SkillsManagementPage.kErrorPage, SkillsManagementAction.kPageOpened);
    }
  }

  protected isGlicDisabled_(): boolean {
    return this.errorType === ErrorType.GLIC_NOT_ENABLED;
  }

  protected isSkillsDisabled_(): boolean {
    return this.errorType === ErrorType.SKILLS_DISABLED;
  }

  protected isRemoteAuthorityUnreachable_(): boolean {
    return this.errorType === ErrorType.REMOTE_AUTHORITY_UNREACHABLE;
  }

  protected errorTitle(): string {
    switch (this.errorType) {
      case ErrorType.GLIC_NOT_ENABLED:
      case ErrorType.SKILLS_DISABLED:
      case ErrorType.REMOTE_AUTHORITY_UNREACHABLE:
        return loadTimeData.getString('errorPageTitle');
      case ErrorType.NO_SEARCH_RESULTS:
        return loadTimeData.getString('noSearchResultsTitle');
      default:
        assertNotReached();
    }
  }

  protected errorDescription(): string {
    switch (this.errorType) {
      case ErrorType.GLIC_NOT_ENABLED:
        return loadTimeData.getString('errorPageDescription');
      case ErrorType.SKILLS_DISABLED:
        return loadTimeData.getString('disabledErrorPageDescription');
      case ErrorType.NO_SEARCH_RESULTS:
        return loadTimeData.getString('noSearchResultsDescription');
      case ErrorType.REMOTE_AUTHORITY_UNREACHABLE:
        return loadTimeData.getString('remoteAuthorityUnreachableDescription');
      default:
        assertNotReached();
    }
  }

  protected onGoToSettingsClick_() {
    OpenWindowProxyImpl.getInstance().openUrl('chrome://settings/ai/skills');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'error-page': ErrorPageElement;
  }
}

customElements.define(ErrorPageElement.is, ErrorPageElement);
