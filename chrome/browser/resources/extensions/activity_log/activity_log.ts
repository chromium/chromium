// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import './activity_log_stream.js';
import './activity_log_history.js';
import '/strings.m.js';

import {NONE_SELECTED} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {I18nMixinLit} from 'chrome://resources/cr_elements/i18n_mixin_lit.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import {navigation, Page} from '../navigation_helper.js';

import {getCss} from './activity_log.css.js';
import {getHtml} from './activity_log.html.js';
import type {ActivityLogDelegate} from './activity_log_history.js';
import {DummyActivityLogDelegate} from './activity_log_history.js';

/**
 * Subpages/views for the activity log. HISTORY shows extension activities
 * fetched from the activity log database with some fields such as args
 * omitted. STREAM displays extension activities in a more verbose format in
 * real time. NONE is used when user is away from the page.
 */
const enum ActivityLogSubpage {
  HISTORY = 0,
  STREAM = 1,
}

type MaybeActivityLogSubpage = ActivityLogSubpage|typeof NONE_SELECTED;


/**
 * A struct used as a placeholder for chrome.developerPrivate.ExtensionInfo
 * for this component if the extensionId from the URL does not correspond to
 * installed extension.
 */
export interface ActivityLogExtensionPlaceholder {
  id: string;
  isPlaceholder: boolean;
}

export interface ExtensionsActivityLogElement {
  $: {
    closeButton: HTMLElement,
    tabs: CrTabsElement,
  };
}

const ExtensionsActivityLogElementBase = I18nMixinLit(CrLitElement);

export class ExtensionsActivityLogElement extends
    ExtensionsActivityLogElementBase {
  static get is() {
    return 'extensions-activity-log';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       */
      extensionInfo: {type: Object},

      delegate: {type: Object},

      selectedSubpage_: {type: Number},

      tabNames_: {type: Array},
    };
  }

  extensionInfo: chrome.developerPrivate.ExtensionInfo|
      ActivityLogExtensionPlaceholder = {
    isPlaceholder: true,
    id: '',
  };
  delegate: ActivityLogDelegate = new DummyActivityLogDelegate();
  protected selectedSubpage_: MaybeActivityLogSubpage = NONE_SELECTED;
  protected tabNames_: string[] = [
    loadTimeData.getString('activityLogHistoryTabHeading'),
    loadTimeData.getString('activityLogStreamTabHeading'),
  ];

  override firstUpdated() {
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('selectedSubpage_')) {
      let oldValue = changedPrivateProperties.get('selectedSubpage_');
      if (oldValue === undefined) {
        oldValue = NONE_SELECTED;
      }
      this.onSelectedSubpageChanged_(
          this.selectedSubpage_, oldValue as MaybeActivityLogSubpage);
    }
  }

  protected isPlaceholder_(): boolean {
    return !!(this.extensionInfo as ActivityLogExtensionPlaceholder)
                 .isPlaceholder;
  }

  protected getExtensionIconUrl_(): string {
    if (this.isPlaceholder_()) {
      return '';
    }
    return (this.extensionInfo as chrome.developerPrivate.ExtensionInfo)
        .iconUrl;
  }

  /**
   * Focuses the back button when page is loaded and set the activie view to
   * be HISTORY when we navigate to the page.
   */
  private async onViewEnterStart_() {
    this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
    await this.updateComplete;
    focusWithoutInk(this.$.closeButton);
  }

  /**
   * Set |selectedSubpage_| to NONE_SELECTED to remove the active view from the
   * DOM.
   */
  private onViewExitFinish_() {
    this.selectedSubpage_ = NONE_SELECTED;
    // clear the stream if the user is exiting the activity log page.
    const activityLogStream =
        this.shadowRoot!.querySelector('activity-log-stream');
    if (activityLogStream) {
      activityLogStream.clearStream();
    }
  }

  protected getActivityLogHeading_(): string {
    const headingName =
        (this.extensionInfo as ActivityLogExtensionPlaceholder).isPlaceholder ?
        this.i18n('missingOrUninstalledExtension') :
        (this.extensionInfo as chrome.developerPrivate.ExtensionInfo).name;
    return this.i18n('activityLogPageHeading', headingName);
  }

  protected isHistoryTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
  }

  protected isStreamTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
  }

  protected onTabsChangedSelectedSubpage_(
      e: CustomEvent<{value: ActivityLogSubpage}>) {
    this.selectedSubpage_ = e.detail.value;
  }

  protected onSelectedSubpageChanged_(
      newTab: MaybeActivityLogSubpage, oldTab: MaybeActivityLogSubpage) {
    const activityLogStream =
        this.shadowRoot!.querySelector('activity-log-stream');
    if (activityLogStream) {
      if (newTab === ActivityLogSubpage.STREAM) {
        // Start the stream if the user is switching to the real-time tab.
        // This will not handle the first tab switch to the real-time tab as
        // the stream has not been attached to the DOM yet, and is handled
        // instead by the stream's |connectedCallback| method.
        activityLogStream.startStream();
      } else if (oldTab === ActivityLogSubpage.STREAM) {
        // Pause the stream if the user is navigating away from the real-time
        // tab.
        activityLogStream.pauseStream();
      }
    }
  }

  protected onCloseButtonClick_() {
    if ((this.extensionInfo as ActivityLogExtensionPlaceholder).isPlaceholder) {
      navigation.navigateTo({page: Page.LIST});
    } else {
      navigation.navigateTo(
          {page: Page.DETAILS, extensionId: this.extensionInfo.id});
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-activity-log': ExtensionsActivityLogElement;
  }
}


customElements.define(
    ExtensionsActivityLogElement.is, ExtensionsActivityLogElement);
