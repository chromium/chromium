// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './icons.html.js';
import './viewer_attachment_bar.js';
import './viewer_document_outline.js';
import './viewer_thumbnail_bar.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {assert} from 'chrome://resources/js/assert.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {Bookmark} from '../bookmark_type.js';
import type {Attachment} from '../constants.js';
import {record, UserAction} from '../metrics.js';

import {getCss} from './viewer_pdf_sidenav.css.js';
import {getHtml} from './viewer_pdf_sidenav.html.js';

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

export class ViewerPdfSidenavElement extends CrLitElement {
  static get is() {
    return 'viewer-pdf-sidenav';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      activePage: {type: Number},
      attachments: {type: Array},
      bookmarks: {type: Array},
      clockwiseRotations: {type: Number},
      docLength: {type: Number},
      pdfCr23Enabled: {type: Boolean},
      selectedTab_: {type: Number},
      tabs_: {type: Array},
    };
  }

  activePage: number = 0;
  attachments: Attachment[] = [];
  bookmarks: Bookmark[] = [];
  clockwiseRotations: number = 0;
  docLength: number = 0;
  pdfCr23Enabled: boolean = false;
  private selectedTab_: number = 0;
  protected tabs_: Tab[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('bookmarks') ||
        changedProperties.has('attachments')) {
      this.tabs_ = this.computeTabs_();
    }
  }

  private iconsetName_(): string {
    return this.pdfCr23Enabled ? 'pdf-cr23' : 'pdf';
  }

  private computeTabs_(): Tab[] {
    const tabs = [
      {
        id: TabId.THUMBNAIL,
        icon: this.iconsetName_() + ':thumbnails',
        title: '$i18n{tooltipThumbnails}',
      },
    ];

    if (this.bookmarks.length > 0) {
      tabs.push({
        id: TabId.OUTLINE,
        icon: this.iconsetName_() + ':doc-outline',
        title: '$i18n{tooltipDocumentOutline}',
      });
    }

    if (this.attachments.length > 0) {
      tabs.push({
        id: TabId.ATTACHMENT,
        icon: this.iconsetName_() + ':attach-file',
        title: '$i18n{tooltipAttachments}',
      });
    }
    return tabs;
  }

  protected hideIcons_(): boolean {
    return this.tabs_.length === 1;
  }

  protected getTabAriaSelected_(tabId: number): string {
    return this.tabs_[this.selectedTab_]!.id === tabId ? 'true' : 'false';
  }

  protected getTabIndex_(tabId: number): string {
    return this.tabs_[this.selectedTab_]!.id === tabId ? '0' : '-1';
  }

  protected getTabSelectedClass_(tabId: number): string {
    return this.tabs_[this.selectedTab_]!.id === tabId ? 'selected' : '';
  }

  protected onTabClick_(e: Event) {
    const tabId = (e.currentTarget as HTMLElement).dataset['tabId'];
    assert(tabId !== undefined);
    switch (Number.parseInt(tabId, 10)) {
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

  protected hideThumbnailView_(): boolean {
    return this.tabs_[this.selectedTab_]!.id !== TabId.THUMBNAIL;
  }

  protected hideOutlineView_(): boolean {
    return this.tabs_[this.selectedTab_]!.id !== TabId.OUTLINE;
  }

  protected hideAttachmentView_(): boolean {
    return this.tabs_[this.selectedTab_]!.id !== TabId.ATTACHMENT;
  }

  protected onKeydown_(e: KeyboardEvent) {
    if (this.tabs_.length === 1 ||
        (e.key !== 'ArrowUp' && e.key !== 'ArrowDown')) {
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
}

declare global {
  interface HTMLElementTagNameMap {
    'viewer-pdf-sidenav': ViewerPdfSidenavElement;
  }
}

customElements.define(ViewerPdfSidenavElement.is, ViewerPdfSidenavElement);
