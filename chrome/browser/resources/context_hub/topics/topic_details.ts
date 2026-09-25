// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';

import {CIRCLE_PATH, CLOUD_PATH, DEFAULT_ICON, DIAMOND_PATH, FLOWER_PATH, getBackgroundColorForTopic, getBadgeShapeForTopic, toTopicItem} from './topic_card.js';
import type {BadgeShape, TopicItem} from './topic_card.js';
import {getCss} from './topic_details.css.js';
import {getHtml} from './topic_details.html.js';

const MAX_URLS_TO_OPEN = 10;

// Matches the cap `PageHandler::OpenGlicPanel()` applies browser-side.
const MAX_SUGGESTED_PROMPTS = 3;

// The topics Mojo methods are gated by the kTopics runtime feature in the
// browser process, so they must not be called when the feature is off.
function isTopicsEnabled(): boolean {
  return loadTimeData.valueExists('kTopics') &&
      loadTimeData.getBoolean('kTopics');
}

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
      notFound_: {type: Boolean},
    };
  }

  accessor topic: TopicItem|null = null;
  protected accessor isScrolled_: boolean = false;
  // Set when the topic in the URL doesn't exist (e.g. it expired or was
  // deleted since the page was opened) or couldn't be fetched.
  protected accessor notFound_: boolean = false;

  // Guards against re-running init (a duplicate fetch, reopening the Glic
  // panel) if the element is re-attached to the DOM, whether or not the first
  // fetch has completed.
  private initStarted_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.initTopic_();
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
      this.notFound_ = true;
      return;
    }

    this.updateDocumentTitleAndIcon_();
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
    if (params.get('open_glic') !== '1' || !isTopicsEnabled()) {
      return;
    }

    // Glic availability is deliberately not checked here; the browser side is
    // authoritative and no-ops when Glic is unavailable for the profile.
    const handler = browserProxyFactory.getInstance().handler;
    if (handler) {
      handler.openGlicPanel(this.getSuggestedPrompts_());
    }
  }

  // Builds the suggestion chips offered when the side panel opens, using the
  // continuation queries the backend generated for this topic. Returning an
  // empty list is fine: `PageHandler::OpenGlicPanel()` leaves Zero State
  // Suggestions enabled when the page has nothing topic-specific to offer.
  private getSuggestedPrompts_(): string[] {
    return (this.topic?.continuationQueries || [])
        .map(query => query.title.trim() || query.prompt.trim())
        .filter(prompt => !!prompt)
        .slice(0, MAX_SUGGESTED_PROMPTS);
  }

  private updateDocumentTitleAndIcon_() {
    const icon = this.getIcon_();
    const title = this.topic?.title || 'Topic Details';
    document.title = icon.includes(':') ? title : `${icon} ${title}`;

    if (!icon.includes(':')) {
      let link = document.querySelector<HTMLLinkElement>('link[rel~="icon"]');
      if (!link) {
        link = document.createElement('link');
        link.rel = 'icon';
        document.head.appendChild(link);
      }
      link.type = 'image/svg+xml';
      const svgPrefix = 'data:image/svg+xml,<svg xmlns=%22http://' +
          'www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text ' +
          'y=%22.9em%22 font-size=%2290%22>';
      link.href = `${svgPrefix}${encodeURIComponent(icon)}</text></svg>`;
    }
  }

  protected onScroll_ = (e?: Event) => {
    const target = (e?.target as HTMLElement) ||
        this.shadowRoot?.querySelector('.details-wrapper') || this;
    const scrollTop = target.scrollTop || 0;
    const shouldBeScrolled = scrollTop > 10;
    if (this.isScrolled_ !== shouldBeScrolled) {
      this.isScrolled_ = shouldBeScrolled;
    }
  };

  // Derived from the topic id, the same way `topic-card` does, so the page
  // matches the card it was opened from.
  protected getBadgeShape_(): BadgeShape {
    return getBadgeShapeForTopic(this.topic?.id || '');
  }

  protected getBadgePath_(): string {
    const shape = this.getBadgeShape_();
    switch (shape) {
      case 'flower':
        return FLOWER_PATH;
      case 'circle':
        return CIRCLE_PATH;
      case 'diamond':
        return DIAMOND_PATH;
      case 'cloud':
      default:
        return CLOUD_PATH;
    }
  }

  protected getBackgroundColor_(): string {
    return getBackgroundColorForTopic(this.topic?.id || '');
  }

  protected getIcon_(): string {
    return this.topic?.icon || DEFAULT_ICON;
  }

  protected getTextIcon_(): string {
    const icon = this.getIcon_();
    return icon.includes(':') ? '' : icon;
  }

  protected getLongDescription_(): string {
    return this.topic?.longDescription || this.topic?.description || '';
  }

  // Only web pages are openable, matching the browser-side filter in
  // `PageHandler::OpenUrlsInTabGroup()`. This also covers the `window.open`
  // fallback, which would otherwise bypass that filter.
  protected isValidUrl_(urlStr: string): boolean {
    if (!urlStr) {
      return false;
    }
    try {
      const {protocol} = new URL(urlStr);
      return protocol === 'https:' || protocol === 'http:';
    } catch {
      return false;
    }
  }

  protected hasRelatedUrls_(): boolean {
    return !!this.topic?.relatedUrls?.some(url => this.isValidUrl_(url));
  }

  protected async onOpenRelatedTabsClick_() {
    const rawUrls = this.topic?.relatedUrls || [];
    const urls = rawUrls.filter(url => this.isValidUrl_(url));
    if (urls.length === 0) {
      return;
    }

    const groupLabel = this.topic?.title || 'Related Tabs';

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
