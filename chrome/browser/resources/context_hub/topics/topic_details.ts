// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

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

      let topic: TopicItem | null = null;
      if (id) {
        const storedById = sessionStorage.getItem(`context_hub_topic_${id}`);
        if (storedById) {
          topic = JSON.parse(storedById);
        }
      }

      if (!topic) {
        const storedActive = sessionStorage.getItem('active_topic');
        if (storedActive) {
          topic = JSON.parse(storedActive);
        }
      }

      if (topic && shape) {
        topic.badgeShape = shape;
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

  protected getLongDescription_(): string {
    return this.topic?.longDescription || this.topic?.description || '';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-details': TopicDetailsElement;
  }
}

customElements.define(TopicDetailsElement.is, TopicDetailsElement);
