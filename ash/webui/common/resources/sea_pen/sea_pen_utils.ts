// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {parseTemplateText, SeaPenOption, SeaPenTemplate} from './constants.js';
import {SeaPenTemplateChip, SeaPenTemplateId} from './sea_pen.mojom-webui.js';

// Returns true if `maybeDataUrl` is a Url that contains a base64 encoded image.
export function isImageDataUrl(maybeDataUrl: unknown): maybeDataUrl is Url {
  return !!maybeDataUrl && typeof maybeDataUrl === 'object' &&
      'url' in maybeDataUrl && typeof maybeDataUrl.url === 'string' &&
      (maybeDataUrl.url.startsWith('data:image/png;base64') ||
       maybeDataUrl.url.startsWith('data:image/jpeg;base64'));
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

export function logSeaPenTemplateFeedback(
    templateName: string, positiveFeedback: boolean) {
  chrome.metricsPrivate.recordBoolean(
      `Ash.SeaPen.${templateName}.UserFeedback`, positiveFeedback);
}

export function logGenerateSeaPenWallpaper(seaPenTemplateId: SeaPenTemplateId) {
  chrome.metricsPrivate.recordEnumerationValue(
      `Ash.SeaPen.CreateButton`, seaPenTemplateId,
      SeaPenTemplateId.MAX_VALUE + 1);
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
