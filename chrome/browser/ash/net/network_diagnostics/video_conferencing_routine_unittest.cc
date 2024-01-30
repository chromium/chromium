// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/video_conferencing_routine.h"

#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ash/net/network_diagnostics/network_diagnostics_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace network_diagnostics {

namespace {

namespace mojom = ::chromeos::network_diagnostics::mojom;

// Test implementation of UdpProber.
class TestUdpProber final : public UdpProber {
 public:
  TestUdpProber(UdpProber::UdpProbeCompleteCallback callback,
                int result,
                UdpProber::ProbeExitEnum probe_exit_enum) {
    // Post an asynchronous task simulating a completed probe. This mimics the
    // behavior of the production UdpProber constructor since the TestUdpProber
    // instance will be complete before FinishProbe is invoked. In the
    // production UdpProber, the constructor completes before DNS host
    // resolution is complete.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TestUdpProber::FinishProbe, weak_factory_.GetWeakPtr(),
                       std::move(callback), result, probe_exit_enum));
  }

  TestUdpProber(const TestUdpProber&) = delete;
  TestUdpProber& operator=(const TestUdpProber&) = delete;
  ~TestUdpProber() override = default;

 private:
  void FinishProbe(UdpProber::UdpProbeCompleteCallback callback,
                   int result,
                   UdpProber::ProbeExitEnum probe_exit_enum) {
    std::move(callback).Run(result, probe_exit_enum);
  }

  base::WeakPtrFactory<TestUdpProber> weak_factory_{this};
};

// Test implementation of TlsProber.
class TestTlsProber final : public TlsProber {
 public:
  TestTlsProber(TlsProber::TlsProbeCompleteCallback callback,
                int result,
                TlsProber::ProbeExitEnum probe_exit_enum) {
    // Post an asynchronous task simulating a completed probe. This mimics the
    // behavior of the production TlsProber constructor since the TestTlsProber
    // instance will be complete before FinishProbe is invoked. In the
    // production TlsProber, the constructor completes before DNS host
    // resolution is complete.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&TestTlsProber::FinishProbe, weak_factory_.GetWeakPtr(),
                       std::move(callback), result, probe_exit_enum));
  }

  TestTlsProber(const TestTlsProber&) = delete;
  TestTlsProber& operator=(const TestTlsProber&) = delete;
  ~TestTlsProber() override = default;

 private:
  void FinishProbe(TlsProber::TlsProbeCompleteCallback callback,
                   int result,
                   TlsProber::ProbeExitEnum probe_exit_enum) {
    std::move(callback).Run(result, probe_exit_enum);
  }

  base::WeakPtrFactory<TestTlsProber> weak_factory_{this};
};

}  // namespace

class VideoConferencingRoutineTest : public ::testing::Test {
 public:
  struct UdpProberReturnValue {
    net::Error result;
    UdpProber::ProbeExitEnum probe_exit_enum;
  };
  struct TlsProberReturnValue {
    net::Error result;
    TlsProber::ProbeExitEnum probe_exit_enum;
  };

  VideoConferencingRoutineTest() = default;
  VideoConferencingRoutineTest(const VideoConferencingRoutineTest&) = delete;
  VideoConferencingRoutineTest& operator=(const VideoConferencingRoutineTest&) =
      delete;
  ~VideoConferencingRoutineTest() override = default;

  void CompareResult(
      mojom::RoutineVerdict expected_verdict,
      const std::vector<mojom::VideoConferencingProblem>& expected_problems,
      mojom::RoutineResultPtr result) {
    EXPECT_EQ(expected_verdict, result->verdict);
    EXPECT_EQ(expected_problems,
              result->problems->get_video_conferencing_problems());
    run_loop_.Quit();
  }

  std::unique_ptr<UdpProber> CreateAndExecuteUdpProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      base::span<const uint8_t> data,
      net::NetworkTrafficAnnotationTag tag,
      base::TimeDelta timeout_after_host_resolution,
      UdpProber::UdpProbeCompleteCallback callback) {
    DCHECK(fake_udp_probe_results_.size() > 0);

    auto value = fake_udp_probe_results_.front();
    fake_udp_probe_results_.pop_front();
    auto test_udp_prober = std::make_unique<TestUdpProber>(
        std::move(callback), value.result, value.probe_exit_enum);
    return std::move(test_udp_prober);
  }

  std::unique_ptr<TlsProber> CreateAndExecuteTlsProber(
      network::NetworkContextGetter network_context_getter,
      net::HostPortPair host_port_pair,
      bool negotiate_tls,
      TlsProber::TlsProbeCompleteCallback callback) {
    DCHECK(fake_tls_probe_results_.size() > 0);

    auto value = fake_tls_probe_results_.front();
    fake_tls_probe_results_.pop_front();
    auto test_tls_prober = std::make_unique<TestTlsProber>(
        std::move(callback), value.result, value.probe_exit_enum);
    return std::move(test_tls_prober);
  }

 protected:
  void RunRoutine(
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::VideoConferencingProblem>& expected_problems) {
    video_conferencing_routine_->RunRoutine(
        base::BindOnce(&VideoConferencingRoutineTest::CompareResult, weak_ptr(),
                       expected_routine_verdict, expected_problems));
    run_loop_.Run();
  }

  void SetUpRoutine(std::deque<UdpProberReturnValue> fake_udp_probe_results,
                    std::deque<TlsProberReturnValue> fake_tls_probe_results) {
    fake_udp_probe_results_ = std::move(fake_udp_probe_results);
    fake_tls_probe_results_ = std::move(fake_tls_probe_results);
    video_conferencing_routine_ = std::make_unique<VideoConferencingRoutine>(
        mojom::RoutineCallSource::kDiagnosticsUI);
    video_conferencing_routine_->set_udp_prober_getter_callback_for_testing(
        base::BindRepeating(
            &VideoConferencingRoutineTest::CreateAndExecuteUdpProber,
            base::Unretained(this)));
    video_conferencing_routine_->set_tls_prober_getter_callback_for_testing(
        base::BindRepeating(
            &VideoConferencingRoutineTest::CreateAndExecuteTlsProber,
            base::Unretained(this)));
  }

  // Sets up required properties (via fakes) and runs the test.
  //
  // Parameters:
  // |fake_udp_probe_results|: Represents the results of UDP probes.
  // |fake_tls_probe_results|: Represents the results of TCP and TLS probes.
  // |expected_routine_verdict|: Represents the expected verdict
  // reported by this test.
  void SetUpAndRunRoutine(
      std::deque<UdpProberReturnValue> fake_udp_probe_results,
      std::deque<TlsProberReturnValue> fake_tls_probe_results,
      mojom::RoutineVerdict expected_routine_verdict,
      const std::vector<mojom::VideoConferencingProblem>& expected_problems) {
    SetUpRoutine(std::move(fake_udp_probe_results),
                 std::move(fake_tls_probe_results));
    RunRoutine(expected_routine_verdict, expected_problems);
  }

  base::WeakPtr<VideoConferencingRoutineTest> weak_ptr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  std::deque<UdpProberReturnValue> fake_udp_probe_results_;
  std::deque<TlsProberReturnValue> fake_tls_probe_results_;
  std::unique_ptr<VideoConferencingRoutine> video_conferencing_routine_;
  base::WeakPtrFactory<VideoConferencingRoutineTest> weak_factory_{this};
};

// Tests the scenario where:
// (1) An open port to the STUN server is found via UDP and TCP.
// (2) All media hostnames are reachable.
TEST_F(VideoConferencingRoutineTest, TestSuccessfulPath) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    if (i < num_udp_ports_tested - 1) {
      fake_udp_probe_results.push_back(UdpProberReturnValue{
          net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
      continue;
    }
    fake_udp_probe_results.push_back(
        UdpProberReturnValue{net::OK, UdpProber::ProbeExitEnum::kSuccess});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    if (i < num_tcp_ports_tested - 1) {
      fake_tls_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
      continue;
    }
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  // Results corresponding to the TLS requests to media hostnames.
  int num_media_hostnames_tested = util::GetDefaultMediaUrls().size();
  for (int i = 0; i < num_media_hostnames_tested; i++) {
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kNoProblem,
                     /*expected_problems=*/{});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via UDP.
TEST_F(VideoConferencingRoutineTest, TestUdpFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    fake_udp_probe_results.push_back(UdpProberReturnValue{
        net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    if (i < num_tcp_ports_tested - 1) {
      fake_tls_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
      continue;
    }
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  // Results corresponding to the TLS requests to media hostnames.
  int num_media_hostnames_tested = util::GetDefaultMediaUrls().size();
  for (int i = 0; i < num_media_hostnames_tested; i++) {
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kUdpFailure});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via TCP.
TEST_F(VideoConferencingRoutineTest, TestTcpFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    if (i < num_udp_ports_tested - 1) {
      fake_udp_probe_results.push_back(UdpProberReturnValue{
          net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
      continue;
    }
    fake_udp_probe_results.push_back(
        UdpProberReturnValue{net::OK, UdpProber::ProbeExitEnum::kSuccess});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    fake_tls_probe_results.push_back(TlsProberReturnValue{
        net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
  }

  // Results corresponding to the TLS requests to media hostnames.
  int num_media_hostnames_tested = util::GetDefaultMediaUrls().size();
  for (int i = 0; i < num_media_hostnames_tested; i++) {
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kTcpFailure});
}

// Tests the scenario where:
// (1) Requests to one or more media hostnames failed.
TEST_F(VideoConferencingRoutineTest, TestMediaFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    if (i < num_udp_ports_tested - 1) {
      fake_udp_probe_results.push_back(UdpProberReturnValue{
          net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
      continue;
    }
    fake_udp_probe_results.push_back(
        UdpProberReturnValue{net::OK, UdpProber::ProbeExitEnum::kSuccess});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    if (i < num_tcp_ports_tested - 1) {
      fake_tls_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
      continue;
    }
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  // Results corresponding to the TLS requests to media hostnames.
  fake_tls_probe_results.push_back(TlsProberReturnValue{
      net::ERR_FAILED, TlsProber::ProbeExitEnum::kTlsUpgradeFailure});

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kMediaFailure});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via UDP.
// (2) No open port to the STUN server is found via TCP.
TEST_F(VideoConferencingRoutineTest, TestUdpAndTcpFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    fake_udp_probe_results.push_back(UdpProberReturnValue{
        net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    fake_tls_probe_results.push_back(TlsProberReturnValue{
        net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
  }

  // Results corresponding to the TLS requests to media hostnames.
  int num_media_hostnames_tested = util::GetDefaultMediaUrls().size();
  for (int i = 0; i < num_media_hostnames_tested; i++) {
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kUdpFailure,
                      mojom::VideoConferencingProblem::kTcpFailure});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via UDP.
// (2) Requests to one or more media hostnames failed.
TEST_F(VideoConferencingRoutineTest, TestUdpAndMediaFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    fake_udp_probe_results.push_back(UdpProberReturnValue{
        net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    if (i < num_tcp_ports_tested - 1) {
      fake_tls_probe_results.push_back(TlsProberReturnValue{
          net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
      continue;
    }
    fake_tls_probe_results.push_back(
        TlsProberReturnValue{net::OK, TlsProber::ProbeExitEnum::kSuccess});
  }

  // Results corresponding to the TLS requests to media hostnames.
  fake_tls_probe_results.push_back(TlsProberReturnValue{
      net::ERR_FAILED, TlsProber::ProbeExitEnum::kTlsUpgradeFailure});

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kUdpFailure,
                      mojom::VideoConferencingProblem::kMediaFailure});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via TCP.
// (2) Requests to one or more media hostnames failed.
TEST_F(VideoConferencingRoutineTest, TestTcpAndMediaFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    if (i < num_udp_ports_tested - 1) {
      fake_udp_probe_results.push_back(UdpProberReturnValue{
          net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
      continue;
    }
    fake_udp_probe_results.push_back(
        UdpProberReturnValue{net::OK, UdpProber::ProbeExitEnum::kSuccess});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    fake_tls_probe_results.push_back(TlsProberReturnValue{
        net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
  }

  // Results corresponding to the TLS requests to media hostnames.
  fake_tls_probe_results.push_back(TlsProberReturnValue{
      net::ERR_FAILED, TlsProber::ProbeExitEnum::kTlsUpgradeFailure});

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kTcpFailure,
                      mojom::VideoConferencingProblem::kMediaFailure});
}

// Tests the scenario where:
// (1) No open port to the STUN server is found via UDP.
// (2) No open port to the STUN server is found via TCP.
// (3) Requests to one or more media hostnames failed.
TEST_F(VideoConferencingRoutineTest, TestTcpAndUdpAndMediaFailure) {
  // Results corresponding to UDP requests to the STUN server.
  int num_udp_ports_tested = util::GetUdpPortsForGoogleStunServer().size();
  std::deque<UdpProberReturnValue> fake_udp_probe_results;
  for (int i = 0; i < num_udp_ports_tested; i++) {
    fake_udp_probe_results.push_back(UdpProberReturnValue{
        net::ERR_FAILED, UdpProber::ProbeExitEnum::kNoDataReceivedFailure});
  }

  // Tracks the results corresponding to TCP requests to the STUN server and
  // TLS requests to media hostnames.
  std::deque<TlsProberReturnValue> fake_tls_probe_results;

  // Results corresponding to the STUN server via TCP.
  int num_tcp_ports_tested = util::GetTcpPortsForGoogleStunServer().size();
  for (int i = 0; i < num_tcp_ports_tested; i++) {
    fake_tls_probe_results.push_back(TlsProberReturnValue{
        net::ERR_FAILED, TlsProber::ProbeExitEnum::kTcpConnectionFailure});
  }

  // Results corresponding to the TLS requests to media hostnames.
  fake_tls_probe_results.push_back(TlsProberReturnValue{
      net::ERR_FAILED, TlsProber::ProbeExitEnum::kTlsUpgradeFailure});

  SetUpAndRunRoutine(std::move(fake_udp_probe_results),
                     std::move(fake_tls_probe_results),
                     mojom::RoutineVerdict::kProblem,
                     {mojom::VideoConferencingProblem::kUdpFailure,
                      mojom::VideoConferencingProblem::kTcpFailure,
                      mojom::VideoConferencingProblem::kMediaFailure});
}

}  // namespace network_diagnostics
}  // namespace ash
