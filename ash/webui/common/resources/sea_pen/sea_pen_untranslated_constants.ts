// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SeaPenSamplePrompt} from './constants.js';

export const SEA_PEN_SUGGESTIONS: string[] = [
  'abstract art',
  'animated art',
  'anime style',
  'black and white',
  'centered',
  'cinematic',
  'close-up',
  'cool color temperatures',
  'diffuse lighting',
  'digital art',
  'DSLR camera',
  'expired film',
  'fairytale art',
  'futuristic',
  'golden hour lighting',
  'illustration',
  'natural colors',
  'natural lighting',
  'neutral colors',
  'oil painting',
  'overhead photography',
  'photography',
  'pixel art',
  'sci-fi concept art',
  'soft focus',
  'soft lighting',
  'studio photography',
  'surreal',
  'vibrant colors',
  'vintage photography',
  'wide angle lens',
  'with bright rays of sunlight',
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
