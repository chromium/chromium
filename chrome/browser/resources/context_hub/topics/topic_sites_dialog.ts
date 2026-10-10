// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_dialog/cr_dialog.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons.html.js';

import type {CrDialogElement} from '//resources/cr_elements/cr_dialog/cr_dialog.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './topic_sites_dialog.css.js';
import {getHtml} from './topic_sites_dialog.html.js';
import {getDisplayDomain, getTopicSites, isTopicsFishfoodFeedbackEnabled, isVisitRejected, setVisitsRejected} from './topic_utils.js';
import type {TopicFeedback, TopicItem, TopicVisit} from './topic_utils.js';

export interface TopicSitesDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

// A row of the dialog: a site plus its domain, computed once per topic.
export interface DialogSite extends TopicVisit {
  domain: string;
  // The times of all of the topic's visits to the site.
  visitTimes: Array<TopicVisit['visitTime']>;
}

// Lists the sites of a topic (see `getTopicSites()`) in a modal dialog. Each
// site opens in a new tab when clicked. "Open all tabs" fires the same
// `open-related-tabs` event as the summary panel's "Open related tabs" button,
// so the page opens them the same way.
//
// When fishfood feedback is enabled, each site also has a "Doesn't belong"
// toggle, which flags all of the topic's visits to it and fires
// `rejected-visits-change`; the page owns storing the feedback.
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
      // The stored fishfood feedback on `topic`, if any.
      feedback: {type: Object},
      feedbackEnabled_: {type: Boolean},
      sites_: {type: Array},
    };
  }

  accessor topic: TopicItem|null = null;
  accessor feedback: TopicFeedback|null = null;
  protected accessor feedbackEnabled_: boolean =
      isTopicsFishfoodFeedbackEnabled();
  protected accessor sites_: DialogSite[] = [];

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topic')) {
      const visits = this.topic?.visits || [];
      const sites = this.topic ? getTopicSites(this.topic) : [];
      this.sites_ =
          sites.map(site => ({
                      ...site,
                      domain: getDisplayDomain(site.url),
                      visitTimes: visits.filter(visit => visit.url === site.url)
                                      .map(visit => visit.visitTime),
                    }));
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

  // A site doesn't belong once all of its visits are flagged.
  protected isRejected_(site: DialogSite): boolean {
    const rejectedVisits = this.feedback?.rejectedVisits || [];
    return site.visitTimes.every(time => isVisitRejected(rejectedVisits, time));
  }

  // TODO(crbug.com/558572977): Use internationalized strings once GRD strings
  // are added.
  protected getRejectAriaLabel_(site: DialogSite): string {
    return `Doesn't belong: ${site.title || site.domain}`;
  }

  protected onRejectClick_(e: Event) {
    const index = Number((e.currentTarget as HTMLElement).dataset['index']);
    const site = this.sites_[index];
    if (!site) {
      return;
    }
    this.fire('rejected-visits-change', {
      rejectedVisits: setVisitsRejected(
          this.feedback?.rejectedVisits || [], site.visitTimes,
          !this.isRejected_(site)),
    });
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
