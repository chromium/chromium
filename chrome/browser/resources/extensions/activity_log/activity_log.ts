// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_page_selector/cr_page_selector.js';
import 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './activity_log_stream.js';
import './activity_log_history.js';
import '../strings.m.js';
import '../shared_style.css.js';
import '../shared_vars.css.js';

import {NONE_SELECTED} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import type {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {afterNextRender, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {navigation, Page} from '../navigation_helper.js';

import {getTemplate} from './activity_log.html.js';
import type {ActivityLogDelegate} from './activity_log_history.js';

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

const ExtensionsActivityLogElementBase = I18nMixin(PolymerElement);

export class ExtensionsActivityLogElement extends
    ExtensionsActivityLogElementBase {
  static get is() {
    return 'extensions-activity-log';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       */
      extensionInfo: Object,

      delegate: Object,

      selectedSubpage_: {
        type: Number,
        value: NONE_SELECTED,
        observer: 'onSelectedSubpageChanged_',
      },

      tabNames_: {
        type: Array,
        value: () => ([
          loadTimeData.getString('activityLogHistoryTabHeading'),
          loadTimeData.getString('activityLogStreamTabHeading'),
        ]),
      },
    };
  }

  extensionInfo: chrome.developerPrivate.ExtensionInfo|
      ActivityLogExtensionPlaceholder;
  delegate: ActivityLogDelegate;
  private selectedSubpage_: ActivityLogSubpage;
  private tabNames_: string[];

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  /**
   * Focuses the back button when page is loaded and set the activie view to
   * be HISTORY when we navigate to the page.
   */
  private onViewEnterStart_() {
    this.selectedSubpage_ = ActivityLogSubpage.HISTORY;
    afterNextRender(this, () => focusWithoutInk(this.$.closeButton));
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

  private getActivityLogHeading_(): string {
    const headingName =
        (this.extensionInfo as ActivityLogExtensionPlaceholder).isPlaceholder ?
        this.i18n('missingOrUninstalledExtension') :
        (this.extensionInfo as chrome.developerPrivate.ExtensionInfo).name;
    return this.i18n('activityLogPageHeading', headingName);
  }

  private isHistoryTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.HISTORY;
  }

  private isStreamTabSelected_(): boolean {
    return this.selectedSubpage_ === ActivityLogSubpage.STREAM;
  }

  private onSelectedSubpageChanged_(
      newTab: ActivityLogSubpage, oldTab: ActivityLogSubpage) {
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

  private onCloseButtonClick_() {
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
