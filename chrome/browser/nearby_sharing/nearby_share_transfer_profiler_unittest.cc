// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/nearby_share_transfer_profiler.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kEndpointId[] = "ABCD";
constexpr char kHistogramPrefix[] = "Nearby.Share.TransferDuration.";

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
    histogram_tester_.ExpectUniqueTimeSample(
        kHistogramPrefix + histogram_suffix,
        base::Milliseconds(expected_millis), expected_count);
  }

  void ExpectTotalCount(std::string histogram_suffix, uint expected_count) {
    histogram_tester_.ExpectTotalCount(kHistogramPrefix + histogram_suffix,
                                       expected_count);
  }
};

// Note: Only the happy path is tested due to EXPECT_CHECK_DEATH being
// prohibitively expensive.

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(kEndpointId);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(kEndpointId);
  AdvanceClock(100);
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteManyRecievers) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(kEndpointId);
  subject.OnEndpointDiscovered("EFGH");
  subject.OnEndpointDiscovered("IJKL");
  subject.OnEndpointDiscovered("MNOP");
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(kEndpointId);
  subject.OnOutgoingEndpointDecoded("IJKL");
  subject.OnOutgoingEndpointDecoded("MNOP");
  AdvanceClock(100);
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_FlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(kEndpointId);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(kEndpointId);
  AdvanceClock(100);
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade2", 100);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade2.WifiLan",
                   100);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent2", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent2.WifiLan", 300);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 500);
}

TEST_F(NearbyShareTransferProfilerTest,
       Sender_MultipleConnectionsToSameEndpoint) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  // First successful transfer.
  subject.OnEndpointDiscovered(kEndpointId);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(kEndpointId);
  AdvanceClock(100);
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  // Second successful transfer without rediscovering endpoint.
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300, 1);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 200, 2);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 100, 2);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 400, 2);
}

TEST_F(NearbyShareTransferProfilerTest, Sender_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnEndpointDiscovered(kEndpointId);
  AdvanceClock(100);
  subject.OnOutgoingEndpointDecoded(kEndpointId);
  AdvanceClock(100);
  subject.OnShareTargetSelected(kEndpointId);
  AdvanceClock(100);
  subject.OnConnectionEstablished(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameSent(kEndpointId);
  AdvanceClock(100);
  subject.OnSendStart(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnSendComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Sender.DiscoveredToConnectionEstablished", 300);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade2", 100);
  ExpectTimeMetric("Sender.ConnectionEstablishedToBandwidthUpgrade2.WifiLan",
                   100);
  ExpectTimeMetric("Sender.InitiatedToSentIntroductionFrame", 300);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent2", 400);
  ExpectTimeMetric("Sender.BandwidthUpgradeToAllFilesSent2.WifiLan", 400);
  ExpectTimeMetric("Sender.StartSendFilesToAllFilesSent", 200);
  ExpectTimeMetric("Sender.InitiatedToAllFilesSent", 600);
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_FlowCompleteWithoutUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, false);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_HighVisFlowCompleteWithUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, true);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2",
                   100);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2.WifiLan", 100);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2", 400);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2.WifiLan", 400);
}

TEST_F(NearbyShareTransferProfilerTest, Receiver_MultipleBandwidthUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, true);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2",
                   200);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2.WifiLan", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2.WifiLan", 300);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_FlowCompleteWithNonHighVisUpgrade) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, false);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 200);
  ExpectTimeMetric(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade2", 300);
  ExpectTimeMetric(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade2.WifiLan",
      300);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2", 100);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2.WifiLan", 100);
}

// Regression test for b/301132314
TEST_F(NearbyShareTransferProfilerTest,
       Receiver_FlowBandwidthUpgradeAfterComplete) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, false);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 100);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 200);

  // Bandwidth upgrade metrics should not be emitted if the bandwidth upgrade
  // completes after the transfer has ended.
  ExpectTotalCount(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade2", 0);
  ExpectTotalCount(
      "Receiver.NonHighVisibilityPairedKeyCompleteToBandwidthUpgrade2.WifiLan",
      0);
  ExpectTotalCount("Receiver.BandwidthUpgradeToAllFilesReceived2", 0);
  ExpectTotalCount("Receiver.BandwidthUpgradeToAllFilesReceived2.WifiLan", 0);
}

TEST_F(NearbyShareTransferProfilerTest,
       Receiver_FlowCompleteWithMutlipleUpgrades) {
  NearbyShareTransferProfiler subject = NearbyShareTransferProfiler();

  subject.OnIncomingEndpointDecoded(kEndpointId, true);
  AdvanceClock(100);
  subject.OnPairedKeyHandshakeComplete(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnIntroductionFrameReceived(kEndpointId);
  AdvanceClock(100);
  subject.OnTransferAccepted(kEndpointId);
  AdvanceClock(100);
  subject.OnBandwidthUpgrade(kEndpointId,
                             nearby::connections::mojom::Medium::kWifiLan);
  AdvanceClock(100);
  subject.OnReceiveComplete(kEndpointId, TransferMetadata::Status::kComplete);

  ExpectTimeMetric("Receiver.EndpointDecodedToReceivedIntroductionFrame", 300);
  ExpectTimeMetric("Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2",
                   200);
  ExpectTimeMetric(
      "Receiver.HighVisibilityEndpointDecodedToBandwidthUpgrade2.WifiLan", 200);
  ExpectTimeMetric("Receiver.AcceptedTransferToAllFilesReceived", 200);
  ExpectTimeMetric("Receiver.ReceivedIntroductionFrameToAllFilesReceived", 300);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2", 400);
  ExpectTimeMetric("Receiver.BandwidthUpgradeToAllFilesReceived2.WifiLan", 400);
}
