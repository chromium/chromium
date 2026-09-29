// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_page_selector/cr_page_selector.js';
import '//resources/cr_elements/cr_tabs/cr_tabs.js';
import '/strings.m.js';
import './topic_hero.js';
import './topic_sites_dialog.js';
import './topic_summary_panel.js';

import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {getFaviconForPageURL} from '//resources/js/icon.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import {getCss} from './topic_details.css.js';
import {getHtml} from './topic_details.html.js';
import type {TopicSitesDialogElement} from './topic_sites_dialog.js';
import {getOpenableUrls, getTopicSites, isCrIcon, isTopicsEnabled, toTopicItem} from './topic_utils.js';
import type {TopicItem, TopicVisit} from './topic_utils.js';

const MAX_URLS_TO_OPEN = 10;

// How many favicons the sites button shows next to its label.
const MAX_SITES_BUTTON_FAVICONS = 3;

// Matches the cap `PageHandler::OpenGlicPanel()` applies browser-side.
const MAX_SUGGESTED_PROMPTS = 3;

export type TopicDetailsLoadState = 'loading'|'loaded'|'not-found';

// A tab of the details page. To add one, append it here and add its panel to
// the `cr-page-selector` in topic_details.html.ts, in the same order.
// TODO(crbug.com/558572977): Use internationalized strings once GRD strings
// are added.
export const TOPIC_DETAILS_TABS: readonly string[] = ['Summary'];

// Builds the suggestion chips offered when the Glic side panel opens, using
// the continuation queries the backend generated for `topic`. Returning an
// empty list is fine: `PageHandler::OpenGlicPanel()` leaves Zero State
// Suggestions enabled when the page has nothing topic-specific to offer.
export function getSuggestedPrompts(topic: TopicItem): string[] {
  return topic.continuationQueries
      .map(query => query.title.trim() || query.prompt.trim())
      .filter(prompt => !!prompt)
      .slice(0, MAX_SUGGESTED_PROMPTS);
}

// The topic details page (chrome://context-hub/topic_details?id=...). Owns
// loading the topic, page-level state (document title, selected tab, Glic
// panel) and layout; the content itself lives in child elements that take the
// topic as a property.
export class TopicDetailsElement extends CrLitElement {
  static get is() {
    return 'topic-details';
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
      isScrolled_: {type: Boolean},
      loadState_: {type: String},
      selectedTab_: {type: Number},
      sites_: {type: Array},
      tabNames_: {type: Array},
    };
  }

  // Set directly (e.g. by tests) to skip fetching the topic named in the URL.
  accessor topic: TopicItem|null = null;
  protected accessor isScrolled_: boolean = false;
  // Nothing is rendered while loading. 'not-found' is used when the topic in
  // the URL doesn't exist (e.g. it expired or was deleted since the page was
  // opened) or couldn't be fetched.
  protected accessor loadState_: TopicDetailsLoadState = 'loading';
  protected accessor selectedTab_: number = 0;
  // What the sites button counts and the sites dialog lists. Derived from
  // `topic`.
  protected accessor sites_: TopicVisit[] = [];
  // A copy, since `cr-tabs` takes a mutable array.
  protected accessor tabNames_: string[] = [...TOPIC_DETAILS_TABS];

  // Guards against re-running init (a duplicate fetch, reopening the Glic
  // panel) if the element is re-attached to the DOM, whether or not the first
  // fetch has completed.
  private initStarted_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    // Shows focus rings on keyboard navigation only, e.g. in `cr-tabs`.
    FocusOutlineManager.forDocument(document);
    this.initTopic_();
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('topic')) {
      this.sites_ = this.topic ? getTopicSites(this.topic) : [];
    }
  }

  private async initTopic_() {
    if (this.initStarted_) {
      return;
    }
    this.initStarted_ = true;

    if (!this.topic) {
      this.topic = await this.fetchTopic_();
    }
    if (!this.topic) {
      this.loadState_ = 'not-found';
      return;
    }
    this.loadState_ = 'loaded';

    this.updateDocumentTitle_();
    // Only once the topic has loaded, so the panel gets its suggestion chips.
    this.maybeOpenGlicPanel_();
  }

  // Fetches the topic named by the `id` query parameter from the browser.
  // Nothing else is read from the URL: the browser is the source of truth for
  // the topic's contents.
  private async fetchTopic_(): Promise<TopicItem|null> {
    const id = new URLSearchParams(window.location.search).get('id');
    if (!id || !isTopicsEnabled()) {
      return null;
    }
    try {
      const {topic} =
          await browserProxyFactory.getInstance().handler.getTopic(id);
      return topic ? toTopicItem(topic) : null;
    } catch (e) {
      console.error('Failed to fetch topic:', e);
      return null;
    }
  }

  // Opens the Glic side panel bound to this tab, seeded with topic-specific
  // suggestion chips. Only runs when the page was opened from the "jump back
  // in" entry point, which sets the `open_glic` query parameter.
  //
  // The parameter is deliberately left in the URL so this runs on every load.
  // That way the panel comes back if the user closed it, and its suggestion
  // chips are refreshed to match this topic if it was already open.
  private maybeOpenGlicPanel_() {
    const params = new URLSearchParams(window.location.search);
    if (!this.topic || params.get('open_glic') !== '1' || !isTopicsEnabled()) {
      return;
    }

    // Glic availability is deliberately not checked here; the browser side is
    // authoritative and no-ops when Glic is unavailable for the profile.
    browserProxyFactory.getInstance().handler.openGlicPanel(
        getSuggestedPrompts(this.topic));
  }

  // Prefixes the title with the topic's emoji, if it has one. The favicon is
  // deliberately left as the default Context Hub icon: an emoji drawn into an
  // SVG favicon renders without color and duplicates the one in the title.
  private updateDocumentTitle_() {
    const icon = this.topic?.icon || '';
    const title = this.topic?.title || 'Topic Details';
    document.title = !icon || isCrIcon(icon) ? title : `${icon} ${title}`;
  }

  protected onScroll_(e: Event) {
    const scrolled = (e.currentTarget as HTMLElement).scrollTop > 10;
    if (this.isScrolled_ !== scrolled) {
      this.isScrolled_ = scrolled;
    }
  }

  protected onTabsSelectedChanged_(e: CustomEvent<{value: number}>) {
    this.selectedTab_ = e.detail.value;
  }

  // The first few sites' favicons are shown on the sites button.
  protected getButtonFaviconUrls_(): string[] {
    return this.sites_.slice(0, MAX_SITES_BUTTON_FAVICONS)
        .map(site => site.url);
  }

  protected getFavicon_(url: string): string {
    return getFaviconForPageURL(url, /*isSyncedUrlForHistoryUi=*/ false);
  }

  // TODO(crbug.com/558572977): Use internationalized (plural) strings once GRD
  // strings are added.
  protected getSitesLabel_(): string {
    const count = this.sites_.length;
    return count === 1 ? '1 site' : `${count} sites`;
  }

  protected onSitesButtonClick_() {
    this.shadowRoot.querySelector<TopicSitesDialogElement>('#sitesDialog')
        ?.showModal();
  }

  // Opens the topic's related tabs in a tab group. Used by both the summary
  // panel's "Open related tabs" button and the sites dialog's "Open all tabs"
  // button.
  protected async onOpenRelatedTabs_() {
    if (!this.topic) {
      return;
    }
    const urls = getOpenableUrls(this.topic);
    if (urls.length === 0) {
      return;
    }

    const groupLabel = this.topic.title || 'Related Tabs';

    // Attempt to open tabs in a tab group via the Mojo PageHandler.
    // `OpenUrlsInTabGroup()` is gated by the kTopics runtime feature, so fall
    // through to opening individual tabs when it is off.
    if (isTopicsEnabled()) {
      try {
        const {success} =
            await browserProxyFactory.getInstance().handler.openUrlsInTabGroup(
                groupLabel, urls);
        if (success) {
          return;
        }
      } catch (e) {
        // Fallback if backend method call fails or is not supported.
        console.error('Failed to open tabs in group:', e);
      }
    }

    // Fallback: open capped URLs in new tabs. Note that opening multiple tabs
    // via window.open is best-effort and may be throttled by popup blockers.
    for (const url of urls.slice(0, MAX_URLS_TO_OPEN)) {
      OpenWindowProxyImpl.getInstance().openUrl(url);
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-details': TopicDetailsElement;
  }
}

customElements.define(TopicDetailsElement.is, TopicDetailsElement);
