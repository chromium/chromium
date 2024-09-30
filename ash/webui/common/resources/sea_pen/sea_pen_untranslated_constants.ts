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
  'star filled sky',
  'dramatic shadows',
  'covered in snow',
  'bioluminescent',
  'long exposure',
  'foggy',
  'shooting star',
  'galaxy',
  'neon lights',
  'reflections',
  'lightning',
  'bokeh effect',
  'with color grading',
  'cinematic shot',
  'volumetric light',
  'negative space',
  'digital art',
  't-rex',
  'unicorn',
  'cats',
  'vector art style',
  '3D render',
];

// These values are used for metrics; do not change or reuse values.
export enum SeaPenSamplePromptId {
  CHROME_SPHERES = 0,
  GALAXY_WITH_SPACESHIP = 1,
  CAT_RIDING_UNICORN = 2,
  ANIMATED_FLOWERS = 3,
  LILY_IN_RAIN = 4,
  COLORFUL_TREEHOUSE = 5,
  DALMATION = 6,
  DELOREAN = 7,
  BLACK_MOTORCYCLE = 8,
  SPACESHIP_OVER_CITY = 9,
  CAT_ON_WINDOWSILL = 10,
  BIOLUMINESCENT_BEACH = 11,
  BLACK_SAND_DUNES = 12,
  TREE_MADE_OF_STARS = 13,
  MOON_OVER_LAKE = 14,
  MARBLE_ARCH = 15,
  STEAMPUNK_SPACESHIP = 16,
  WHITE_TIGER = 17,
  ANIME_PATH_OVERLOOKING_OCEAN = 18,
  PAPAVER_RHEA_STEMS = 19,
  METEOR_SHOWER = 20,

  MAX_VALUE = METEOR_SHOWER,
}

export const SEA_PEN_SAMPLES: SeaPenSamplePrompt[] =
    [
      {
        id: SeaPenSamplePromptId.CHROME_SPHERES,
        prompt:
            '3D rendering of chrome spheres and sphere shapes on a background of chrome architectural elements. The composition is a modern minimalistic concept for advertising, branding or presentation with copy space. A large arch stands in front of an abstract background in the style of minimalism. Studio lighting. Closeup shot.',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/3d_rendering_of_chrome_spheres.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.GALAXY_WITH_SPACESHIP,
        prompt:
            'a beautiful cosmic nebula galaxy with a fast futuristic spaceship, blue, pink and purple hues',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_beautiful_cosmic_nebula_galaxy_with_a_fast_futuristic_spaceship.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.CAT_RIDING_UNICORN,
        prompt:
            'a cat playing a flying v while riding a unicorn, with a lightning bolt in the background',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_cat_playing_a_flying_v_while_riding_a_unicorn.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.ANIMATED_FLOWERS,
        prompt:
            'a close up of several white and yellow flowers against a purple background, a still from an animated film, a delicate pale pink flower with long petals and a green stem placed between two large yellow tulips against a blue sky with no leaves or foliage in pastel colors, a cinematic still shot with high resolution and very detailed intricate details, octane render with beautiful lighting, volumetric light and moody light.',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_close_up_of_several_white_and_yellow_flowers.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.LILY_IN_RAIN,
        prompt:
            'a closeup of an intricate, dew-covered pond lily in the rain. The focus is on the delicate petals and light-reflecting water droplets, capturing their soft pastel colors against a blue background. Shot from eye level using natural light',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_closeup_of_an_intricate_dew-covered_pond_lily.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.COLORFUL_TREEHOUSE,
        prompt:
            'a colorful treehouse with rounded shapes, rendered in the style of cinema4d, playful character design, futuristic chromatic waves, organic architecture, 3D render, cute and dreamy scene with a bright color palette using vibrant colors like pink, blue, green, orange, red, and yellow, purple, shown from the front view without a background and in high resolution',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_colorful_treehouse_with_rounded_shapes.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.DALMATION,
        prompt:
            'a dalmatian dog in front of a pink background in a full body dynamic pose, shot with high resolution photography hyper realistic stock background with color grading',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_dalmatian_dog.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.DELOREAN,
        prompt:
            'a Delorean with neon lights driving through the galaxy with stars whizzing by',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_delorean_with_neon_lights.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.BLACK_MOTORCYCLE,
        prompt:
            'a futuristic motorcycle made of black paper with neon lights dark background',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_futuristic_motorcycle_made_of_black_paper.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.SPACESHIP_OVER_CITY,
        prompt:
            'A huge spaceship floats over the city, which is illuminated by many lights. The entire scene has an oil painting style and presents dark tones. It creates a mysterious atmosphere with futuristic sci-fi elements. This artwork was created using C4D software and OC renderer to highlight high resolution and high details. There were also some urban buildings in close range, adding a depth of field effect. The scene is rendered in the style of an oil painting, presenting dark tones and creating a mysterious atmosphere with futuristic sci-fi elements.',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_huge_spaceship_floats_over_the_city.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.CAT_ON_WINDOWSILL,
        prompt:
            'a Japanese animation illustration of a cat sitting on a windowsill watching the rain in the afternoon',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_japanese_animation_illustration_of_a_cat_sitting.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.BIOLUMINESCENT_BEACH,
        prompt:
            'a long exposure photo of a bioluminescent beach at sunset with stars in the sky, beautiful pastel colors, 4k',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_long_exposure_photo_of_a_bioluminescent_beach.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.BLACK_SAND_DUNES,
        prompt:
            'a minimal professional photo of glittery black sand dunes with dramatic shadows',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_minimal_professional_photo_of_glittery_black_sand.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.TREE_MADE_OF_STARS,
        prompt:
            'a photo of a tree made of stars with a beautiful night sky and galaxy in the background',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_photo_of_a_tree_made_of_stars.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.MOON_OVER_LAKE,
        prompt:
            'a photograph of a moon over a lake with falling cherry blossom leaves, 4k',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_photograph_of_a_moon_over_a_lake.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.MARBLE_ARCH,
        prompt:
            'a piece of white organza floating in the middle, above a white marble arch with a cream background',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_piece_of_white_organza_floating_in_the_middle.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.STEAMPUNK_SPACESHIP,
        prompt: 'a steampunk space ship flying through a sand storm',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_steampunk_space_ship_flying_through.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.WHITE_TIGER,
        prompt:
            'a surreal white tiger with gold eyes and gold stripes sitting on a white sand dune',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_surreal_white_tiger_with_gold_eyes.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.ANIME_PATH_OVERLOOKING_OCEAN,
        prompt:
            'anime path overlooking the ocean with falling cherry blossom leaves',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/anime_path_overlooking_the_ocean.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.PAPAVER_RHEA_STEMS,
        prompt:
            'minimal still life of a few Papaver rhea stems in a glass vase on a black background',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/minimal_still_life_of_a_few_Papaver_rhea_stems.jpeg',
        },
      },
      {
        id: SeaPenSamplePromptId.METEOR_SHOWER,
        prompt:
            'photo of a night sky with a meteor shower and a few trees in the foreground, with purple and pink hues',
        preview: {
          url:
              'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/photo_of_a_night_sky_with_a_meteor_shower.jpeg',
        },
      },
    ];
