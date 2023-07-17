// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

const char endpoint_id[] = "ABCD";

class NearbyShareTransferProfilerTest : public ::testing::Test {
 protected:
  NearbyShareTransferProfilerTest() = default;
  ~NearbyShareTransferProfilerTest() override = default;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;

  void AdvanceClock(uint millis) {
    task_environment_.AdvanceClock(base::Milliseconds(millis));
  }

  void ExpectTimeMetric(std::string histogram_suffix,
                        uint expected_millis,
                        uint expected_count = 1) {
    const std::string histogram_prefix = "Nearby.Share.TransferDuration.";
    histogram_tester_.ExpectUniqueTimeSample(
        histogram_prefix + histogram_suffix,
        base::Milliseconds(expected_millis), expected_count);
  }
};

// Note: Only the happy path is tested due to EXPECT_CHECK_DEATH being
// prohibitively expensive.

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  AdvanceClock(100);
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteManyRecievers) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  subject.OnEndpointDiscovered("EFGH");
  subject.OnEndpointDiscovered("IJKL");
  subject.OnEndpointDiscovered("MNOP");
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  subject.OnOutgoingEndpointDecoded("IJKL");
  subject.OnOutgoingEndpointDecoded("MNOP");
  AdvanceClock(100);
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  AdvanceClock(100);
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade", 100);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade.WifiLan",
                   100);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent.WifiLan", 300);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 500);
}

TEST_F(NearbyShareTransferProfilerTest,
       Sender_MultipleConnectionsToSameEndpoint) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  // First successful transfer.
  subject.OnEndpointDiscovered(endpoint_id);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  AdvanceClock(100);
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  // Second successful transfer without rediscovering endpoint.
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300, 1);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200, 2);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100, 2);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400, 2);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(endpoint_id);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(endpoint_id);
  AdvanceClock(100);
  subject.OnShareTargetSelected(endpoint_id);
  AdvanceClock(100);
  subject.OnConnectionEstablished(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(endpoint_id);
  AdvanceClock(100);
  subject.OnSendStart(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnSendComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade", 100);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade.WifiLan",
                   100);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent", 400);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent.WifiLan", 400);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 200);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 600);
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, false);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(endpoint_id);
  AdvanceClock(100);
  subject.OnTransferAccepted(endpoint_id);
  AdvanceClock(100);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_HighVisFlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, true);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(endpoint_id);
  AdvanceClock(100);
  subject.OnTransferAccepted(endpoint_id);
  AdvanceClock(100);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade",
                   100);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade.WifiLan", 100);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived", 400);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived.WifiLan", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, true);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(endpoint_id);
  AdvanceClock(100);
  subject.OnTransferAccepted(endpoint_id);
  AdvanceClock(100);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade",
                   200);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade.WifiLan", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived.WifiLan", 300);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_FlowCompleteWithNonHighVisUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, false);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(endpoint_id);
  AdvanceClock(100);
  subject.OnTransferAccepted(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 200);
  ExpectTimeMetric(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade", 300);
  ExpectTimeMetric(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade.WifiLan",
      300);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived.WifiLan", 100);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_FlowCompleteWithMutlipleUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(endpoint_id, true);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(endpoint_id);
  AdvanceClock(100);
  subject.OnTransferAccepted(endpoint_id);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(endpoint_id,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnReceiveComplete(endpoint_id, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade",
                   200);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade.WifiLan", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived", 400);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived.WifiLan", 400);
}
