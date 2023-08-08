// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './shared-vars.css.js';
import '../pdf_viewer_shared_style.css.js';
import './icons.html.js';
import './viewer-attachment-bar.js';
import './viewer-document-outline.js';
import './viewer-thumbnail-bar.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_hidden_style.css.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';

import {DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Bookmark} from '../bookmark_type.js';
import {Attachment} from '../constants.js';
import {record, UserAction} from '../metrics.js';

import {getTemplate} from './viewer-pdf-sidenav.html.js';

enum TabId {
  THUMBNAIL = 0,
  OUTLINE = 1,
  ATTACHMENT = 2,
}

interface Tab {
  id: number;
  title: string;
  icon: string;
}

export interface ViewerPdfSidenavElement {
  $: {
    icons: HTMLElement,
  };
}

export class ViewerPdfSidenavElement extends PolymerElement {
  static get is() {
    return 'viewer-pdf-sidenav';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activePage: Number,

      attachments: {
        type: Array,
        value: () => [],
      },

      bookmarks: {
        type: Array,
        value: () => [],
      },

      clockwiseRotations: Number,

      docLength: Number,

      hideIcons_: {
        type: Boolean,
        computed: 'computeHideIcons_(tabs_.length)',
      },

      tabs_: {
        type: Array,
        computed: `computeTabs_(bookmarks.length, attachments.length)`,
      },

      selectedTab_: {
        type: Number,
        value: 0,
      },
    };
  }

  activePage: number;
  attachments: Attachment[];
  bookmarks: Bookmark[];
  clockwiseRotations: number;
  docLength: number;
  private hideIcons_: boolean;
  private selectedTab_: number;
  private tabs_: Tab[];

  override ready() {
    super.ready();

    this.$.icons.addEventListener('keydown', this.onKeydown_.bind(this));
  }

  private computeTabs_(): Tab[] {
    const tabs = [
      {
        id: TabId.THUMBNAIL,
        icon: 'pdf:thumbnails',
        title: '$i18n{tooltipThumbnails}',
      },
    ];

    if (this.bookmarks.length > 0) {
      tabs.push({
        id: TabId.OUTLINE,
        icon: 'pdf:doc-outline',
        title: '$i18n{tooltipDocumentOutline}',
      });
    }

    if (this.attachments.length > 0) {
      tabs.push({
        id: TabId.ATTACHMENT,
        icon: 'pdf:attach-file',
        title: '$i18n{tooltipAttachments}',
      });
    }
    return tabs;
  }

  private computeHideIcons_(): boolean {
    return this.tabs_.length === 1;
  }

  private getTabAriaSelected_(tabId: number): string {
    return this.tabs_[this.selectedTab_].id === tabId ? 'true' : 'false';
  }

  private getTabIndex_(tabId: number): string {
    return this.tabs_[this.selectedTab_].id === tabId ? '0' : '-1';
  }

  private getTabSelectedClass_(tabId: number): string {
    return this.tabs_[this.selectedTab_].id === tabId ? 'selected' : '';
  }

  private onTabClick_(e: DomRepeatEvent<Tab>) {
    const targetTab = e.model.item;
    switch (targetTab.id) {
      case TabId.THUMBNAIL:
        record(UserAction.SELECT_SIDENAV_THUMBNAILS);
        this.selectedTab_ = 0;
        break;

      case TabId.OUTLINE:
        record(UserAction.SELECT_SIDENAV_OUTLINE);
        this.selectedTab_ = 1;
        break;

      case TabId.ATTACHMENT:
        record(UserAction.SELECT_SIDENAV_ATTACHMENT);
        this.selectedTab_ = this.tabs_.length - 1;
        break;
    }
  }

  private hideThumbnailView_(): boolean {
    return this.tabs_[this.selectedTab_].id !== TabId.THUMBNAIL;
  }

  private hideOutlineView_(): boolean {
    return this.tabs_[this.selectedTab_].id !== TabId.OUTLINE;
  }

  private hideAttachmentView_(): boolean {
    return this.tabs_[this.selectedTab_].id !== TabId.ATTACHMENT;
  }

  private onKeydown_(e: KeyboardEvent) {
    if (this.hideIcons_ || (e.key !== 'ArrowUp' && e.key !== 'ArrowDown')) {
      return;
    }

    e.preventDefault();
    e.stopPropagation();

    if (e.key === 'ArrowUp') {
      if (this.selectedTab_ === 0) {
        this.selectedTab_ = this.tabs_.length - 1;
      } else {
        this.selectedTab_--;
      }
    } else {
      if (this.selectedTab_ === this.tabs_.length - 1) {
        this.selectedTab_ = 0;
      } else {
        this.selectedTab_++;
      }
    }
  }

  getHideIconsForTesting(): boolean {
    return this.hideIcons_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-pdf-sidenav': ViewerPdfSidenavElement;
  }
}

customElements.define(ViewerPdfSidenavElement.is, ViewerPdfSidenavElement);
