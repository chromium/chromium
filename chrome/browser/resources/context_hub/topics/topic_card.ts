// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_button/cr_button.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';
import '//resources/cr_elements/icons.html.js';

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {Topic, TopicContinuationQuery} from '../context_hub.mojom-webui.js';

import {getCss} from './topic_card.css.js';
import {getHtml} from './topic_card.html.js';

export interface TopicItem {
  id: string;
  title: string;
  description: string;
  longDescription?: string;
  icon?: string;
  relatedUrls?: string[];
  continuationQueries?: TopicContinuationQuery[];
}

// Adapts a Topic from the browser process to what the topics UI renders. The
// backend already resolved every visit to a URL and a title, so this is a
// pure field mapping.
export function toTopicItem(topic: Topic): TopicItem {
  return {
    id: topic.id,
    title: topic.title,
    // The short summary is what the card has room for; fall back to the long
    // one when the server only sent that.
    description: topic.shortOverview || topic.overview || '',
    longDescription: topic.overview || undefined,
    // `topic-card` renders any icon string without a colon as literal text,
    // which is what an emoji needs.
    icon: topic.emoji || undefined,
    relatedUrls: topic.visits.map(visit => visit.url),
    continuationQueries: topic.continuationQueries,
  };
}

export type BadgeShape = 'cloud' | 'flower' | 'circle' | 'diamond';

const BADGE_SHAPES: readonly BadgeShape[] = [
  'cloud',
  'flower',
  'circle',
  'diamond',
];

// Badge background colors. Deliberately a different count than BADGE_SHAPES,
// and picked from different bits of the hash, so a given shape does not always
// pair with the same color.
export const BADGE_BACKGROUND_COLORS: readonly string[] = [
  'var(--google-blue-100)',
  'var(--google-green-200)',
  'var(--google-yellow-100)',
  'var(--google-red-100)',
  'var(--google-purple-200)',
];

export const DEFAULT_ICON = 'cr:insert-drive-file';

export const FLOWER_PATH =
    'M28 2C31.5 2 34 5.5 37.5 6.5C41 7.5 44.5 7 47.5 9.5C50.5 12 50.5 15.5 ' +
    '52.5 18.5C54.5 21.5 56 24.5 56 28C56 31.5 54.5 34.5 52.5 37.5C50.5 40.5 ' +
    '50.5 44 47.5 46.5C44.5 49 41 48.5 37.5 49.5C34 50.5 31.5 54 28 54C24.5 ' +
    '54 22 50.5 18.5 49.5C15 48.5 11.5 49 8.5 46.5C5.5 44 5.5 40.5 3.5 ' +
    '37.5C1.5 34.5 0 31.5 0 28C0 24.5 1.5 21.5 3.5 18.5C5.5 15.5 5.5 12 8.5 ' +
    '9.5C11.5 7 15 7.5 18.5 6.5C22 5.5 24.5 2 28 2Z';

export const CLOUD_PATH =
    'M28 2C34.5 2 39 6.5 43.5 10.5C47.5 14.5 54 19 54 28C54 37 47.5 41.5 ' +
    '43.5 45.5C39 49.5 34.5 54 28 54C21.5 54 17 49.5 12.5 45.5C8.5 41.5 2 ' +
    '37 2 28C2 19 8.5 14.5 12.5 10.5C17 6.5 21.5 2 28 2Z';

export const CIRCLE_PATH =
    'M28 2C42.36 2 54 13.64 54 28C54 42.36 42.36 54 28 54C13.64 54 2 ' +
    '42.36 2 28C2 13.64 13.64 2 28 2Z';

export const DIAMOND_PATH =
    'M28 4C29.5 2.5 32.5 2.5 34 4L52 22C53.5 23.5 53.5 26.5 52 28L34 ' +
    '46C32.5 47.5 29.5 47.5 28 46L10 28C8.5 26.5 8.5 23.5 10 22Z';

// 32-bit FNV-1a hash of `id`. The badge is derived from the topic id, rather
// than from its position in the list, so that the topics list and the topic
// details page (which only knows the id) always agree.
function hashTopicId(id: string): number {
  let hash = 0x811c9dc5;
  for (let i = 0; i < id.length; i++) {
    hash ^= id.charCodeAt(i);
    hash = Math.imul(hash, 0x01000193);
  }
  return hash >>> 0;
}

export function getBadgeShapeForTopic(id: string): BadgeShape {
  return BADGE_SHAPES[hashTopicId(id) % BADGE_SHAPES.length]!;
}

export function getBackgroundColorForTopic(id: string): string {
  const hash = Math.floor(hashTopicId(id) / BADGE_SHAPES.length);
  return BADGE_BACKGROUND_COLORS[hash % BADGE_BACKGROUND_COLORS.length]!;
}

// TODO(crbug.com/558572977): Use internationalized strings once GRD
// strings are added.
export class TopicCardElement extends CrLitElement {
  static get is() {
    return 'topic-card';
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
    };
  }

  accessor topic: TopicItem|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('topic')) {
      this.toggleAttribute('hidden', !this.hasContent_());
    }
  }

  protected hasContent_(): boolean {
    return Boolean(
        this.topic &&
        (this.topic.title?.trim() || this.topic.description?.trim()));
  }

  protected getBadgeShape_(): BadgeShape {
    return getBadgeShapeForTopic(this.topic?.id || '');
  }

  protected getBackgroundColor_(): string {
    return getBackgroundColorForTopic(this.topic?.id || '');
  }

  protected getFlowerPath_(): string {
    return FLOWER_PATH;
  }

  protected getCloudPath_(): string {
    return CLOUD_PATH;
  }

  protected getIcon_(): string {
    return this.topic?.icon || DEFAULT_ICON;
  }

  // Assumes icon strings containing a colon (e.g. 'cr:insert-drive-file')
  // are cr-icon iconset identifiers, while others are treated as text/emojis.
  protected isCrIcon_(): boolean {
    return this.getIcon_().includes(':');
  }

  protected getActionAriaLabel_(): string {
    return this.topic?.title?.trim() ?
        `Jump back in to ${this.topic.title}` :
        'Jump back in';
  }

  protected onJumpBackInClick_() {
    if (!this.topic) {
      return;
    }
    this.fire('jump-back-in', {topic: this.topic});
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'topic-card': TopicCardElement;
  }
}

customElements.define(TopicCardElement.is, TopicCardElement);
