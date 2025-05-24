// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {getVcBackgroundTemplates, getWallpaperTemplates} from './constants_generated.js';
import type {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from './sea_pen_generated.mojom-webui.js';

export type Query = 'Query';
export const QUERY: Query = 'Query';

/** Enumeration of supported tabs. */
export enum FreeformTab {
  SAMPLE_PROMPTS = 'sample_prompts',
  RESULTS = 'results',
}

// SeaPen images are identified by a positive integer. For a newly generated
// thumbnail, this is `SeaPenThumbnail.id`.
export type SeaPenImageId = number;

export interface SeaPenSamplePrompt {
  id: number;
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
  'seaPenFreeformSuggestion4K',
  'seaPenFreeformSuggestionRealisticPhoto',
  'seaPenFreeformSuggestionSurreal',
  'seaPenFreeformSuggestionBeautiful',
  'seaPenFreeformSuggestionMinimal',
  'seaPenFreeformSuggestionSunset',
  'seaPenFreeformSuggestionPastelColors',
  'seaPenFreeformSuggestionGlowing',
  'seaPenFreeformSuggestionStarFilledSky',
  'seaPenFreeformSuggestionDramaticShadows',
  'seaPenFreeformSuggestionCoveredInSnow',
  'seaPenFreeformSuggestionBioluminescent',
  'seaPenFreeformSuggestionLongExposure',
  'seaPenFreeformSuggestionFoggy',
  'seaPenFreeformSuggestionGalaxy',
  'seaPenFreeformSuggestionNeonLights',
  'seaPenFreeformSuggestionReflections',
  'seaPenFreeformSuggestionLightning',
  'seaPenFreeformSuggestionBokehEffect',
  'seaPenFreeformSuggestionWithColorGrading',
  'seaPenFreeformSuggestionVolumetricLight',
  'seaPenFreeformSuggestionNegativeSpace',
  'seaPenFreeformSuggestionDigitalArt',
  'seaPenFreeformSuggestionTRex',
  'seaPenFreeformSuggestionUnicorn',
  'seaPenFreeformSuggestionCats',
  'seaPenFreeformSuggestionVectorArtStyle',
  'seaPenFreeformSuggestion3DRender',
];

// These values are used for metrics; do not change or reuse values.
export enum SeaPenSamplePromptId {
  CHROME_SPHERES = 0,
  GALAXY_WITH_SPACESHIP = 1,
  CAT_RIDING_UNICORN = 2,
  ANIMATED_FLOWERS = 3,
  LILY_IN_RAIN = 4,
  // Value 5 is deprecated.
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

export const SEA_PEN_SAMPLES: SeaPenSamplePrompt[] = [
  {
    id: SeaPenSamplePromptId.CHROME_SPHERES,
    prompt: 'seaPenFreeformSampleChromeSpheres',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/3d_rendering_of_chrome_spheres.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.GALAXY_WITH_SPACESHIP,
    prompt: 'seaPenFreeformSampleGalaxyWithSpaceship',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_beautiful_cosmic_nebula_galaxy_with_a_fast_futuristic_spaceship.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.CAT_RIDING_UNICORN,
    prompt: 'seaPenFreeformSampleCatRidingUnicorn',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_cat_playing_a_flying_v_while_riding_a_unicorn.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.ANIMATED_FLOWERS,
    prompt: 'seaPenFreeformSampleAnimatedFlowers',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_close_up_of_several_white_and_yellow_flowers.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.LILY_IN_RAIN,
    prompt: 'seaPenFreeformSampleLilyInRain',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_closeup_of_an_intricate_dew-covered_pond_lily.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.DALMATION,
    prompt: 'seaPenFreeformSampleDalmation',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_dalmatian_dog.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.DELOREAN,
    prompt: 'seaPenFreeformSampleDelorean',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_delorean_with_neon_lights.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.BLACK_MOTORCYCLE,
    prompt: 'seaPenFreeformSampleBlackMotorcycle',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_futuristic_motorcycle_made_of_black_paper.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.SPACESHIP_OVER_CITY,
    prompt: 'seaPenFreeformSampleSpaceshipOverCity',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_huge_spaceship_floats_over_the_city.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.CAT_ON_WINDOWSILL,
    prompt: 'seaPenFreeformSampleCatOnWindowsill',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_japanese_animation_illustration_of_a_cat_sitting.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.BIOLUMINESCENT_BEACH,
    prompt: 'seaPenFreeformSampleBioluminescentBeach',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_long_exposure_photo_of_a_bioluminescent_beach.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.BLACK_SAND_DUNES,
    prompt: 'seaPenFreeformSampleBlackSandDunes',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_minimal_professional_photo_of_glittery_black_sand.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.TREE_MADE_OF_STARS,
    prompt: 'seaPenFreeformSampleTreeMadeOfStars',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_photo_of_a_tree_made_of_stars.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.MOON_OVER_LAKE,
    prompt: 'seaPenFreeformSampleMoonOverLake',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_photograph_of_a_moon_over_a_lake.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.MARBLE_ARCH,
    prompt: 'seaPenFreeformSampleMarbleArch',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_piece_of_white_organza_floating_in_the_middle.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.STEAMPUNK_SPACESHIP,
    prompt: 'seaPenFreeformSampleSteampunkSpaceship',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_steampunk_space_ship_flying_through.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.WHITE_TIGER,
    prompt: 'seaPenFreeformSampleWhiteTiger',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/a_surreal_white_tiger_with_gold_eyes.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.ANIME_PATH_OVERLOOKING_OCEAN,
    prompt: 'seaPenFreeformSampleAnimePathOverlookingOcean',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/anime_path_overlooking_the_ocean.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.PAPAVER_RHEA_STEMS,
    prompt: 'seaPenFreeformSamplePapaverRheaStems',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/minimal_still_life_of_a_few_Papaver_rhea_stems.jpeg',
    },
  },
  {
    id: SeaPenSamplePromptId.METEOR_SHOWER,
    prompt: 'seaPenFreeformSampleMeteorShower',
    preview: {
      url:
          'https://www.gstatic.com/chromecast/home/chromeos/sea_pen/freeform/photo_of_a_night_sky_with_a_meteor_shower.jpeg',
    },
  },
];
