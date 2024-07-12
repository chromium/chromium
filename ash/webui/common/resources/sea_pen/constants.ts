// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getVcBackgroundTemplates, getWallpaperTemplates} from './constants_generated.js';
import {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from './sea_pen_generated.mojom-webui.js';

export type Query = 'Query';
export const QUERY: Query = 'Query';

// SeaPen images are identified by a positive integer. For a newly generated
// thumbnail, this is `SeaPenThumbnail.id`.
export type SeaPenImageId = number;

export interface SeaPenSamplePrompt {
  prompt: string;
  preview: Url;
}

export interface SeaPenOption {
  // `value` is the actual option value to be sent to the server side.
  value: SeaPenTemplateOption;
  // `translation` is the translated value to be displayed in the UI.
  translation: string;
  // The preview image url of the option.
  previewUrl?: string;
}

export interface SeaPenTemplate {
  id: SeaPenTemplateId|Query;
  // `title` is the user-visible string in collection titles and breadcrumbs.
  title: string;
  preview: Url[];
  // `text` is the string that shows up on the sea pen subpage.
  text: string;
  // `options` are in the form of 'option_name': [option1, option2, ...].
  options: Map<SeaPenTemplateChip, SeaPenOption[]>;
}

export function getSeaPenTemplates(): SeaPenTemplate[] {
  const templates = window.location.origin === 'chrome://personalization' ?
      getWallpaperTemplates() :
      getVcBackgroundTemplates();
  return templates;
}

/**
 * Split the template string into an array of strings, where each string is
 * either a literal string or a placeholder for a chip.
 * @example
 * // returns ['A park in', '<city>', 'in the style of', '<style>']
 * parseTemplateText('A park in <city> in the style of <style>');
 */
export function parseTemplateText(template: string): string[] {
  return template.split(/(<\w+>)/g)
      .filter(function(entry) {
        return entry.trim() != '';
      })
      .map(entry => entry.trim());
}

export const SEA_PEN_SUGGESTIONS: string[] = [
  'blue',
  'black',
  'green',
  'purple',
  'red',
  'yellow',
  'gray',
  'brown',
  'white',
  'teal',
  'olive',
  'cerulean',
  'beige',
];

export const SEA_PEN_SAMPLES: SeaPenSamplePrompt[] = [
  {
    prompt: 'A fluffy golden retreiver puppy with floppy ears',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_glowscapes.jpg',
    },
  },
  {
    prompt: 'A time lapse photo of a bioluminescent beach',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_dreamscapes.jpg',
    },
  },
  {
    prompt: 'Sand dunes with abstract shadows at dawn',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_terrain.jpg',
    },
  },
  {
    prompt: 'A misty green forest of flowering cactus',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_surreal.jpg',
    },
  },
  {
    prompt: 'A beige feathergrass stem on a pink background',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_art.jpg',
    },
  },
  {
    prompt: 'A cat riding a unicorn off into the sunset',
    preview: {
      url:
          'chrome://resources/ash/common/sea_pen/sea_pen_images/sea_pen_characters.jpg',
    },
  },
];
