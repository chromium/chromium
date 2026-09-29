// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_sites_dialog.css.js';
import {getHtml} from './topic_sites_dialog.html.js';
import {getDisplayDomain, getTopicSites} from './topic_utils.js';
import type {TopicItem, TopicVisit} from './topic_utils.js';

export interface TopicSitesDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

// A row of the dialog: a site plus its domain, computed once per topic.
export interface DialogSite extends TopicVisit {
  domain: string;
}

// Lists the sites of a topic (see `getTopicSites()`) in a modal dialog. Each
// site opens in a new tab when clicked. "Open all tabs" fires the same
// `open-related-tabs` event as the summary panel's "Open related tabs" button,
// so the page opens them the same way.
export class TopicSitesDialogElement extends CrLitElement {
  static get is() {
    return 'topic-sites-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      topic: {type: Object},
      sites_: {type: Array},
    };
  }

  accessor topic: TopicItem|null = null;
  protected accessor sites_: DialogSite[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topic')) {
      const sites = this.topic ? getTopicSites(this.topic) : [];
      this.sites_ =
          sites.map(site => ({...site, domain: getDisplayDomain(site.url)}));
    }
  }

  showModal() {
    this.$.dialog.showModal();
  }

  close() {
    this.$.dialog.close();
  }

  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(url, /*isSyncedUrlForHistoryUi=*/ false);
  }

  protected onSiteClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const site = this.sites_[index];
    if (site) {
      OpenWindowProxyImpl.getInstance().openUrl(site.url);
    }
  }

  protected onOpenAllTabsClick_() {
    this.fire('open-related-tabs');
    this.close();
  }

  protected onOkClick_() {
    this.close();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-sites-dialog': TopicSitesDialogElement;
  }
}

customElements.define(TopicSitesDialogElement.is, TopicSitesDialogElement);
