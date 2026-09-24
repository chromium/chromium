// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '/strings.m.js';

import {loadTimeData} from '//resources/js/load_time_data.js';
import {OpenWindowProxyImpl} from '//resources/js/open_window_proxy.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {browserProxyFactory} from '../context_hub.mojom-webui.js';
import type {TopicContinuationQuery} from '../context_hub.mojom-webui.js';

import {
  CIRCLE_PATH,
  CLOUD_PATH,
  DEFAULT_BACKGROUND_COLOR,
  DEFAULT_ICON,
  DIAMOND_PATH,
  FLOWER_PATH,
} from './topic_card.js';
import type {BadgeShape, TopicItem} from './topic_card.js';
import {getCss} from './topic_details.css.js';
import {getHtml} from './topic_details.html.js';

const MAX_URLS_TO_OPEN = 10;

// Matches the cap `PageHandler::OpenGlicPanel()` applies browser-side.
const MAX_SUGGESTED_PROMPTS = 3;

// Validates a continuation query parsed from the URL, which is untrusted
// input.
function isContinuationQuery(value: unknown): value is TopicContinuationQuery {
  const query = value as Partial<TopicContinuationQuery>| null;
  return !!query && typeof query.title === 'string' &&
      typeof query.prompt === 'string';
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
    };
  }

  accessor topic: TopicItem|null = null;
  protected accessor isScrolled_: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.initTopic_();
    this.maybeOpenGlicPanel_();
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
    if (params.get('open_glic') !== '1') {
      return;
    }

    // `OpenGlicPanel()` is gated by the kTopics runtime feature in the browser
    // process, so don't call it when the feature is off.
    if (!loadTimeData.valueExists('kTopics') ||
        !loadTimeData.getBoolean('kTopics')) {
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

  private initTopic_() {
    if (!this.topic) {
      this.topic = this.loadStoredTopic_();
    }

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

  private loadStoredTopic_(): TopicItem|null {
    try {
      const urlParams = new URLSearchParams(window.location.search);
      const id = urlParams.get('id');
      const shape = urlParams.get('shape') as BadgeShape | null;
      const icon = urlParams.get('icon');
      const title = urlParams.get('title');
      const bg = urlParams.get('bg');
      const desc = urlParams.get('desc');
      const longDesc = urlParams.get('long_desc');
      const urlsParam = urlParams.get('urls');
      const queriesParam = urlParams.get('queries');

      let topic: TopicItem | null = null;
      if (id) {
        const raw = sessionStorage.getItem(`context_hub_topic_${id}`);
        if (raw) {
          topic = JSON.parse(raw);
        }
      }

      if (!topic) {
        const raw = sessionStorage.getItem('active_topic');
        if (raw) {
          topic = JSON.parse(raw);
        }
      }

      let relatedUrls: string[]|undefined;
      if (urlsParam) {
        try {
          const parsed = JSON.parse(urlsParam);
          if (Array.isArray(parsed)) {
            relatedUrls = parsed.filter(
                (item): item is string => typeof item === 'string');
          }
        } catch {
          // Ignore parse errors.
        }
      }

      let continuationQueries: TopicContinuationQuery[]|undefined;
      if (queriesParam) {
        try {
          const parsed = JSON.parse(queriesParam);
          if (Array.isArray(parsed)) {
            continuationQueries = parsed.filter(isContinuationQuery);
          }
        } catch {
          // Ignore parse errors.
        }
      }

      if (topic) {
        if (shape) {
          topic.badgeShape = shape;
        }
        if (icon && !topic.icon) {
          topic.icon = icon;
        }
        if (bg && !topic.backgroundColor) {
          topic.backgroundColor = bg;
        }
        if (desc && !topic.description) {
          topic.description = desc;
        }
        if (longDesc && !topic.longDescription) {
          topic.longDescription = longDesc;
        }
        if (relatedUrls &&
            (!topic.relatedUrls || topic.relatedUrls.length === 0)) {
          topic.relatedUrls = relatedUrls;
        }
        if (continuationQueries &&
            (!topic.continuationQueries ||
             topic.continuationQueries.length === 0)) {
          topic.continuationQueries = continuationQueries;
        }
      } else if (id || title) {
        topic = {
          id: id || '',
          title: title || '',
          description: desc || '',
          longDescription: longDesc || undefined,
          relatedUrls,
          icon: icon || undefined,
          backgroundColor: bg || undefined,
          badgeShape: shape || undefined,
          continuationQueries,
        };
      }
      return topic;
    } catch {
      // Ignore storage errors.
    }
    return null;
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

  protected getBadgeShape_(): BadgeShape {
    return this.topic?.badgeShape || 'cloud';
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
    return this.topic?.backgroundColor || DEFAULT_BACKGROUND_COLOR;
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
    if (loadTimeData.valueExists('kTopics') &&
        loadTimeData.getBoolean('kTopics')) {
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
