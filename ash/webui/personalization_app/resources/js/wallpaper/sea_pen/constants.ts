// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {SeaPenTemplateChip, SeaPenTemplateId, SeaPenTemplateOption} from '../../../sea_pen.mojom-webui.js';

import {isSeaPenTextInputEnabled} from './load_time_booleans.js';

export const QUERY: string = 'Query';

/**
 * An interface for the data of a recent sea pen image.
 */
export interface RecentSeaPenData {
  url: Url;
  queryInfo: string;
}

export interface SeaPenOption {
  // `value` is the actual option value to be sent to the server side.
  value: SeaPenTemplateOption;
  // `translation` is the translated value to be displayed in the UI.
  translation: string;
}

export interface SeaPenTemplate {
  id: string;
  // `title` is the user-visible string in collection titles and breadcrumbs.
  title: string;
  preview: Url[];
  // `text` is the string that shows up on the sea pen subpage.
  text: string;
  // `options` are in the form of 'option_name': [option1, option2, ...].
  options: Map<SeaPenTemplateChip, SeaPenOption[]>;
}

export function getSeaPenTemplates(): SeaPenTemplate[] {
  const templates = [
    {
      id: SeaPenTemplateId.kFlower.toString(),
      title: 'Airbrushed',
      text: `A radiant <${SeaPenTemplateChip.kFlowerColor}> <${
          SeaPenTemplateChip.kFlowerType}> in bloom`,
      preview: [{
        url: 'chrome://personalization/images/sea_pen_tile.svg',
      }],
      options: new Map([
        [
          SeaPenTemplateChip.kFlowerType,
          [
            {
              value: SeaPenTemplateOption.kFlowerTypeRose,
              translation: 'rose',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeCallaLily,
              translation: 'calla lily',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeWindflower,
              translation: 'windflower',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeTulip,
              translation: 'tulip',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeLilyOfTheValley,
              translation: 'lily of the valley',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeBirdOfParadise,
              translation: 'bird-of-paradise flower',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeOrchid,
              translation: 'orchid',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeRanunculus,
              translation: 'ranunculus',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeDaisy,
              translation: 'daisy',
            },
            {
              value: SeaPenTemplateOption.kFlowerTypeHydrangeas,
              translation: 'hydrangeas',
            },
          ],
        ],
        [
          SeaPenTemplateChip.kFlowerColor,
          [
            {
              value: SeaPenTemplateOption.kFlowerColorPink,
              translation: 'pink',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorPurple,
              translation: 'purple',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorBlue,
              translation: 'blue',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorWhite,
              translation: 'white',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorCoral,
              translation: 'coral',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorYellow,
              translation: 'yellow',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorGreen,
              translation: 'green',
            },
            {
              value: SeaPenTemplateOption.kFlowerColorRed,
              translation: 'red',
            },
          ],
        ],
      ]),
    },
    {
      id: SeaPenTemplateId.kMineral.toString(),
      title: 'Mineral',
      text: `A close-up image of <${SeaPenTemplateChip.kMineralName}> with <${
          SeaPenTemplateChip.kMineralColor}> hues`,
      preview: [{
        url: 'chrome://personalization/images/sea_pen_tile.svg',
      }],
      options: new Map([
        [
          SeaPenTemplateChip.kMineralName,
          [
            {
              value: SeaPenTemplateOption.kMineralNameWhiteQuartz,
              translation: 'white quartz',
            },
            {
              value: SeaPenTemplateOption.kMineralNameAmethyst,
              translation: 'amethyst',
            },
            {
              value: SeaPenTemplateOption.kMineralNameBlueSapphire,
              translation: 'blue sapphire',
            },
            {
              value: SeaPenTemplateOption.kMineralNameAmberCarnelian,
              translation: 'amber carnelian',
            },
            {
              value: SeaPenTemplateOption.kMineralNameEmerald,
              translation: 'emerald',
            },
            {
              value: SeaPenTemplateOption.kMineralNameRuby,
              translation: 'ruby',
            },
          ],
        ],
        [
          SeaPenTemplateChip.kMineralColor,
          [
            {
              value: SeaPenTemplateOption.kMineralColorWhite,
              translation: 'white',
            },
            {
              value: SeaPenTemplateOption.kMineralColorPeriwinkle,
              translation: 'periwinkle',
            },
            {
              value: SeaPenTemplateOption.kMineralColorPink,
              translation: 'pink',
            },
            {
              value: SeaPenTemplateOption.kMineralColorLavender,
              translation: 'lavender',
            },
          ],
        ],
      ]),
    },
    {
      id: SeaPenTemplateId.kLandscape.toString(),
      title: 'Landscape',
      text: `A <${SeaPenTemplateChip.kLandscapeBiome}> landscape with <${
          SeaPenTemplateChip.kLandscapeLighting}> lighting`,
      preview: [{
        url: 'chrome://personalization/images/sea_pen_tile.svg',
      }],
      options: new Map([
        [
          SeaPenTemplateChip.kLandscapeBiome,
          [
            {
              value: SeaPenTemplateOption.kLandscapeBiomeTaiga,
              translation: 'taiga',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeDesert,
              translation: 'desert',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeRainforest,
              translation: 'rainforest',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeTundra,
              translation: 'tundra',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeBeach,
              translation: 'beach',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeIcebergs,
              translation: 'icebergs',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeSwamp,
              translation: 'swamp',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeGrassland,
              translation: 'grassland',
            },
            {
              value: SeaPenTemplateOption.kLandscapeBiomeForest,
              translation: 'forest',
            },
          ],
        ],
        [
          SeaPenTemplateChip.kLandscapeLighting,
          [
            {
              value: SeaPenTemplateOption.kLandscapeLightingDiffuse,
              translation: 'diffuse',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingNorthernLights,
              translation: 'northern lights',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingSunRays,
              translation: 'sun rays',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingGoldenHour,
              translation: 'golden hour',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingEarlyMorning,
              translation: 'early morning',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingBlueHour,
              translation: 'blue hour',
            },
            {
              value: SeaPenTemplateOption.kLandscapeLightingMidday,
              translation: 'midday',
            },
          ],
        ],
      ]),
    },
    {
      id: SeaPenTemplateId.kScifi.toString(),
      title: 'Sci-fi',
      text: `Otherworldly <${SeaPenTemplateChip.kScifiFeature}> in <${
          SeaPenTemplateChip.kScifiColor}> colors`,
      preview: [{
        url: 'chrome://personalization/images/sea_pen_tile.svg',
      }],
      options: new Map([
        [
          SeaPenTemplateChip.kScifiFeature,
          [
            {
              value: SeaPenTemplateOption.kScifiFeatureStreet,
              translation: 'street',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureSkyline,
              translation: 'skyline',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureSwamp,
              translation: 'swamp',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureTransport,
              translation: 'transport hub',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureBusStop,
              translation: 'bus stop',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureDesert,
              translation: 'desert',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureBeach,
              translation: 'beach',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureMountains,
              translation: 'mountains',
            },
            {
              value: SeaPenTemplateOption.kScifiFeaturePark,
              translation: 'park',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureForest,
              translation: 'forest',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureSmallTown,
              translation: 'small town',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureFarm,
              translation: 'farm',
            },
            {
              value: SeaPenTemplateOption.kScifiFeatureUnderwater,
              translation: 'underwater',
            },
          ],
        ],
        [
          SeaPenTemplateChip.kScifiColor,
          [
            {
              value: SeaPenTemplateOption.kScifiColorEarthy,
              translation: 'earthy',
            },
            {
              value: SeaPenTemplateOption.kScifiColorVibrant,
              translation: 'vibrant',
            },
            {
              value: SeaPenTemplateOption.kScifiColorSilver,
              translation: 'silver',
            },
            {
              value: SeaPenTemplateOption.kScifiColorEerie,
              translation: 'eerie',
            },
            {
              value: SeaPenTemplateOption.kScifiColorComplementary,
              translation: 'complementary',
            },
            {
              value: SeaPenTemplateOption.kScifiColorNeutral,
              translation: 'neutral',
            },
          ],
        ],
      ]),
    },
  ];
  if (isSeaPenTextInputEnabled()) {
    templates.push({
      preview: [{
        url: 'chrome://personalization/images/sea_pen_tile.svg',
      }],
      title: 'Freeform',
      text: 'Freeform',
      id: QUERY,
      options: new Map(),
    });
  }
  return templates;
}

/**
 * Split the template string into an array of strings, where each string is
 * either a literal string or a placeholder for a chip.
 * @example
 * // returns ['A park in ', '<city>', ' in the style of ', '<style>']
 * parseTemplateText('A park in <city> in the style of <style>');
 */
export function parseTemplateText(template: string): string[] {
  return template.split(/(<\w+>)/g);
}
