// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenSamplePrompt} from './constants.js';

export const SEA_PEN_SUGGESTIONS: string[] = [
  '4k',
  'realistic photo',
  'surreal',
  'beautiful',
  'minimal',
  'sunset',
  'pastel colors',
  'glowing',
  'star-filled sky',
  'dramatic shadows',
  'covered in snow',
  'bioluminescent',
  'time lapse',
  'foggy',
  'shooting star',
  'galaxy',
  'neon lights',
  'reflections',
  'lightning',
  't-rex',
  'unicorn',
  'cats',
];

export const SEA_PEN_SAMPLES: SeaPenSamplePrompt[] = [
  {
    prompt: 'A fluffy golden retriever puppy with floppy ears',
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
