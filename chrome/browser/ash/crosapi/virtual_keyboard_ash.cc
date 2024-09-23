// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/virtual_keyboard_ash.h"

#include "ash/public/cpp/keyboard/keyboard_config.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"

namespace crosapi {
namespace {
using crosapi::mojom::VirtualKeyboardFeature;

void PopulateFeatureRestrictionsToConfig(
    const std::vector<VirtualKeyboardFeature>& features,
    bool enabled,
    std::vector<VirtualKeyboardFeature>* update,
    keyboard::KeyboardConfig* config) {
  for (auto feature : features) {
    switch (feature) {
      case VirtualKeyboardFeature::AUTOCOMPLETE:
        if (config->auto_complete != enabled)
          update->push_back(VirtualKeyboardFeature::AUTOCOMPLETE);
        config->auto_complete = enabled;
        break;
      case VirtualKeyboardFeature::AUTOCORRECT:
        if (config->auto_correct != enabled)
          update->push_back(VirtualKeyboardFeature::AUTOCORRECT);
        config->auto_correct = enabled;
        break;
      case VirtualKeyboardFeature::HANDWRITING:
        if (config->handwriting != enabled)
          update->push_back(VirtualKeyboardFeature::HANDWRITING);
        config->handwriting = enabled;
        break;
      case VirtualKeyboardFeature::SPELL_CHECK:
        if (config->spell_check != enabled)
          update->push_back(VirtualKeyboardFeature::SPELL_CHECK);
        config->spell_check = enabled;
        break;
      case VirtualKeyboardFeature::VOICE_INPUT:
        if (config->voice_input != enabled)
          update->push_back(VirtualKeyboardFeature::VOICE_INPUT);
        config->voice_input = enabled;
        break;
      case VirtualKeyboardFeature::NONE:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
}
}  // namespace

VirtualKeyboardAsh::VirtualKeyboardAsh() = default;
VirtualKeyboardAsh::~VirtualKeyboardAsh() = default;

void VirtualKeyboardAsh::BindReceiver(
    mojo::PendingReceiver<mojom::VirtualKeyboard> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void VirtualKeyboardAsh::RestrictFeatures(
    mojom::VirtualKeyboardRestrictionsPtr restrictions,
    RestrictFeaturesCallback callback) {
  keyboard::KeyboardConfig current_config =
      ChromeKeyboardControllerClient::Get()->GetKeyboardConfig();
  keyboard::KeyboardConfig config(current_config);

  auto update = mojom::VirtualKeyboardRestrictions::New();

  PopulateFeatureRestrictionsToConfig(restrictions->enabled_features, true,
                                      &update->enabled_features, &config);
  PopulateFeatureRestrictionsToConfig(restrictions->disabled_features, false,
                                      &update->disabled_features, &config);

  if (config != current_config) {
    ChromeKeyboardControllerClient::Get()->SetKeyboardConfig(config);
    // This reloads the virtual keyboard (VK) even if it exists, so it can get
    // new restrictFeatures via chrome.virtualKeyboardPrivate.getKeyboardConfig.
    // However, this reload is unnecessary as the API specs do NOT require
    // restrictFeatures to take effect immediately midway through a VK session.
    // Keeping this unnecessary reload for now, just to avoid behaviour changes.
    ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();
  }

  std::move(callback).Run(std::move(update));
}

}  // namespace crosapi
