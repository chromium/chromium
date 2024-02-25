// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_lookup.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/accelerators_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/types/optional_ref.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/ash/keyboard_capability.h"

namespace ash {

namespace {

constexpr char16_t kDetailsDelimiter[] = u"+";

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

using OptionalAccelerators =
    base::optional_ref<const std::vector<ui::Accelerator>>;

}  // namespace

AcceleratorLookup::AcceleratorLookup(
    raw_ptr<AcceleratorConfiguration> ash_accelerators)
    : ash_accelerator_configuration_(ash_accelerators) {}

AcceleratorLookup::~AcceleratorLookup() = default;

// static
std::u16string AcceleratorLookup::GetAcceleratorDetailsText(
    AcceleratorDetails details) {
  std::u16string details_text;

  if (details.accelerator.IsCmdDown()) {
    std::u16string cmd_string =
        Shell::Get()->keyboard_capability()->HasLauncherButtonOnAnyKeyboard()
            ? l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_LAUNCHER_KEY)
            : l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_SEARCH_KEY);
    details_text = base::StrCat({details_text, cmd_string, kDetailsDelimiter});
  }

  if (details.accelerator.IsCtrlDown()) {
    details_text = base::StrCat(
        {details_text,
         l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_CONTROL_KEY),
         kDetailsDelimiter});
  }

  if (details.accelerator.IsAltDown()) {
    details_text = base::StrCat(
        {details_text, l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_ALT_KEY),
         kDetailsDelimiter});
  }

  if (details.accelerator.IsShiftDown()) {
    details_text = base::StrCat(
        {details_text, l10n_util::GetStringUTF16(IDS_ASH_ACCELERATOR_SHIFT_KEY),
         kDetailsDelimiter});
  }
  return base::StrCat({details_text, details.key_display});
}

std::vector<AcceleratorDetails> AcceleratorLookup::GetAcceleratorsForAction(
    uint32_t action) const {
  CHECK(ash_accelerator_configuration_);

  std::vector<AcceleratorDetails> details;
  OptionalAccelerators accelerators =
      ash_accelerator_configuration_->GetAcceleratorsForAction(action);
  if (!accelerators.has_value()) {
    return details;
  }

  for (const auto& accelerator : *accelerators) {
    details.push_back({accelerator, GetKeyDisplay(accelerator.key_code())});
  }

  return details;
}

std::vector<AcceleratorDetails>
AcceleratorLookup::GetAvailableAcceleratorsForAction(uint32_t action) const {
  CHECK(ash_accelerator_configuration_);

  std::vector<AcceleratorDetails> details;
  OptionalAccelerators accelerators =
      ash_accelerator_configuration_->GetAcceleratorsForAction(action);

  for (const auto& accelerator : *accelerators) {
    // Get the aliased and filtered accelerators associated for `accelerator`.
    // This ensures that clients will only fetch available accelerators.
    std::vector<ui::Accelerator> aliased_accelerators =
        alias_converter_.CreateAcceleratorAlias(accelerator);

    base::ranges::transform(
        aliased_accelerators, std::back_inserter(details),
        [](const ui::Accelerator& aliased_accelerator) {
          return AcceleratorDetails{
              aliased_accelerator,
              GetKeyDisplay(aliased_accelerator.key_code())};
        });
  }

  return details;
}

}  // namespace ash
