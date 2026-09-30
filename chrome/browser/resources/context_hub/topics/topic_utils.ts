// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data model and helpers shared by the topics list (`topics-view`,
// `topic-card`) and the topic details page (`topic-details` and its children).
// Keep this file free of custom element definitions, so that importing it does
// not register elements a page doesn't use.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import type {Topic, TopicCollection, TopicCollectionItem, TopicContinuationQuery, TopicVisit} from '../context_hub.mojom-webui.js';

export type {TopicContinuationQuery, TopicVisit} from '../context_hub.mojom-webui.js';

// A page in a `Collection`, as rendered on its carousel card.
export interface CollectionItem {
  title: string;
  url: string;
  // The backend's site name, or the page's domain when it didn't send one.
  siteName: string;
}

// A titled group of pages related to a topic, shown as a carousel on the
// topic details page.
export interface Collection {
  title: string;
  items: CollectionItem[];
}

// What the topics UI renders. Adapted from the Mojo `Topic` by `toTopicItem()`,
// which resolves the fallbacks below once so that the views don't have to.
export interface TopicItem {
  id: string;
  title: string;
  // Short summary; falls back to the long one when only that was sent.
  description: string;
  // Long summary; falls back to the short one when only that was sent.
  longDescription: string;
  // An emoji, or a cr-icon identifier (containing a colon) when the topic has
  // no emoji. See `isCrIcon()`.
  icon: string;
  creationTime: Time;
  visits: TopicVisit[];
  continuationQueries: TopicContinuationQuery[];
  // Only collections with a title and at least one openable page.
  collections: Collection[];
}

export const DEFAULT_ICON = 'cr:insert-drive-file';

// The topics Mojo methods are gated by the kTopics runtime feature in the
// browser process, so they must not be called when the feature is off.
export function isTopicsEnabled(): boolean {
  return loadTimeData.valueExists('kTopics') &&
      loadTimeData.getBoolean('kTopics');
}

// Adapts a Topic from the browser process to what the topics UI renders. The
// backend already resolved every visit to a URL and a title, so this is a
// pure field mapping.
export function toTopicItem(topic: Topic): TopicItem {
  const shortOverview = topic.shortOverview || '';
  const overview = topic.overview || '';
  return {
    id: topic.id,
    title: topic.title,
    description: shortOverview || overview,
    longDescription: overview || shortOverview,
    icon: topic.emoji || DEFAULT_ICON,
    creationTime: topic.creationTime,
    visits: topic.visits,
    continuationQueries: topic.continuationQueries,
    collections: toCollections(topic.collections),
  };
}

// Keeps the collections that have a title and at least one page that can be
// opened in a tab and has a title.
function toCollections(collections: TopicCollection[]): Collection[] {
  return collections.map(toCollection)
      .filter(collection => !!collection.title && collection.items.length > 0);
}

function toCollection(collection: TopicCollection): Collection {
  return {
    title: collection.title.trim(),
    items: collection.items.filter(item => isWebUrl(item.url))
               .map(toCollectionItem)
               .filter(item => !!item.title),
  };
}

function toCollectionItem(item: TopicCollectionItem): CollectionItem {
  return {
    title: item.title.trim(),
    url: item.url,
    siteName: item.siteName?.trim() || getDisplayDomain(item.url),
  };
}

// Icon strings containing a colon (e.g. 'cr:insert-drive-file') are cr-icon
// iconset identifiers, everything else is rendered as literal text (emoji).
export function isCrIcon(icon: string): boolean {
  return icon.includes(':');
}

// Only web pages are openable, matching the browser-side filter in
// `PageHandler::OpenUrlsInTabGroup()`.
export function isWebUrl(url: string): boolean {
  if (!url) {
    return false;
  }
  try {
    const {protocol} = new URL(url);
    return protocol === 'https:' || protocol === 'http:';
  } catch {
    return false;
  }
}

// Returns the URLs of `topic`'s visits that can be opened in a tab.
export function getOpenableUrls(topic: TopicItem): string[] {
  return topic.visits.map(visit => visit.url).filter(isWebUrl);
}

// The most sites the topic details page lists, on its sites button and in the
// sites dialog.
export const MAX_TOPIC_SITES = 10;

// Returns the sites the topic details page lists for `topic`: its visits that
// can be opened in a tab, most recent first, one per URL (its latest visit),
// capped to `MAX_TOPIC_SITES`.
export function getTopicSites(topic: TopicItem): TopicVisit[] {
  const visits = topic.visits.filter(visit => isWebUrl(visit.url))
                     .sort((a, b) => compareTimes(b.visitTime, a.visitTime));
  const seenUrls = new Set<string>();
  const sites: TopicVisit[] = [];
  for (const visit of visits) {
    if (sites.length === MAX_TOPIC_SITES) {
      break;
    }
    if (seenUrls.has(visit.url)) {
      continue;
    }
    seenUrls.add(visit.url);
    sites.push(visit);
  }
  return sites;
}

function compareTimes(a: Time, b: Time): number {
  if (a.internalValue === b.internalValue) {
    return 0;
  }
  return a.internalValue < b.internalValue ? -1 : 1;
}

// Returns the host of `url` without a leading "www.", for display, or an
// empty string if `url` can't be parsed.
export function getDisplayDomain(url: string): string {
  try {
    return new URL(url).hostname.replace(/^www\./, '');
  } catch {
    return '';
  }
}

export type BadgeShape = 'cloud'|'flower'|'circle'|'diamond';

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

// Badge outlines. All of them are drawn in, and centered on, a 56x56 box.
const BADGE_PATHS: Readonly<Record<BadgeShape, string>> = {
  cloud: 'M28 2C34.5 2 39 6.5 43.5 10.5C47.5 14.5 54 19 54 28C54 37 47.5 ' +
      '41.5 43.5 45.5C39 49.5 34.5 54 28 54C21.5 54 17 49.5 12.5 45.5C8.5 ' +
      '41.5 2 37 2 28C2 19 8.5 14.5 12.5 10.5C17 6.5 21.5 2 28 2Z',
  flower: 'M28 2C31.5 2 34 5.5 37.5 6.5C41 7.5 44.5 7 47.5 9.5C50.5 12 50.5 ' +
      '15.5 52.5 18.5C54.5 21.5 56 24.5 56 28C56 31.5 54.5 34.5 52.5 ' +
      '37.5C50.5 40.5 50.5 44 47.5 46.5C44.5 49 41 48.5 37.5 49.5C34 50.5 ' +
      '31.5 54 28 54C24.5 54 22 50.5 18.5 49.5C15 48.5 11.5 49 8.5 46.5C5.5 ' +
      '44 5.5 40.5 3.5 37.5C1.5 34.5 0 31.5 0 28C0 24.5 1.5 21.5 3.5 ' +
      '18.5C5.5 15.5 5.5 12 8.5 9.5C11.5 7 15 7.5 18.5 6.5C22 5.5 24.5 2 28 ' +
      '2Z',
  circle: 'M28 2C42.36 2 54 13.64 54 28C54 42.36 42.36 54 28 54C13.64 54 2 ' +
      '42.36 2 28C2 13.64 13.64 2 28 2Z',
  // A square with rounded corners, rotated 45 degrees.
  diamond: 'M23 7Q28 2 33 7L49 23Q54 28 49 33L33 49Q28 54 23 49L7 33Q2 28 7 ' +
      '23Z',
};

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

// Returns the SVG path of `shape`, drawn in a 56x56 box.
export function getBadgePath(shape: BadgeShape): string {
  return BADGE_PATHS[shape];
}
