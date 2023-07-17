// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"

#include "testing/gtest/include/gtest/gtest.h"

const char endpoint_id[] = "ABCD";

class NearbyShareTransferProfilerTest : public ::testing::Test {
 protected:
  NearbyShareTransferProfilerTest() = default;
  ~NearbyShareTransferProfilerTest() override = default;
};

// Note: Only the happy path is tested due to EXPECT_CHECK_DEATH being
// prohibitively expensive.

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteManyRecievers) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnEndpointDiscovered("EFGH");
  subject.OnEndpointDiscovered("IJKL");
  subject.OnEndpointDiscovered("MNOP");
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnOutgoingEndpointDecoded("IJKL");
  subject.OnOutgoingEndpointDecoded("MNOP");
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest,
       Sender_MultipleConnectionsToSameEndpoint) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  // First successful transfer.
  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // Second successful transfer without rediscovering endpoint.
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Sender_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnShareTargetSelected(endpoint_id);
  subject.OnConnectionEstablished(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnIntroductionFrameSent(endpoint_id);
  subject.OnSendStart(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, false);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  subject.OnIntroductionFrameReceived(endpoint_id);
  subject.OnTransferAccepted(endpoint_id);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_FlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, false);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnIntroductionFrameReceived(endpoint_id);
  subject.OnTransferAccepted(endpoint_id);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, false);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnIntroductionFrameReceived(endpoint_id);
  subject.OnTransferAccepted(endpoint_id);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // TODO(b/279464086): Expect associated UMA histograms once logged.
}
