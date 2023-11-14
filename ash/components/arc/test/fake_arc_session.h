// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
#define ASH_COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_

#include <memory>
#include <string>

#include "ash/components/arc/session/arc_session.h"
#include "ash/components/arc/session/arc_stop_reason.h"

namespace cryptohome {
class Identification;
}  // namespace cryptohome

namespace arc {

// A fake ArcSession that creates a local connection.
class FakeArcSession : public ArcSession {
 public:
  FakeArcSession();

  FakeArcSession(const FakeArcSession&) = delete;
  FakeArcSession& operator=(const FakeArcSession&) = delete;

  ~FakeArcSession() override;

  // ArcSession overrides:
  void StartMiniInstance() override;
  void RequestUpgrade(UpgradeParams params) override;
  void Stop() override;
  bool IsStopRequested() override;
  void OnShutdown() override;
  void SetUserInfo(const cryptohome::Identification& cryptohome_id,
                   const std::string& hash,
                   const std::string& serial_number) override;
  void SetDemoModeDelegate(
      ArcClientAdapter::DemoModeDelegate* delegate) override;
  void TrimVmMemory(TrimVmMemoryCallback callback, int page_limit) override;
  void SetDefaultDeviceScaleFactor(float scale_factor) override;
  void SetUseVirtioBlkData(bool use_virtio_blk_data) override;
  void SetArcSignedIn(bool arc_signed_in) override;

  // To emulate unexpected stop, such as crash.
  void StopWithReason(ArcStopReason reason);

  // The following control Start() behavior for testing various situations.

  // Enables/disables boot failure emulation, in which OnSessionStopped(reason)
  // will be called when Start() or StartForLoginScreen() is called.
  void EnableBootFailureEmulation(ArcStopReason reason);

  // Emulate Start() is suspended at some phase.
  void SuspendBoot();

  // To emulate the mini-container starting. This can cause a failure if
  // EnableBootFailureEmulation was called on this instance
  void EmulateMiniContainerStart();

  // Returns true if the session is considered as running.
  bool is_running() const { return running_; }

  // Returns an upgrade parameter passed to the session.
  std::string upgrade_locale_param() const { return upgrade_locale_param_; }

  int trim_vm_memory_count() const { return trim_vm_memory_count_; }
  int last_trim_vm_page_limit() const { return last_trim_vm_page_limit_; }

  // Set values passed to the callback of TrimVmMemory, for testing.
  void set_trim_result(bool success, const std::string& reason) {
    trim_success_ = success;
    trim_fail_reason = reason;
  }

  // Returns FakeArcSession instance. This can be used for a factory
  // in ArcBridgeServiceImpl.
  static std::unique_ptr<ArcSession> Create();

 private:
  bool boot_failure_emulation_enabled_ = false;
  ArcStopReason boot_failure_reason_;

  bool boot_suspended_ = false;
  bool upgrade_requested_ = false;
  bool running_ = false;
  bool stop_requested_ = false;
  std::string upgrade_locale_param_;
  int trim_vm_memory_count_ = 0;
  int last_trim_vm_page_limit_ = arc::ArcSession::kNoPageLimit;
  bool trim_success_ = true;
  std::string trim_fail_reason;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_FAKE_ARC_SESSION_H_
