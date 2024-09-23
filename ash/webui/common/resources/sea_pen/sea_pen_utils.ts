// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getSeaPenTemplates, parseTemplateText, QUERY, Query, SeaPenImageId, SeaPenOption, SeaPenTemplate} from './constants.js';
import {SeaPenQuery} from './sea_pen.mojom-webui.js';
import {SeaPenTemplateChip, SeaPenTemplateId} from './sea_pen_generated.mojom-webui.js';

// Returns true if `maybeDataUrl` is a Url that contains a base64 encoded image.
export function isImageDataUrl(maybeDataUrl: unknown): maybeDataUrl is Url {
  return !!maybeDataUrl && typeof maybeDataUrl === 'object' &&
      'url' in maybeDataUrl && typeof maybeDataUrl.url === 'string' &&
      (maybeDataUrl.url.startsWith('data:image/png;base64') ||
       maybeDataUrl.url.startsWith('data:image/jpeg;base64'));
}

// SeaPenImageId must always be a positive
export function isSeaPenImageId(maybeSeaPenImageId: unknown):
    maybeSeaPenImageId is SeaPenImageId {
  return typeof maybeSeaPenImageId === 'number' &&
      Number.isInteger(maybeSeaPenImageId) && maybeSeaPenImageId >= 0;
}

// Returns true if `maybeArray` is an array with at least one item.
export function isNonEmptyArray(maybeArray: unknown): maybeArray is unknown[] {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

// Returns true is `obj` is a FilePath with a non-empty path.
export function isNonEmptyFilePath(obj: unknown): obj is FilePath {
  return !!obj && typeof obj === 'object' && 'path' in obj &&
      typeof obj.path === 'string' && !!obj.path;
}

/**
 * Returns a random number between [0, max).
 */
function getRandomInt(max: number) {
  return Math.floor(Math.random() * max);
}

function isChip(word: string): boolean {
  return !!word && word.startsWith('<') && word.endsWith('>');
}

function toChip(word: string): SeaPenTemplateChip {
  return parseInt(word.slice(1, -1)) as SeaPenTemplateChip;
}

/**
 * Returns the default mapping of chip to option for the template.
 * Randomly picks the option if `random` is true.
 */
export function getDefaultOptions(template: SeaPenTemplate, random = false):
    Map<SeaPenTemplateChip, SeaPenOption> {
  const selectedOptions = new Map<SeaPenTemplateChip, SeaPenOption>();
  template.options.forEach((options, chip) => {
    if (isNonEmptyArray(options)) {
      let option = options[0];
      if (random) {
        option = options[getRandomInt(options.length)];
      }
      selectedOptions.set(chip, option);
    } else {
      console.warn('empty options for', template.id);
    }
  });
  return selectedOptions;
}

/**
 * A template token that is a chip.
 */
export interface ChipToken {
  // The translated string displayed on the UI.
  translation: string;
  // The identifier of the chip.
  id: SeaPenTemplateChip;
}

/**
 * A tokenized unit of the `SeaPenTemplate`. Used to render the prompt on the UI
 */
export type TemplateToken = string|ChipToken;

/**
 * Separates a template into tokens that can be displayed on the UI.
 */
export function getTemplateTokens(
    template: SeaPenTemplate,
    selectedOptions: Map<SeaPenTemplateChip, SeaPenOption>): TemplateToken[] {
  const strs = parseTemplateText(template.text);
  return strs.map(str => {
    if (isChip(str)) {
      const templateChip = toChip(str);
      return {
        translation: selectedOptions.get(templateChip)?.translation || '',
        id: templateChip,
      };
    } else {
      return str;
    }
  });
}

/**
 * Get the selected template options map from the options information in
 * SeaPenQuery `query` and SeaPenTemplate `template`.
 */
export function getSelectedOptionsFromQuery(
    query: SeaPenQuery|null,
    template: SeaPenTemplate): Map<SeaPenTemplateChip, SeaPenOption>|null {
  if (!query || query.textQuery) {
    return null;
  }

  const templateId = query.templateQuery?.id;
  assert(templateId === template.id, 'template id should match');

  // Update the selected options to match with current Sea Pen query.
  const options = query.templateQuery?.options;
  const newSelectedOptions = new Map<SeaPenTemplateChip, SeaPenOption>();
  for (const [key, value] of Object.entries(options ?? new Map())) {
    const chip = parseInt(key) as SeaPenTemplateChip;
    const chipOptions = template.options.get(chip);
    const selectedChipOption =
        chipOptions?.find((option) => option.value === value);
    if (selectedChipOption) {
      newSelectedOptions.set(chip, selectedChipOption);
    }
  }
  return newSelectedOptions;
}

/**
 * Checks whether a Sea Pen query is active. Freeform query is active by
 * default. Template query should have active template and chip options.
 */
export function isActiveSeaPenQuery(query: SeaPenQuery|undefined): boolean {
  if (!query) {
    return false;
  }

  if (query.textQuery) {
    return true;
  }

  const template = getSeaPenTemplates().find(
      (seaPenTemplate) => seaPenTemplate.id === query.templateQuery?.id);
  const options = query.templateQuery?.options;
  if (!template || !options) {
    return false;
  }

  const isActive = Object.entries(options).every(([key, value]) => {
    const chip = parseInt(key) as SeaPenTemplateChip;
    const activeOptions = template.options.get(chip);
    return !!activeOptions && activeOptions.some(opt => opt.value === value);
  });
  return isActive;
}

/**
 * Get the user visible query from SeaPenQuery `query`. Empty string if the
 * query is null or invalid.
 */
export function getUserVisibleQuery(query: SeaPenQuery): string {
  if (!query) {
    return '';
  }
  if (query.textQuery) {
    return query.textQuery;
  }
  if (query.templateQuery) {
    return query.templateQuery.userVisibleQuery?.text ?? '';
  }
  return '';
}

/**
 * Convert Sea Pen template id in string type to SeaPenTemplateId/Query type.
 */
export function getTemplateIdFromString(templateId: string): SeaPenTemplateId|
    Query {
  if (templateId === QUERY) {
    return QUERY;
  }
  return parseInt(templateId) as SeaPenTemplateId;
}

/**
 * Checks whether the origin of the URL from Personalization App.
 */
export function isPersonalizationApp(): boolean {
  return window.location.origin === 'chrome://personalization';
}

/** Returns true if this event is a user action to select an item. */
export function isSelectionEvent(event: Event): boolean {
  return (event instanceof MouseEvent && event.type === 'click') ||
      (event instanceof KeyboardEvent && event.key === 'Enter');
}

/**
 * Fisher-Yates Shuffle
 */
export function shuffle<T>(array: T[]): T[] {
  const copy = [...array];
  for (let i = copy.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [copy[i], copy[j]] = [copy[j], copy[i]];
  }
  return copy;
}

/**
 * Checks whether the two arrays contain the same elements. Uses strict equals
 * comparison on each member of the arrays.
 */
export function isArrayEqual<T>(arr1: T[], arr2: T[]): boolean {
  return arr1.length === arr2.length &&
      arr1.every((value, index) => value === arr2[index]);
}
