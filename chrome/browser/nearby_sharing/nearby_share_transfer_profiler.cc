// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"

#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/nearby_share_metrics.h"
#include "components/cross_device/logging/logging.h"

base::TimeDelta ComputeDelta(const base::TimeTicks& start,
                             const base::TimeTicks& end) {
  // The delta between start and end times must be non-negative.
  // Note: Due to how time works in unittests, they may be equal.
  CHECK(end >= start);
  return end - start;
}

NearbyShareTransferProfiler::NearbyShareTransferProfiler() = default;
NearbyShareTransferProfiler::~NearbyShareTransferProfiler() = default;

void NearbyShareTransferProfiler::OnEndpointDiscovered(
    const std::string& endpoint_id) {
  // It is possible for an endpoint to be rediscovered before it is lost, so
  // reset any existing state associated with it. The can easily occur when a
  // transfer is requested, the dialog is closed after the endpoint is
  // discovered, and then a new transfer is requested before the endpoint is
  // lost.
  SenderData new_data;
  new_data.discovered_time = base::TimeTicks::Now();
  auto [it, inserted] = sender_data_.insert_or_assign(endpoint_id, new_data);
  if (!inserted) {
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] rediscovered";
  }
}

void NearbyShareTransferProfiler::OnEndpointLost(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered before it can be lost.
  auto it = sender_data_.find(endpoint_id);
  CHECK(it != sender_data_.end());
  CHECK(it->second.discovered_time.has_value());
  sender_data_.erase(it);
}

void NearbyShareTransferProfiler::OnOutgoingEndpointDecoded(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered before it can be decoded.
  auto it = sender_data_.find(endpoint_id);
  CHECK(it != sender_data_.end());
  CHECK(it->second.discovered_time.has_value());
  // The endpoint should not have been connected yet, since it must be decoded
  // in order to connect.
  CHECK(!it->second.connection_established_time.has_value());

  // An endpoint should only be decoded once.
  CHECK(!it->second.endpoint_decoded_time.has_value());
  it->second.endpoint_decoded_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnShareTargetSelected(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered and decoded before it can be
  // selected. Note: A connection may have been established prior, in the case
  // where the same endpoint is used for multiple transfers without redisovery.
  auto it = sender_data_.find(endpoint_id);
  CHECK(it != sender_data_.end());
  CHECK(it->second.discovered_time.has_value());

  // An endpoint should only be selected once.
  CHECK(!it->second.share_target_selected_time.has_value());
  it->second.share_target_selected_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnConnectionEstablished(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered, decoded, and selected before a
  // connection can be established.
  auto it = sender_data_.find(endpoint_id);
  CHECK(it != sender_data_.end());
  SenderData& sender_data = it->second;
  CHECK(sender_data.discovered_time.has_value());
  CHECK(sender_data.endpoint_decoded_time.has_value());
  CHECK(sender_data.share_target_selected_time.has_value());

  // Note: the same receiver can be reused for subsequent shares without
  // triggering the discovery logic.
  bool is_first_connection =
      !sender_data.connection_established_time.has_value();
  sender_data.connection_established_time = base::TimeTicks::Now();

  // Only log the time to first connection, since subsequent connections without
  // rediscovery could dramatically pollute these metrics.
  if (is_first_connection) {
    // Compute and log the time delta between when the endpoint was first
    // discovered and when the connection was established. This provides insight
    // into how long it takes for a sender to detect a receiver, select it, and
    // connect to it. Note: this delta includes user interaction, so it may vary
    // significantly for each connection.
    base::TimeDelta discovery_delta = ComputeDelta(
        *sender_data.discovered_time, *sender_data.connection_established_time);
    RecordNearbyShareDiscoveredToConnectionEstablishedDuration(discovery_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] discovery to first connection established: " << discovery_delta;
  }
}

void NearbyShareTransferProfiler::OnIntroductionFrameSent(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered, decoded, selected, and connected
  // before an introduction frame can be sent.
  auto it = sender_data_.find(endpoint_id);
  if (it == sender_data_.end() || !it->second.discovered_time.has_value() ||
      !it->second.endpoint_decoded_time.has_value() ||
      !it->second.share_target_selected_time.has_value() ||
      !it->second.connection_established_time.has_value()) {
    return;
  }

  SenderData& sender_data = it->second;
  // Note: the same receiver can be reused for subsequent shares without
  // triggering the discovery logic.
  bool is_first_introduction = !sender_data.introduction_sent_time.has_value();
  sender_data.introduction_sent_time = base::TimeTicks::Now();

  // Only log the time to first introduction, since subsequent introductions
  // without rediscovery could dramatically pollute these metrics.
  if (is_first_introduction) {
    // Compute and log the time delta between when the outgoing endpoint was
    // decoded and when the introduction frame was sent. This provides insight
    // into how long it takes to notify the receiver that it has been selected.
    // Note: this delta includes user interaction, so it may vary significantly
    // for each transfer.
    base::TimeDelta introduction_delta =
        ComputeDelta(*sender_data.endpoint_decoded_time,
                     *sender_data.introduction_sent_time);
    // We do not log this to UMA currently, since the user interaction timing
    // will make it unacceptably noisy.
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] decode to introduction sent: " << introduction_delta;
  }
  // Compute and log the time delta between when the user selects the endpoint
  // as a share target and when the introduction frame was sent. This provides
  // insight into how long it takes to establish a connection and notify the
  // receiver.
  base::TimeDelta selection_delta =
      ComputeDelta(*sender_data.share_target_selected_time,
                   *sender_data.introduction_sent_time);
  RecordNearbyShareInitiatedToSentIntroductionFrameDuration(selection_delta);
  CD_LOG(INFO, Feature::NS)
      << "Endpoint [" << endpoint_id
      << "] share target selected to introduction sent: " << selection_delta;
}

void NearbyShareTransferProfiler::OnSendStart(const std::string& endpoint_id) {
  // The endpoint must have been discovered, decoded, selected, connected, and
  // introduced before transfers can be started.
  auto it = sender_data_.find(endpoint_id);
  if (it == sender_data_.end() || !it->second.discovered_time.has_value() ||
      !it->second.endpoint_decoded_time.has_value() ||
      !it->second.share_target_selected_time.has_value() ||
      !it->second.connection_established_time.has_value() ||
      !it->second.introduction_sent_time.has_value()) {
    return;
  }

  // Transfers should only be started once.
  if (it->second.send_start_time.has_value()) {
    return;
  }
  it->second.send_start_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnSendComplete(
    const std::string& endpoint_id,
    TransferMetadata::Status status) {
  // Only log completion metrics for successful transfers.
  if (status == TransferMetadata::Status::kComplete) {
    // The endpoint must have been discovered, decoded, selected, connected,
    // introduced, and the sending must have started before a transfer can
    // successfully complete.
    auto it = sender_data_.find(endpoint_id);
    CHECK(it != sender_data_.end());
    SenderData& sender_data = it->second;
    CHECK(sender_data.discovered_time.has_value());
    CHECK(sender_data.endpoint_decoded_time.has_value());
    CHECK(sender_data.share_target_selected_time.has_value());
    CHECK(sender_data.connection_established_time.has_value());
    CHECK(sender_data.introduction_sent_time.has_value());
    CHECK(sender_data.send_start_time.has_value());

    base::TimeTicks now = base::TimeTicks::Now();

    // A bandwidth upgrade is not guaranteed to have been successful before the
    // transfer is complete.
    bool has_bandwidth_upgraded =
        sender_data.bandwidth_upgrade_time.has_value();
    if (has_bandwidth_upgraded) {
      // There should be an upgraded medium recoded.
      CHECK(sender_data.upgraded_medium.has_value());

      // Compute and log the time delta between when the first bandwidth upgrade
      // completes and when transfer finishes. This provides insight into how
      // long the transfer takes to complete for a given transfer medium.
      base::TimeDelta upgrade_delta =
          ComputeDelta(*sender_data.bandwidth_upgrade_time, now);
      RecordNearbyShareBandwidthUpgradeToAllFilesSentDuration(
          *sender_data.upgraded_medium, upgrade_delta);
      CD_LOG(INFO, Feature::NS)
          << "Endpoint [" << endpoint_id << "] bandwidth upgrade ("
          << *sender_data.upgraded_medium
          << ") to transfer complete: " << upgrade_delta;
    }

    // Compute and log the time delta between when the sender starts sending
    // data and when transfer finishes. This provides insight into the time a
    // transfer takes to complete.
    base::TimeDelta completion_delta =
        ComputeDelta(*sender_data.send_start_time, now);
    RecordNearbyShareStartSendFilesToAllFilesSentDuration(completion_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] file sent to transfer complete: " << completion_delta;

    // Compute and log the time delta between when the user has selected a
    // receiver and when transfer finishes. This provides insight into the
    // perceived user time the nearby share process takes to complete for the
    // sender. Note: this delta includes user interaction (the receiver needs to
    // accept the transfer), so it may vary significantly for each transfer.
    base::TimeDelta total_delta =
        ComputeDelta(*sender_data.share_target_selected_time, now);
    RecordNearbyShareInitiatedToAllFilesSentDuration(total_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] share target selected to transfer complete: " << total_delta;
  }

  // Reset the endpoint using the current discovery, decode, connection, and
  // introduction timestamps since they could be reused for subsequent
  // transfers.
  SenderData& sender_data = sender_data_[endpoint_id];
  sender_data = SenderData{
      .discovered_time = sender_data.discovered_time,
      .endpoint_decoded_time = sender_data.endpoint_decoded_time,
      .connection_established_time =
          sender_data.connection_established_time,
      .introduction_sent_time =
          sender_data.introduction_sent_time,
  };
}

void NearbyShareTransferProfiler::OnIncomingEndpointDecoded(
    const std::string& endpoint_id,
    bool is_high_visibility) {
  auto [it, insert_succeeded] =
      receiver_data_.try_emplace(endpoint_id, ReceiverData());
  // An endpoint should only be decoded once. Ignore if the endpoint was
  // already in the map.
  if (!insert_succeeded) {
    return;
  }
  ReceiverData& receiver_data = it->second;
  receiver_data.endpoint_decoded_time = base::TimeTicks::Now();

  // High-visibility endpoints emit different metrics, so save this for later
  // use.
  if (is_high_visibility) {
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] in high-visibility mode";
    receiver_data.is_high_visibility = true;
  }
}

void NearbyShareTransferProfiler::OnPairedKeyHandshakeComplete(
    const std::string& endpoint_id) {
  // The endpoint must have been decoded before a handshake can be complete.
  auto it = receiver_data_.find(endpoint_id);
  CHECK(it != receiver_data_.end());
  CHECK(it->second.endpoint_decoded_time.has_value());

  // Only one handshake should be completed for each transfer.
  CHECK(!it->second.paired_key_handshake_time.has_value());
  it->second.paired_key_handshake_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnIntroductionFrameReceived(
    const std::string& endpoint_id) {
  // The endpoint must have been decoded and the handshake completed before an
  // introduction frame can be received.
  auto it = receiver_data_.find(endpoint_id);
  CHECK(it != receiver_data_.end());
  ReceiverData& receiver_data = it->second;
  CHECK(receiver_data.endpoint_decoded_time.has_value());
  CHECK(receiver_data.paired_key_handshake_time.has_value());

  // Only one introduction frame should be received.
  CHECK(!receiver_data.introduction_received_time.has_value());
  receiver_data.introduction_received_time = base::TimeTicks::Now();

  // Compute and log the time delta between when the receiver decodes the
  // incoming endpoint and when the introduction frame was received. This
  // provides insight into how long the paired key handshake takes to complete
  // and be acknowledged.
  base::TimeDelta introduction_delta =
      ComputeDelta(*receiver_data.endpoint_decoded_time,
                   *receiver_data.introduction_received_time);
  RecordNearbyShareEndpointDecodedToReceivedIntroductionFrameDuration(
      introduction_delta);
  CD_LOG(INFO, Feature::NS)
      << "Endpoint [" << endpoint_id
      << "] decode to introduction received: " << introduction_delta;
}
void NearbyShareTransferProfiler::OnTransferAccepted(
    const std::string& endpoint_id) {
  // The endpoint must have been decoded and introduced before a transfer can be
  // accepted.
  auto it = receiver_data_.find(endpoint_id);
  if (it == receiver_data_.end() ||
      !it->second.endpoint_decoded_time.has_value() ||
      !it->second.introduction_received_time.has_value()) {
    return;
  }

  // A transfer can only be accepted once.
  if (it->second.accept_transfer_time.has_value()) {
    return;
  }
  it->second.accept_transfer_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnReceiveComplete(
    const std::string& endpoint_id,
    TransferMetadata::Status status) {
  // Only log completion metrics for successful transfers.
  if (status == TransferMetadata::Status::kComplete) {
    // The endpoint must have been decoded, introduced, and accepted before a
    // transfer can be completed successfully.
    auto it = receiver_data_.find(endpoint_id);
    CHECK(it != receiver_data_.end());
    auto& receiver_data = it->second;
    CHECK(receiver_data.endpoint_decoded_time.has_value());
    CHECK(receiver_data.introduction_received_time.has_value());
    CHECK(receiver_data.accept_transfer_time.has_value());

    base::TimeTicks now = base::TimeTicks::Now();

    // A bandwidth upgrade is not guaranteed to have been successful before the
    // transfer is complete.
    bool has_bandwidth_upgraded =
        receiver_data.bandwidth_upgrade_time.has_value();
    if (has_bandwidth_upgraded) {
      // There should be an upgraded medium recoded.
      CHECK(receiver_data.upgraded_medium.has_value());

      // Compute and log the time delta between when the first bandwidth upgrade
      // completes and when the transfer finishes. This provides insight into
      // how long the transfer takes to complete for a given transfer medium.
      base::TimeDelta upgrade_delta =
          ComputeDelta(*receiver_data.bandwidth_upgrade_time, now);
      RecordNearbyShareBandwidthUpgradeToAllFilesReceivedDuration(
          *receiver_data.upgraded_medium, upgrade_delta);
      CD_LOG(INFO, Feature::NS)
          << "Endpoint [" << endpoint_id << "] bandwidth upgrade ("
          << *receiver_data.upgraded_medium
          << ") to transfer complete: " << upgrade_delta;
    }

    // Compute and log the time delta between when the user accepts the transfer
    // and when the transfer finishes. This provides insight into the time a
    // transfer takes to complete.
    base::TimeDelta completion_delta =
        ComputeDelta(*receiver_data.accept_transfer_time, now);
    RecordNearbyShareAcceptedTransferToAllFilesReceivedDuration(
        completion_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] transfer accepted to transfer complete: " << completion_delta;

    // Compute and log the time delta between when the receiver has verified the
    // connection and when the transfer finishes. This provides insight into the
    // perceived user time the entire nearby share process takes to complete for
    // the receiver.
    base::TimeDelta total_delta =
        ComputeDelta(*receiver_data.introduction_received_time, now);
    RecordNearbyShareReceivedIntroductionFrameToAllFilesReceivedDuration(
        total_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] introduction to transfer complete: " << total_delta;
  }

  // Reset the endpoint since the transfer is complete.
  receiver_data_.erase(endpoint_id);
}

void NearbyShareTransferProfiler::OnBandwidthUpgrade(
    const std::string& endpoint_id,
    nearby::connections::mojom::Medium medium) {
  // An endpoint must either be a sender or a receiver.
  auto sender_it = sender_data_.find(endpoint_id);
  auto receiver_it = receiver_data_.find(endpoint_id);
  bool is_sender = sender_it != sender_data_.end();
  bool is_receiver = receiver_it != receiver_data_.end();

  if (!is_sender && !is_receiver) {
    // It is possible for the |endpoint_id| to be in neither |sender_data_| nor
    // |receiver_data_| if, e.g. the transfer is cancelled just before the
    // bandwidth upgrade completes.
    return;
  }

  // An endpoint cannot be both a sender and a receiver.
  if (is_sender && is_receiver) {
    return;
  }

  if (is_sender) {
    // A connection must have been established at this point.
    if (!sender_it->second.connection_established_time.has_value()) {
      return;
    }

    // Bandwidth upgrades are requested a number of times, so it is possible for
    // more than one upgrade to complete successfully.
    bool is_first_upgrade =
        !sender_it->second.bandwidth_upgrade_time.has_value();
    sender_it->second.upgraded_medium = medium;
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] upgraded bandwidth to " << medium;

    // Only log metrics the first time bandwidth upgrade is successful, since
    // subsequent upgrades could pollute these metrics.
    if (is_first_upgrade) {
      // Only store the first time a bandwidth upgrade occurred.
      sender_it->second.bandwidth_upgrade_time = base::TimeTicks::Now();

      // Compute and log the time delta between when the sender establishes a
      // connection and when the first bandwidth upgrade occurs. This provides
      // insight into how long a bandwidth upgrade takes to complete from the
      // sender side.
      base::TimeDelta upgrade_delta =
          ComputeDelta(*sender_it->second.connection_established_time,
                       *sender_it->second.bandwidth_upgrade_time);
      RecordNearbyShareConnectionEstablishedToBandwidthUpgradeDuration(
          medium, upgrade_delta);
      CD_LOG(INFO, Feature::NS) << "Endpoint [" << endpoint_id
                                << "] connection to bandwidth upgrade ("
                                << medium << "): " << upgrade_delta;
    }
  } else {
    // Bandwidth upgrades are requested a number of times, so it is possible for
    // more than one upgrade to complete successfully.
    bool is_first_upgrade =
        !receiver_it->second.bandwidth_upgrade_time.has_value();
    receiver_it->second.upgraded_medium = medium;
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] upgraded bandwidth to " << medium;

    // Only log metrics the first time bandwidth upgrade is successful, since
    // subsequent upgrades could pollute these metrics.
    if (is_first_upgrade) {
      // Only store the first time a bandwidth upgrade occurred.
      receiver_it->second.bandwidth_upgrade_time = base::TimeTicks::Now();

      // Different upgrade metrics should be logged in high visibility mode.
      if (receiver_it->second.is_high_visibility) {
        // The endpoint must have been decoded at this point.
        if (!receiver_it->second.endpoint_decoded_time.has_value()) {
          return;
        }

        // Compute and log the time delta between when the receiver decodes the
        // incoming endpoint and when the first bandwidth upgrade occurs. This
        // is a parallel to the sender upgrade metric and provides insight into
        // how long a bandwidth upgrade takes to complete from the receiver
        // side.
        base::TimeDelta upgrade_delta =
            ComputeDelta(*receiver_it->second.endpoint_decoded_time,
                         *receiver_it->second.bandwidth_upgrade_time);
        RecordNearbyShareHighVisibilityEndpointDecodedToBandwidthUpgradeDuration(
            medium, upgrade_delta);
        CD_LOG(INFO, Feature::NS)
            << "Endpoint [" << endpoint_id << "] decode to bandwidth upgrade ("
            << medium << "): " << upgrade_delta;
      } else {
        // The handshake must have occurred at this point.
        if (!receiver_it->second.paired_key_handshake_time.has_value()) {
          return;
        }

        // Compute and log the time delta between when the paired key handshake
        // completes and when the first bandwidth upgrade occurs. This provides
        // insight into how long a bandwidth upgrade takes to complete for a
        // non-high visibility mode receiver.
        base::TimeDelta handshake_delta = ComputeDelta(
            *receiver_it->second.paired_key_handshake_time,
            *receiver_it->second.bandwidth_upgrade_time);
        RecordNearbyShareNonHighVisibilityPairedKeyCompleteToBandwidthUpgradeDuration(
            medium, handshake_delta);
        CD_LOG(INFO, Feature::NS)
            << "Endpoint [" << endpoint_id
            << "] handshake complete to bandwidth upgrade (" << medium
            << "): " << handshake_delta;
      }
    }
  }
}
