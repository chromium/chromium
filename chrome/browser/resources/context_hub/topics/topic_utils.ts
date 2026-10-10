// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Data model and helpers shared by the topics list (`topics-view`,
// `topic-card`) and the topic details page (`topic-details` and its children).
// Keep this file free of custom element definitions, so that importing it does
// not register elements a page doesn't use.

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {TopicDefectCategory, TopicRating} from '../context_hub.mojom-webui.js';
import type {DuplicateTopicSnapshot, Topic, TopicCollection, TopicCollectionItem, TopicContinuationQuery, TopicFeedback, TopicSnapshot, TopicVisit} from '../context_hub.mojom-webui.js';

export type {TopicContinuationQuery, TopicFeedback, TopicQueryFeedback, TopicVisit} from '../context_hub.mojom-webui.js';

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

// The topics feedback Mojo methods are additionally gated by the
// `fishfood_feedback` param of kTopics, so they and the rating controls must
// only be used when this returns true.
export function isTopicsFishfoodFeedbackEnabled(): boolean {
  return loadTimeData.valueExists('kTopicsFishfoodFeedback') &&
      loadTimeData.getBoolean('kTopicsFishfoodFeedback');
}

export interface TopicDefectCategoryOption {
  category: TopicDefectCategory;
  // Short chip text.
  label: string;
  // The full wording documented on `TopicDefectCategory`, shown as the chip's
  // tooltip and used as its accessible name.
  description: string;
}

// The defect categories a rater can select when disliking a topic, in display
// order.
export const TOPIC_DEFECT_CATEGORIES: readonly TopicDefectCategoryOption[] = [
  {
    category: TopicDefectCategory.kIrrelevantInformationIncluded,
    label: 'Irrelevant content',
    description: 'Irrelevant information included',
  },
  {
    category: TopicDefectCategory.kRelatedInformationMissing,
    label: 'Missing content',
    description: 'Related information missing',
  },
  {
    category: TopicDefectCategory.kTitleWrongOrVague,
    label: 'Wrong title',
    description: 'Title wrong/vague',
  },
  {
    category: TopicDefectCategory.kEmojiWrong,
    label: 'Wrong emoji',
    description: 'Emoji wrong',
  },
  {
    category: TopicDefectCategory.kOverviewInaccurate,
    label: 'Wrong overview',
    description: 'Overview inaccurate',
  },
  {
    category: TopicDefectCategory.kTooBroad,
    label: 'Too broad',
    description: 'Too broad',
  },
  {
    category: TopicDefectCategory.kCombinesSeparateTopics,
    label: 'Mixes topics',
    description: 'Combines separate topics',
  },
  {
    category: TopicDefectCategory.kTooNarrowOrFragmented,
    label: 'Too narrow',
    description: 'Too narrow / fragmented',
  },
  {
    category: TopicDefectCategory.kIncidentalBrowsing,
    label: 'Accidental browsing',
    description: 'Incidental browsing (not intended)',
  },
  {
    category: TopicDefectCategory.kDuplicateOfAnotherTopic,
    label: 'Overlaps another topic',
    // Reworded from the enum's "Duplicate of another topic", since topics are
    // rarely exact duplicates, but rather parts of the same larger goal.
    description: 'Overlaps with another topic, and should be combined with it',
  },
  {
    category: TopicDefectCategory.kSensitiveTopic,
    label: 'Sensitive topic',
    description: 'Sensitive topic shouldn\'t be shown',
  },
  {
    category: TopicDefectCategory.kOther,
    label: 'Other',
    description: 'Other (describe it in the comment)',
  },
];

export interface TopicSuggestionField {
  // The defect that prompts for the suggestion.
  defect: TopicDefectCategory;
  // The suggestion field's label, also its prefix in the stored comment.
  label: string;
}

// The defects for which the rater is also asked what would be better, in
// display order. Until `TopicFeedback` has fields for them, the suggestions are
// stored as "<label>: <suggestion>" lines at the end of its comment.
export const TOPIC_SUGGESTION_FIELDS: readonly TopicSuggestionField[] = [
  {defect: TopicDefectCategory.kTitleWrongOrVague, label: 'Better title'},
  {defect: TopicDefectCategory.kEmojiWrong, label: 'Better emoji'},
  {defect: TopicDefectCategory.kOverviewInaccurate, label: 'Better overview'},
];

// A stored comment split into the rater's own text and their suggestions, by
// the defect they're for.
export interface TopicFeedbackComment {
  text: string;
  suggestions: Map<TopicDefectCategory, string>;
}

// Splits `comment` into the rater's text and the suggestion lines that
// `formatTopicFeedbackComment()` appended to it.
export function parseTopicFeedbackComment(comment: string):
    TopicFeedbackComment {
  const lines = comment.split('\n');
  const suggestions = new Map<TopicDefectCategory, string>();
  while (lines.length > 0) {
    const line = lines[lines.length - 1]!;
    const field = TOPIC_SUGGESTION_FIELDS.find(
        field => line.startsWith(`${field.label}: `));
    if (!field || suggestions.has(field.defect)) {
      break;
    }
    suggestions.set(field.defect, line.slice(field.label.length + 2));
    lines.pop();
  }
  let text = lines.join('\n');
  // Drop the blank line separating the text from the suggestions.
  if (suggestions.size > 0 && text.endsWith('\n')) {
    text = text.slice(0, -1);
  }
  return {text, suggestions};
}

// Appends the non-empty `suggestions` to `text` as one line each. Doesn't trim
// anything, so that it can run on every keystroke; use
// `normalizeTopicFeedbackComment()` before storing the result.
export function formatTopicFeedbackComment(
    text: string, suggestions: Map<TopicDefectCategory, string>): string {
  const lines = TOPIC_SUGGESTION_FIELDS.flatMap(field => {
    // A suggestion has to stay on its line to be parsed back.
    const suggestion =
        (suggestions.get(field.defect) || '').replace(/[\r\n]+/g, ' ');
    return suggestion ? [`${field.label}: ${suggestion}`] : [];
  });
  if (lines.length === 0) {
    return text;
  }
  return text ? `${text}\n\n${lines.join('\n')}` : lines.join('\n');
}

// Trims the text and suggestions of `comment`, dropping blank suggestions.
export function normalizeTopicFeedbackComment(comment: string): string {
  const {text, suggestions} = parseTopicFeedbackComment(comment);
  const trimmed = new Map(
      Array.from(suggestions, ([defect, value]) => [defect, value.trim()]));
  return formatTopicFeedbackComment(text.trim(), trimmed);
}

// Captures what `topic` looks like now, to store alongside its rating.
// `timeRated` is set by the browser when the feedback is saved.
export function createTopicSnapshot(topic: TopicItem): TopicSnapshot {
  return {
    title: topic.title,
    emoji: isCrIcon(topic.icon) ? '' : topic.icon,
    // The long overview, or the short one if that's all the topic has.
    overview: topic.longDescription,
    visitTimes: topic.visits.map(visit => visit.visitTime),
    timeRated: {internalValue: 0n},
  };
}

// Returns unrated feedback, with nothing filled in, for `topic`.
export function createEmptyTopicFeedback(topic: TopicItem): TopicFeedback {
  return {
    id: topic.id,
    snapshot: createTopicSnapshot(topic),
    rating: TopicRating.kUnrated,
    defects: [],
    comment: '',
    queryFeedbacks: [],
    rejectedVisits: [],
    duplicateOf: null,
  };
}

// Whether `feedback` has a comment, ignoring surrounding whitespace.
export function hasTopicFeedbackComment(feedback: TopicFeedback): boolean {
  return feedback.comment.trim().length > 0;
}

// Whether `feedback`'s comment has text of the rater's own, besides any
// suggestions, ignoring surrounding whitespace.
export function hasTopicFeedbackCommentText(feedback: TopicFeedback): boolean {
  return parseTopicFeedbackComment(feedback.comment).text.trim().length > 0;
}

// Whether `a` and `b` have the same rating, defects and comment, i.e. the parts
// of the feedback `<topic-feedback-controls>` edits.
export function hasSameRating(a: TopicFeedback, b: TopicFeedback): boolean {
  return a.rating === b.rating && a.comment === b.comment &&
      a.defects.length === b.defects.length &&
      a.defects.every((defect, i) => defect === b.defects[i]);
}
// Whether `feedback` can be saved: a thumbs down needs at least one defect,
// and the Other defect needs a comment.
export function isTopicFeedbackValid(feedback: TopicFeedback): boolean {
  if (feedback.rating === TopicRating.kDisliked &&
      feedback.defects.length === 0) {
    return false;
  }
  if (feedback.defects.includes(TopicDefectCategory.kOther) &&
      !hasTopicFeedbackCommentText(feedback)) {
    return false;
  }
  return true;
}

// Whether `feedback` holds nothing worth storing, in which case it should be
// deleted rather than saved.
export function isTopicFeedbackEmpty(feedback: TopicFeedback): boolean {
  return feedback.rating === TopicRating.kUnrated &&
      feedback.defects.length === 0 && !hasTopicFeedbackComment(feedback) &&
      feedback.queryFeedbacks.length === 0 &&
      feedback.rejectedVisits.length === 0 && !feedback.duplicateOf;
}

// Matches the cap `PageHandler::OpenGlicPanel()` applies browser-side.
export const MAX_SUGGESTED_PROMPTS = 3;

// A continuation query as shown to the user, with its index in
// `TopicItem.continuationQueries`.
export interface TopicQuery {
  index: number;
  text: string;
}

// The first continuation queries of `topic` that have any text, as sent to the
// Glic side panel and rated by fishfood raters.
export function getTopicQueries(topic: TopicItem): TopicQuery[] {
  return topic.continuationQueries
      .map((query, index) => ({
             index,
             text: query.title.trim() || query.prompt.trim(),
           }))
      .filter(query => !!query.text)
      .slice(0, MAX_SUGGESTED_PROMPTS);
}

// A fishfood rater's feedback on one of a topic's collections (carousels):
// whether it's useful, and which of its pages don't belong in it.
// TODO(b/568422896): Replace with a mojom struct stored in TopicFeedback once
// it has fields for collections. Until then this is only kept in the page.
export interface CollectionFeedback {
  // The collection's title when rated.
  title: string;
  // Null if not rated.
  liked: boolean|null;
  // URLs of the pages the rater flagged as not belonging.
  rejectedItemUrls: string[];
}

// Whether `feedback` holds nothing worth keeping.
export function isCollectionFeedbackEmpty(feedback: CollectionFeedback):
    boolean {
  return feedback.liked === null && feedback.rejectedItemUrls.length === 0;
}

// Captures what `topic` looks like now, to store as the topic another one
// duplicates.
export function createDuplicateTopicSnapshot(topic: TopicItem):
    DuplicateTopicSnapshot {
  return {
    id: topic.id,
    title: topic.title,
    visitTimes: topic.visits.map(visit => visit.visitTime),
  };
}

// Whether `visitTime` is among the visits a rater flagged as not belonging.
export function isVisitRejected(
    rejectedVisits: readonly Time[], visitTime: Time): boolean {
  return rejectedVisits.some(
      time => time.internalValue === visitTime.internalValue);
}

// Returns `rejectedVisits` with `visitTimes` added, or removed if `rejected` is
// false.
export function setVisitsRejected(
    rejectedVisits: readonly Time[], visitTimes: readonly Time[],
    rejected: boolean): Time[] {
  const others =
      rejectedVisits.filter(time => !isVisitRejected(visitTimes, time));
  return rejected ? [...others, ...visitTimes] : others;
}

// Microseconds between the Windows epoch (1601), which `Time` counts from, and
// the Unix epoch.
const WINDOWS_TO_UNIX_EPOCH_US = 11644473600000000n;

// Formats `time` as a short local date and time, e.g. "Oct 7, 2026, 9:41 AM".
export function formatTopicTime(time: Time): string {
  const ms = Number((time.internalValue - WINDOWS_TO_UNIX_EPOCH_US) / 1000n);
  return new Date(ms).toLocaleString(
      undefined, {dateStyle: 'medium', timeStyle: 'short'});
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
