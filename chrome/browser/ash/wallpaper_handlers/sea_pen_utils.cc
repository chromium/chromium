// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper_handlers/sea_pen_utils.h"

#include "ash/webui/personalization_app/mojom/sea_pen.mojom-forward.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom-shared.h"
#include "ash/webui/personalization_app/mojom/sea_pen.mojom.h"

namespace wallpaper_handlers {

namespace {

std::string TemplateIdToString(
    ash::personalization_app::mojom::SeaPenTemplateId id) {
  switch (id) {
    case ash::personalization_app::mojom::SeaPenTemplateId::kFlower:
      return "flower";
    case ash::personalization_app::mojom::SeaPenTemplateId::kMineral:
      return "mineral";
    case ash::personalization_app::mojom::SeaPenTemplateId::kLandscape:
      return "landscape";
    case ash::personalization_app::mojom::SeaPenTemplateId::kScifi:
      return "scifi";
  }
}

std::string TemplateChipToString(
    ash::personalization_app::mojom::SeaPenTemplateChip chip) {
  switch (chip) {
    case ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerType:
      return "<flower_type>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kFlowerColor:
      return "<flower_color>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kMineralName:
      return "<mineral_name>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kMineralColor:
      return "<mineral_color>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kLandscapeBiome:
      return "<landscape_biome>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::
        kLandscapeLighting:
      return "<landscape_lighting>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kScifiFeature:
      return "<scifi_feature>";
    case ash::personalization_app::mojom::SeaPenTemplateChip::kScifiColor:
      return "<scifi_color>";
  }
}

std::string TemplateOptionToString(
    ash::personalization_app::mojom::SeaPenTemplateOption option) {
  switch (option) {
    case ash::personalization_app::mojom::SeaPenTemplateOption::kFlowerTypeRose:
      return "rose";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeCallaLily:
      return "calla_lily";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeWindflower:
      return "windflower";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeTulip:
      return "tulip";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeLilyOfTheValley:
      return "lily_of_the_valley";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeBirdOfParadise:
      return "bird_of_paradise";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeOrchid:
      return "orchid";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeRanunculus:
      return "ranunculus";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeDaisy:
      return "daisy";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerTypeHydrangeas:
      return "hydrangeas";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorPink:
      return "pink";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorPurple:
      return "purple";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorBlue:
      return "blue";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorWhite:
      return "white";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorCoral:
      return "coral";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorYellow:
      return "yellow";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kFlowerColorGreen:
      return "green";
    case ash::personalization_app::mojom::SeaPenTemplateOption::kFlowerColorRed:
      return "red";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameWhiteQuartz:
      return "white_quartz";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameAmethyst:
      return "amethyst";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameBlueSapphire:
      return "blue_sapphire";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameAmberCarnelian:
      return "amber_carnelian";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameEmerald:
      return "emerald";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralNameRuby:
      return "ruby";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorWhite:
      return "white";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorPeriwinkle:
      return "periwinkle";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorPink:
      return "pink";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kMineralColorLavender:
      return "lavender";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeTaiga:
      return "taiga";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeDesert:
      return "desert";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeRainforest:
      return "rainforest";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeTundra:
      return "tundra";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeBeach:
      return "beach";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeIcebergs:
      return "icebergs";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeSwamp:
      return "swamp";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeGrassland:
      return "grassland";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeBiomeForest:
      return "forest";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingDiffuse:
      return "diffuse";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingNorthernLights:
      return "northern_lights";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingSunRays:
      return "sun_rays";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingGoldenHour:
      return "golden_hour";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingEarlyMorning:
      return "early_morning";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingBlueHour:
      return "blue_hour";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kLandscapeLightingMidday:
      return "midday";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureStreet:
      return "street";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureSkyline:
      return "skyline";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureSwamp:
      return "swamp";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureTransport:
      return "transport";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureBusStop:
      return "bus_stop";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureDesert:
      return "desert";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureBeach:
      return "beach";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureMountains:
      return "mountains";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeaturePark:
      return "park";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureForest:
      return "forest";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureSmallTown:
      return "small_town";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureFarm:
      return "farm";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiFeatureUnderwater:
      return "underwater";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorEarthy:
      return "earthy";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorVibrant:
      return "vibrant";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorSilver:
      return "silver";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorEerie:
      return "eerie";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorComplementary:
      return "complementary";
    case ash::personalization_app::mojom::SeaPenTemplateOption::
        kScifiColorNeutral:
      return "neutral";
  }
}

bool IsValidTemplateQuery(
    const ash::personalization_app::mojom::SeaPenTemplateQueryPtr& query) {
  auto id = query->id;
  auto options = query->options;
  switch (id) {
    case ash::personalization_app::mojom::SeaPenTemplateId::kFlower: {
      auto flower_type = options
                             .find(ash::personalization_app::mojom::
                                       SeaPenTemplateChip::kFlowerType)
                             ->second;
      auto flower_color = options
                              .find(ash::personalization_app::mojom::
                                        SeaPenTemplateChip::kFlowerColor)
                              ->second;
      return (flower_type >= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kFlowerTypeRose &&
              flower_type <= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kFlowerTypeHydrangeas &&
              flower_color >= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kFlowerColorPink &&
              flower_color <= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kFlowerColorRed);
    }
    case ash::personalization_app::mojom::SeaPenTemplateId::kMineral: {
      auto mineral_name = options
                              .find(ash::personalization_app::mojom::
                                        SeaPenTemplateChip::kMineralName)
                              ->second;
      auto mineral_color = options
                               .find(ash::personalization_app::mojom::
                                         SeaPenTemplateChip::kMineralColor)
                               ->second;
      return (mineral_name >=
                  ash::personalization_app::mojom::SeaPenTemplateOption::
                      kMineralNameWhiteQuartz &&
              mineral_name <= ash::personalization_app::mojom::
                                  SeaPenTemplateOption::kMineralNameRuby &&
              mineral_color >= ash::personalization_app::mojom::
                                   SeaPenTemplateOption::kMineralColorWhite &&
              mineral_color <= ash::personalization_app::mojom::
                                   SeaPenTemplateOption::kMineralColorLavender);
    }
    case ash::personalization_app::mojom::SeaPenTemplateId::kLandscape: {
      auto landscape_biome = options
                                 .find(ash::personalization_app::mojom::
                                           SeaPenTemplateChip::kLandscapeBiome)
                                 ->second;
      auto landscape_lighting =
          options
              .find(ash::personalization_app::mojom::SeaPenTemplateChip::
                        kLandscapeLighting)
              ->second;
      return (
          landscape_biome >= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kLandscapeBiomeTaiga &&
          landscape_biome <= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kLandscapeBiomeForest &&
          landscape_lighting >=
              ash::personalization_app::mojom::SeaPenTemplateOption::
                  kLandscapeLightingDiffuse &&
          landscape_lighting <=
              ash::personalization_app::mojom::SeaPenTemplateOption::
                  kLandscapeLightingMidday);
    }
    case ash::personalization_app::mojom::SeaPenTemplateId::kScifi: {
      auto scifi_feature = options
                               .find(ash::personalization_app::mojom::
                                         SeaPenTemplateChip::kScifiFeature)
                               ->second;
      auto scifi_color = options
                             .find(ash::personalization_app::mojom::
                                       SeaPenTemplateChip::kScifiColor)
                             ->second;
      return (scifi_feature >= ash::personalization_app::mojom::
                                   SeaPenTemplateOption::kScifiFeatureStreet &&
              scifi_feature <=
                  ash::personalization_app::mojom::SeaPenTemplateOption::
                      kScifiFeatureUnderwater &&
              scifi_color >= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kScifiColorEarthy &&
              scifi_color <= ash::personalization_app::mojom::
                                 SeaPenTemplateOption::kScifiColorNeutral);
    }
  }
  return true;
}

}  // namespace

bool IsValidOutput(manta::proto::OutputData output,
                   const std::string_view source) {
  if (!output.has_generation_seed()) {
    LOG(WARNING) << "Manta output data missing id for " << source;
    return false;
  }
  if (!output.has_image() || !output.image().has_serialized_bytes()) {
    LOG(WARNING) << "Manta output data missing image for" << source;
    return false;
  }
  return true;
}

manta::proto::Request CreateMantaRequest(
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    absl::optional<uint32_t> generation_seed,
    int num_output,
    manta::proto::ImageResolution target_resolution) {
  manta::proto::Request request;
  request.set_feature_name(manta::proto::FeatureName::CHROMEOS_WALLPAPER);
  manta::proto::RequestConfig& request_config =
      *request.mutable_request_config();
  if (generation_seed) {
    request_config.set_generation_seed(*generation_seed);
  }
  request_config.set_num_outputs(num_output);
  request_config.set_image_resolution(target_resolution);
  request_config.set_aspect_ratio(manta::proto::AspectRatio::ASPECT_RATIO_16_9);
  manta::proto::InputData& input_data = *request.add_input_data();
  if (query->is_text_query()) {
    input_data.set_text(query->get_text_query());
  } else if (query->is_template_query() &&
             IsValidTemplateQuery(query->get_template_query())) {
    input_data.set_tag(kTemplateIdTag.data());
    input_data.set_text(TemplateIdToString(query->get_template_query()->id));
    for (auto option : query->get_template_query()->options) {
      manta::proto::InputData& input_option = *request.add_input_data();
      input_option.set_tag(TemplateChipToString(option.first));
      input_option.set_text(TemplateOptionToString(option.second));
    }
  }
  return request;
}

}  // namespace wallpaper_handlers
