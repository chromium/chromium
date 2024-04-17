// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_pair/repository/fast_pair/proto_conversions.h"

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/proto/fastpair_data.pb.h"
#include "ash/quick_pair/repository/fast_pair/device_metadata.h"
#include "ash/quick_pair/repository/fast_pair_repository.h"
#include "base/memory/ref_counted_memory.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"

namespace ash {
namespace quick_pair {

nearby::fastpair::FastPairInfo BuildFastPairInfo(
    const std::string& hex_model_id,
    const std::vector<uint8_t>& account_key,
    const std::string& mac_address,
    const std::optional<std::string>& display_name,
    DeviceMetadata* device_metadata) {
  nearby::fastpair::FastPairInfo proto;
  auto* device = proto.mutable_device();
  device->set_account_key(std::string(account_key.begin(), account_key.end()));

  // Create a SHA256 hash of the |mac_address| with the |account_key| as salt.
  // The hash is used to identify devices via non discoverable advertisements.
  device->set_sha256_account_key_public_address(
      FastPairRepository::GenerateSha256OfAccountKeyAndMacAddress(
          std::string(account_key.begin(), account_key.end()), mac_address));

  auto& details = device_metadata->GetDetails();
  auto& strings = device_metadata->response().strings();
  nearby::fastpair::StoredDiscoveryItem discovery_item;
  discovery_item.set_id(hex_model_id);
  discovery_item.set_trigger_id(hex_model_id);

  if (ash::features::IsFastPairSavedDevicesNicknamesEnabled() &&
      display_name.has_value()) {
    discovery_item.set_title(display_name.value());
  } else {
    discovery_item.set_title(details.name());
  }

  discovery_item.set_description(strings.initial_notification_description());
  discovery_item.set_type(nearby::fastpair::NearbyType::NEARBY_DEVICE);
  discovery_item.set_action_url_type(nearby::fastpair::ResolvedUrlType::APP);
  discovery_item.set_action_url(details.intent_uri());
  discovery_item.set_last_observation_timestamp_millis(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  discovery_item.set_first_observation_timestamp_millis(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  discovery_item.set_state(nearby::fastpair::StoredDiscoveryItem_State::
                               StoredDiscoveryItem_State_STATE_ENABLED);

  auto image_memory = device_metadata->image().As1xPNGBytes();
  std::string png_encoded_image(base::as_string_view(*image_memory));
  discovery_item.set_icon_png(png_encoded_image);

  discovery_item.add_stored_relevances()->mutable_relevance()->set_evaluation(
      nearby::fastpair::Evaluation::EVALUATION_GREAT);
  discovery_item.set_last_user_experience(
      nearby::fastpair::StoredDiscoveryItem_ExperienceType::
          StoredDiscoveryItem_ExperienceType_EXPERIENCE_GOOD);
  if (details.has_anti_spoofing_key_pair()) {
    discovery_item.set_authentication_public_key_secp256r1(
        details.anti_spoofing_key_pair().public_key());
  }
  if (details.has_true_wireless_images()) {
    auto* images = discovery_item.mutable_fast_pair_information()
                       ->mutable_true_wireless_images();
    images->set_left_bud_url(details.true_wireless_images().left_bud_url());
    images->set_right_bud_url(details.true_wireless_images().right_bud_url());
    images->set_case_url(details.true_wireless_images().case_url());
  }
  discovery_item.mutable_fast_pair_information()->set_assistant_supported(
      details.assistant_supported());
  discovery_item.mutable_fast_pair_information()->set_company_name(
      details.company_name());
  discovery_item.mutable_fast_pair_information()->set_device_type(
      details.device_type());

  auto* out_strings = discovery_item.mutable_fast_pair_strings();
  out_strings->set_tap_to_pair_with_account(
      strings.initial_notification_description());
  out_strings->set_tap_to_pair_without_account(
      strings.initial_notification_description_no_account());
  out_strings->set_initial_pairing_description(
      strings.initial_pairing_description());
  out_strings->set_pairing_finished_companion_app_installed(
      strings.connect_success_companion_app_installed());
  out_strings->set_pairing_finished_companion_app_not_installed(
      strings.connect_success_companion_app_not_installed());
  out_strings->set_subsequent_pairing_description(
      strings.subsequent_pairing_description());
  out_strings->set_retroactive_pairing_description(
      strings.retroactive_pairing_description());
  out_strings->set_wait_app_launch_description(
      strings.wait_launch_companion_app_description());
  out_strings->set_pairing_fail_description(
      strings.fail_connect_go_to_settings_description());
  out_strings->set_confirm_pin_title(strings.confirm_pin_title());
  out_strings->set_confirm_pin_description(strings.confirm_pin_description());
  out_strings->set_sync_contacts_title(strings.sync_contacts_title());
  out_strings->set_sync_contacts_description(
      strings.sync_contacts_description());
  out_strings->set_sync_sms_title(strings.sync_sms_title());
  out_strings->set_sync_sms_description(strings.sync_sms_description());

  device->set_discovery_item_bytes(discovery_item.SerializeAsString());

  return proto;
}

nearby::fastpair::FastPairInfo BuildFastPairInfoForOptIn(
    nearby::fastpair::OptInStatus opt_in_status) {
  nearby::fastpair::FastPairInfo proto;
  proto.set_opt_in_status(opt_in_status);
  return proto;
}

}  // namespace quick_pair
}  // namespace ash
