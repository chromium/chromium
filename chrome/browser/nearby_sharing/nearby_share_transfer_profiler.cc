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

NearbyShareTransferProfiler::NearbyShareTransferProfiler() {}
NearbyShareTransferProfiler::~NearbyShareTransferProfiler() {}

void NearbyShareTransferProfiler::OnEndpointDiscovered(
    const std::string& endpoint_id) {
  // It is possible for an endpoint to be rediscovered before it is lost, so
  // reset any existing state associated with it. The can easily occur when a
  // transfer is requested, the dialog is closed after the endpoint is
  // discovered, and then a new transfer is requested before the endpoint is
  // lost.
  if (sender_data_.contains(endpoint_id)) {
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] rediscovered";
    sender_data_.erase(endpoint_id);
  }

  // There should not be data associated with an endpoint before it is
  // discovered.
  CHECK(!sender_data_.contains(endpoint_id));
  sender_data_[endpoint_id].discovered_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnEndpointLost(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered before it can be lost.
  CHECK(sender_data_.contains(endpoint_id));
  CHECK(sender_data_[endpoint_id].discovered_time.has_value());
  sender_data_.erase(endpoint_id);
}

void NearbyShareTransferProfiler::OnOutgoingEndpointDecoded(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered before it can be decoded.
  CHECK(sender_data_.contains(endpoint_id));
  CHECK(sender_data_[endpoint_id].discovered_time.has_value());
  // The endpoint should not have been connected yet, since it must be decoded
  // in order to connect.
  CHECK(!sender_data_[endpoint_id].connection_established_time.has_value());

  // An endpoint should only be decoded once.
  CHECK(!sender_data_[endpoint_id].endpoint_decoded_time.has_value());
  sender_data_[endpoint_id].endpoint_decoded_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnShareTargetSelected(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered and decoded before it can be
  // selected. Note: A connection may have been established prior, in the case
  // where the same endpoint is used for multiple transfers without redisovery.
  CHECK(sender_data_.contains(endpoint_id));
  CHECK(sender_data_[endpoint_id].discovered_time.has_value());

  // An endpoint should only be selected once.
  CHECK(!sender_data_[endpoint_id].share_target_selected_time.has_value());
  sender_data_[endpoint_id].share_target_selected_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnConnectionEstablished(
    const std::string& endpoint_id) {
  // The endpoint must have been discovered, decoded, and selected before a
  // connection can be established.
  CHECK(sender_data_.contains(endpoint_id));
  CHECK(sender_data_[endpoint_id].discovered_time.has_value());
  CHECK(sender_data_[endpoint_id].endpoint_decoded_time.has_value());
  CHECK(sender_data_[endpoint_id].share_target_selected_time.has_value());

  // Note: the same receiver can be reused for subsequent shares without
  // triggering the discovery logic.
  bool is_first_connection =
      !sender_data_[endpoint_id].connection_established_time.has_value();
  sender_data_[endpoint_id].connection_established_time =
      base::TimeTicks::Now();

  // Only log the time to first connection, since subsequent connections without
  // rediscovery could dramatically pollute these metrics.
  if (is_first_connection) {
    // Compute and log the time delta between when the endpoint was first
    // discovered and when the connection was established. This provides insight
    // into how long it takes for a sender to detect a receiver, select it, and
    // connect to it. Note: this delta includes user interaction, so it may vary
    // significantly for each connection.
    base::TimeDelta discovery_delta = ComputeDelta(
        sender_data_[endpoint_id].discovered_time.value(),
        sender_data_[endpoint_id].connection_established_time.value());
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
  if (!sender_data_.contains(endpoint_id) ||
      !sender_data_[endpoint_id].discovered_time.has_value() ||
      !sender_data_[endpoint_id].endpoint_decoded_time.has_value() ||
      !sender_data_[endpoint_id].share_target_selected_time.has_value() ||
      !sender_data_[endpoint_id].connection_established_time.has_value()) {
    return;
  }

  // Note: the same receiver can be reused for subsequent shares without
  // triggering the discovery logic.
  bool is_first_introduction =
      !sender_data_[endpoint_id].introduction_sent_time.has_value();
  sender_data_[endpoint_id].introduction_sent_time = base::TimeTicks::Now();

  // Only log the time to first introduction, since subsequent introductions
  // without rediscovery could dramatically pollute these metrics.
  if (is_first_introduction) {
    // Compute and log the time delta between when the outgoing endpoint was
    // decoded and when the introduction frame was sent. This provides insight
    // into how long it takes to notify the receiver that it has been selected.
    // Note: this delta includes user interaction, so it may vary significantly
    // for each transfer.
    base::TimeDelta introduction_delta =
        ComputeDelta(sender_data_[endpoint_id].endpoint_decoded_time.value(),
                     sender_data_[endpoint_id].introduction_sent_time.value());
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
      ComputeDelta(sender_data_[endpoint_id].share_target_selected_time.value(),
                   sender_data_[endpoint_id].introduction_sent_time.value());
  RecordNearbyShareInitiatedToSentIntroductionFrameDuration(selection_delta);
  CD_LOG(INFO, Feature::NS)
      << "Endpoint [" << endpoint_id
      << "] share target selected to introduction sent: " << selection_delta;
}

void NearbyShareTransferProfiler::OnSendStart(const std::string& endpoint_id) {
  // The endpoint must have been discovered, decoded, selected, connected, and
  // introduced before transfers can be started.
  if (!sender_data_.contains(endpoint_id) ||
      !sender_data_[endpoint_id].discovered_time.has_value() ||
      !sender_data_[endpoint_id].endpoint_decoded_time.has_value() ||
      !sender_data_[endpoint_id].share_target_selected_time.has_value() ||
      !sender_data_[endpoint_id].connection_established_time.has_value() ||
      !sender_data_[endpoint_id].introduction_sent_time.has_value()) {
    return;
  }

  // Transfers should only be started once.
  if (sender_data_[endpoint_id].send_start_time.has_value()) {
    return;
  }
  sender_data_[endpoint_id].send_start_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnSendComplete(
    const std::string& endpoint_id,
    TransferMetadata::Status status) {
  // Only log completion metrics for successful transfers.
  if (status == TransferMetadata::Status::kComplete) {
    // The endpoint must have been discovered, decoded, selected, connected,
    // introduced, and the sending must have started before a transfer can
    // successfully complete.
    CHECK(sender_data_.contains(endpoint_id));
    CHECK(sender_data_[endpoint_id].discovered_time.has_value());
    CHECK(sender_data_[endpoint_id].endpoint_decoded_time.has_value());
    CHECK(sender_data_[endpoint_id].share_target_selected_time.has_value());
    CHECK(sender_data_[endpoint_id].connection_established_time.has_value());
    CHECK(sender_data_[endpoint_id].introduction_sent_time.has_value());
    CHECK(sender_data_[endpoint_id].send_start_time.has_value());

    base::TimeTicks now = base::TimeTicks::Now();

    // A bandwidth upgrade is not guaranteed to have been successful before the
    // transfer is complete.
    bool has_bandwidth_upgraded =
        sender_data_[endpoint_id].bandwidth_upgrade_time.has_value();
    if (has_bandwidth_upgraded) {
      // There should be an upgraded medium recoded.
      CHECK(sender_data_[endpoint_id].upgraded_medium.has_value());

      // Compute and log the time delta between when the first bandwidth upgrade
      // completes and when transfer finishes. This provides insight into how
      // long the transfer takes to complete for a given transfer medium.
      base::TimeDelta upgrade_delta = ComputeDelta(
          sender_data_[endpoint_id].bandwidth_upgrade_time.value(), now);
      RecordNearbyShareBandwidthUpgradeToAllFilesSentDuration(
          sender_data_[endpoint_id].upgraded_medium.value(), upgrade_delta);
      CD_LOG(INFO, Feature::NS)
          << "Endpoint [" << endpoint_id << "] bandwidth upgrade ("
          << sender_data_[endpoint_id].upgraded_medium.value()
          << ") to transfer complete: " << upgrade_delta;
    }

    // Compute and log the time delta between when the sender starts sending
    // data and when transfer finishes. This provides insight into the time a
    // transfer takes to complete.
    base::TimeDelta completion_delta =
        ComputeDelta(sender_data_[endpoint_id].send_start_time.value(), now);
    RecordNearbyShareStartSendFilesToAllFilesSentDuration(completion_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] file sent to transfer complete: " << completion_delta;

    // Compute and log the time delta between when the user has selected a
    // receiver and when transfer finishes. This provides insight into the
    // perceived user time the nearby share process takes to complete for the
    // sender. Note: this delta includes user interaction (the receiver needs to
    // accept the transfer), so it may vary significantly for each transfer.
    base::TimeDelta total_delta = ComputeDelta(
        sender_data_[endpoint_id].share_target_selected_time.value(), now);
    RecordNearbyShareInitiatedToAllFilesSentDuration(total_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] share target selected to transfer complete: " << total_delta;
  }

  // Reset the endpoint using the current discovery, decode, connection, and
  // introduction timestamps since they could be reused for subsequent
  // transfers.
  sender_data_[endpoint_id] = SenderData{
      .discovered_time = sender_data_[endpoint_id].discovered_time,
      .endpoint_decoded_time = sender_data_[endpoint_id].endpoint_decoded_time,
      .connection_established_time =
          sender_data_[endpoint_id].connection_established_time,
      .introduction_sent_time =
          sender_data_[endpoint_id].introduction_sent_time,
  };
}

void NearbyShareTransferProfiler::OnIncomingEndpointDecoded(
    const std::string& endpoint_id,
    bool is_high_visibility) {
  // An endpoint should only be decoded once.
  if (receiver_data_.contains(endpoint_id)) {
    return;
  }
  receiver_data_[endpoint_id].endpoint_decoded_time = base::TimeTicks::Now();

  // High-visibility endpoints emit different metrics, so save this for later
  // use.
  if (is_high_visibility) {
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] in high-visibility mode";
    receiver_data_[endpoint_id].is_high_visibility = true;
  }
}

void NearbyShareTransferProfiler::OnPairedKeyHandshakeComplete(
    const std::string& endpoint_id) {
  // The endpoint must have been decoded before a handshake can be complete.
  CHECK(receiver_data_.contains(endpoint_id));
  CHECK(receiver_data_[endpoint_id].endpoint_decoded_time.has_value());

  // Only one handshake should be completed for each transfer.
  CHECK(!receiver_data_[endpoint_id].paired_key_handshake_time.has_value());
  receiver_data_[endpoint_id].paired_key_handshake_time =
      base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnIntroductionFrameReceived(
    const std::string& endpoint_id) {
  // The endpoint must have been decoded and the handshake completed before an
  // introduction frame can be received.
  CHECK(receiver_data_.contains(endpoint_id));
  CHECK(receiver_data_[endpoint_id].endpoint_decoded_time.has_value());
  CHECK(receiver_data_[endpoint_id].paired_key_handshake_time.has_value());

  // Only one introduction frame should be received.
  CHECK(!receiver_data_[endpoint_id].introduction_received_time.has_value());
  receiver_data_[endpoint_id].introduction_received_time =
      base::TimeTicks::Now();

  // Compute and log the time delta between when the receiver decodes the
  // incoming endpoint and when the introduction frame was received. This
  // provides insight into how long the paired key handshake takes to complete
  // and be acknowledged.
  base::TimeDelta introduction_delta = ComputeDelta(
      receiver_data_[endpoint_id].endpoint_decoded_time.value(),
      receiver_data_[endpoint_id].introduction_received_time.value());
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
  if (!receiver_data_.contains(endpoint_id) ||
      !receiver_data_[endpoint_id].endpoint_decoded_time.has_value() ||
      !receiver_data_[endpoint_id].introduction_received_time.has_value()) {
    return;
  }

  // A transfer can only be accepted once.
  if (receiver_data_[endpoint_id].accept_transfer_time.has_value()) {
    return;
  }
  receiver_data_[endpoint_id].accept_transfer_time = base::TimeTicks::Now();
}

void NearbyShareTransferProfiler::OnReceiveComplete(
    const std::string& endpoint_id,
    TransferMetadata::Status status) {
  // Only log completion metrics for successful transfers.
  if (status == TransferMetadata::Status::kComplete) {
    // The endpoint must have been decoded, introduced, and accepted before a
    // transfer can be completed successfully.
    CHECK(receiver_data_.contains(endpoint_id));
    CHECK(receiver_data_[endpoint_id].endpoint_decoded_time.has_value());
    CHECK(receiver_data_[endpoint_id].introduction_received_time.has_value());
    CHECK(receiver_data_[endpoint_id].accept_transfer_time.has_value());

    base::TimeTicks now = base::TimeTicks::Now();

    // A bandwidth upgrade is not guaranteed to have been successful before the
    // transfer is complete.
    bool has_bandwidth_upgraded =
        receiver_data_[endpoint_id].bandwidth_upgrade_time.has_value();
    if (has_bandwidth_upgraded) {
      // There should be an upgraded medium recoded.
      CHECK(receiver_data_[endpoint_id].upgraded_medium.has_value());

      // Compute and log the time delta between when the first bandwidth upgrade
      // completes and when the transfer finishes. This provides insight into
      // how long the transfer takes to complete for a given transfer medium.
      base::TimeDelta upgrade_delta = ComputeDelta(
          receiver_data_[endpoint_id].bandwidth_upgrade_time.value(), now);
      RecordNearbyShareBandwidthUpgradeToAllFilesReceivedDuration(
          receiver_data_[endpoint_id].upgraded_medium.value(), upgrade_delta);
      CD_LOG(INFO, Feature::NS)
          << "Endpoint [" << endpoint_id << "] bandwidth upgrade ("
          << receiver_data_[endpoint_id].upgraded_medium.value()
          << ") to transfer complete: " << upgrade_delta;
    }

    // Compute and log the time delta between when the user accepts the transfer
    // and when the transfer finishes. This provides insight into the time a
    // transfer takes to complete.
    base::TimeDelta completion_delta = ComputeDelta(
        receiver_data_[endpoint_id].accept_transfer_time.value(), now);
    RecordNearbyShareAcceptedTransferToAllFilesReceivedDuration(
        completion_delta);
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id
        << "] transfer accepted to transfer complete: " << completion_delta;

    // Compute and log the time delta between when the receiver has verified the
    // connection and when the transfer finishes. This provides insight into the
    // perceived user time the entire nearby share process takes to complete for
    // the receiver.
    base::TimeDelta total_delta = ComputeDelta(
        receiver_data_[endpoint_id].introduction_received_time.value(), now);
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
  bool is_sender = sender_data_.contains(endpoint_id);
  bool is_receiver = receiver_data_.contains(endpoint_id);

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
    if (!sender_data_[endpoint_id].connection_established_time.has_value()) {
      return;
    }

    // Bandwidth upgrades are requested a number of times, so it is possible for
    // more than one upgrade to complete successfully.
    bool is_first_upgrade =
        !sender_data_[endpoint_id].bandwidth_upgrade_time.has_value();
    sender_data_[endpoint_id].upgraded_medium = medium;
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] upgraded bandwidth to " << medium;

    // Only log metrics the first time bandwidth upgrade is successful, since
    // subsequent upgrades could pollute these metrics.
    if (is_first_upgrade) {
      // Only store the first time a bandwidth upgrade occurred.
      sender_data_[endpoint_id].bandwidth_upgrade_time = base::TimeTicks::Now();

      // Compute and log the time delta between when the sender establishes a
      // connection and when the first bandwidth upgrade occurs. This provides
      // insight into how long a bandwidth upgrade takes to complete from the
      // sender side.
      base::TimeDelta upgrade_delta = ComputeDelta(
          sender_data_[endpoint_id].connection_established_time.value(),
          sender_data_[endpoint_id].bandwidth_upgrade_time.value());
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
        !receiver_data_[endpoint_id].bandwidth_upgrade_time.has_value();
    receiver_data_[endpoint_id].upgraded_medium = medium;
    CD_LOG(INFO, Feature::NS)
        << "Endpoint [" << endpoint_id << "] upgraded bandwidth to " << medium;

    // Only log metrics the first time bandwidth upgrade is successful, since
    // subsequent upgrades could pollute these metrics.
    if (is_first_upgrade) {
      // Only store the first time a bandwidth upgrade occurred.
      receiver_data_[endpoint_id].bandwidth_upgrade_time =
          base::TimeTicks::Now();

      // Different upgrade metrics should be logged in high visibility mode.
      if (receiver_data_[endpoint_id].is_high_visibility) {
        // The endpoint must have been decoded at this point.
        if (!receiver_data_[endpoint_id].endpoint_decoded_time.has_value()) {
          return;
        }

        // Compute and log the time delta between when the receiver decodes the
        // incoming endpoint and when the first bandwidth upgrade occurs. This
        // is a parallel to the sender upgrade metric and provides insight into
        // how long a bandwidth upgrade takes to complete from the receiver
        // side.
        base::TimeDelta upgrade_delta = ComputeDelta(
            receiver_data_[endpoint_id].endpoint_decoded_time.value(),
            receiver_data_[endpoint_id].bandwidth_upgrade_time.value());
        RecordNearbyShareHighVisibilityEndpointDecodedToBandwidthUpgradeDuration(
            medium, upgrade_delta);
        CD_LOG(INFO, Feature::NS)
            << "Endpoint [" << endpoint_id << "] decode to bandwidth upgrade ("
            << medium << "): " << upgrade_delta;
      } else {
        // The handshake must have occurred at this point.
        if (!receiver_data_[endpoint_id]
                 .paired_key_handshake_time.has_value()) {
          return;
        }

        // Compute and log the time delta between when the paired key handshake
        // completes and when the first bandwidth upgrade occurs. This provides
        // insight into how long a bandwidth upgrade takes to complete for a
        // non-high visibility mode receiver.
        base::TimeDelta handshake_delta = ComputeDelta(
            receiver_data_[endpoint_id].paired_key_handshake_time.value(),
            receiver_data_[endpoint_id].bandwidth_upgrade_time.value());
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
