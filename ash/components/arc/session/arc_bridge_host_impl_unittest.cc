// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/session/arc_bridge_host_impl.h"

#include <memory>
#include <utility>

#include "ash/components/arc/mojom/arc_bridge.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {
namespace {

// A test fixture class that sets up |arc_bridge_host_impl_| and |remote_|. The
// latter allows the test to invoke mojo methods in mojom::ArcBridgeHost.
class ArcBridgeHostImplTest : public testing::Test {
 public:
  ArcBridgeHostImplTest() = default;
  ArcBridgeHostImplTest(const ArcBridgeHostImplTest&) = delete;
  ArcBridgeHostImplTest& operator=(const ArcBridgeHostImplTest&) = delete;
  ~ArcBridgeHostImplTest() override = default;

  void SetUp() override {
    mojo::PendingReceiver<mojom::ArcBridgeHost> pending_receiver;
    mojo::PendingRemote<mojom::ArcBridgeHost> pending_remote =
        pending_receiver.InitWithNewPipeAndPassRemote();
    arc_bridge_host_impl_ =
        std::make_unique<ArcBridgeHostImpl>(&bridge_service_);
    arc_bridge_host_impl_->AddReceiver(std::move(pending_receiver));
    remote_.Bind(std::move(pending_remote));
  }

  const ArcBridgeHostImpl* arc_bridge_host_impl() const {
    return arc_bridge_host_impl_.get();
  }
  mojo::Remote<mojom::ArcBridgeHost>& remote() { return remote_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ArcBridgeService bridge_service_;
  std::unique_ptr<ArcBridgeHostImpl> arc_bridge_host_impl_;
  mojo::Remote<mojom::ArcBridgeHost> remote_;
};

// A small helper class that holds mojo::PendingReceiver<T> and calls reset()
// for the receiver in the dtor.
template <typename T>
class ScopedPendingReceiver {
 public:
  explicit ScopedPendingReceiver(const ArcBridgeHostImpl* arc_bridge_host_impl)
      : arc_bridge_host_impl_(arc_bridge_host_impl) {}

  ScopedPendingReceiver(const ScopedPendingReceiver&) = delete;
  ScopedPendingReceiver& operator=(const ScopedPendingReceiver&) = delete;

  ~ScopedPendingReceiver() {
    // Reset the receiver and verifiy that the number of mojo channels
    // |arc_bridge_host_impl_| manages decreases by one.
    const size_t count_before =
        arc_bridge_host_impl_->GetNumMojoChannelsForTesting();
    pending_receiver_.reset();
    base::RunLoop().RunUntilIdle();
    const size_t count_after =
        arc_bridge_host_impl_->GetNumMojoChannelsForTesting();
    EXPECT_EQ(count_before - 1, count_after);
  }

  mojo::PendingReceiver<T>& get() { return pending_receiver_; }

 private:
  mojo::PendingReceiver<T> pending_receiver_;
  const raw_ptr<const ArcBridgeHostImpl> arc_bridge_host_impl_;
};

// Test that the test fixture class, especially its ArcBridgeHostImpl variable,
// can be constructed and destructed without troubles.
TEST_F(ArcBridgeHostImplTest, TestConstructDestruct) {}

// Test that OnXyzInstanceReady methods work as intended.
TEST_F(ArcBridgeHostImplTest, TestOnInstanceReady) {
  const ArcBridgeHostImpl* impl = arc_bridge_host_impl();
  ASSERT_EQ(0u, impl->GetNumMojoChannelsForTesting());
  {
    auto* proxy = remote().get();

    // A macro that calls On<name>InstanceReady() on the |impl| and verifies
    // that the number of mojo channels |impl| manages increases by one. The
    // |pending_receiver_<name>| instance intentionally exists outside of the
    // block so that it will be alive until all other instances are created.
#define MAKE_INSTANCE_READY(name)                                             \
  ScopedPendingReceiver<mojom::name##Instance> pending_receiver_##name(impl); \
  {                                                                           \
    SCOPED_TRACE("mojom::" #name "Instance");                                 \
    mojo::PendingRemote<mojom::name##Instance> remote =                       \
        pending_receiver_##name.get().InitWithNewPipeAndPassRemote();         \
    const size_t count_before = impl->GetNumMojoChannelsForTesting();         \
    proxy->On##name##InstanceReady(std::move(remote));                        \
    base::RunLoop().RunUntilIdle();                                           \
    const size_t count_after = impl->GetNumMojoChannelsForTesting();          \
    EXPECT_EQ(count_before + 1, count_after);                                 \
  }

#define MAKE_INSTANCE_READY_WITH_NAMESPACE(name_space, name)                 \
  ScopedPendingReceiver<name_space::name##Instance> pending_receiver_##name( \
      impl);                                                                 \
  {                                                                          \
    SCOPED_TRACE(#name_space "::" #name "Instance");                         \
    mojo::PendingRemote<name_space::name##Instance> remote =                 \
        pending_receiver_##name.get().InitWithNewPipeAndPassRemote();        \
    const size_t count_before = impl->GetNumMojoChannelsForTesting();        \
    proxy->On##name##InstanceReady(std::move(remote));                       \
    base::RunLoop().RunUntilIdle();                                          \
    const size_t count_after = impl->GetNumMojoChannelsForTesting();         \
    EXPECT_EQ(count_before + 1, count_after);                                \
  }

#define MAKE_INSTANCE_READY_PREFIX_NAMESPACE(name_space, name)        \
  ScopedPendingReceiver<name_space::mojom::name##Instance>            \
      pending_receiver_##name(impl);                                  \
  {                                                                   \
    SCOPED_TRACE("mojom::" #name "Instance");                         \
    mojo::PendingRemote<name_space::mojom::name##Instance> remote =   \
        pending_receiver_##name.get().InitWithNewPipeAndPassRemote(); \
    const size_t count_before = impl->GetNumMojoChannelsForTesting(); \
    proxy->On##name##InstanceReady(std::move(remote));                \
    base::RunLoop().RunUntilIdle();                                   \
    const size_t count_after = impl->GetNumMojoChannelsForTesting();  \
    EXPECT_EQ(count_before + 1, count_after);                         \
  }

    MAKE_INSTANCE_READY_PREFIX_NAMESPACE(ax::android, AccessibilityHelper);
    MAKE_INSTANCE_READY(AdbdMonitor);
    MAKE_INSTANCE_READY(App);
    MAKE_INSTANCE_READY(AppPermissions);
    MAKE_INSTANCE_READY(Appfuse);
    MAKE_INSTANCE_READY(Audio);
    MAKE_INSTANCE_READY(Auth);
    MAKE_INSTANCE_READY(BackupSettings);
    MAKE_INSTANCE_READY(Bluetooth);
    MAKE_INSTANCE_READY(BootPhaseMonitor);
    MAKE_INSTANCE_READY(Camera);
    MAKE_INSTANCE_READY(CompatibilityMode);
    MAKE_INSTANCE_READY(CrashCollector);
    MAKE_INSTANCE_READY(DigitalGoods);
    MAKE_INSTANCE_READY(DiskSpace);
    MAKE_INSTANCE_READY(EnterpriseReporting);
    MAKE_INSTANCE_READY(FileSystem);
    MAKE_INSTANCE_READY(Ime);
    MAKE_INSTANCE_READY(IioSensor);
    MAKE_INSTANCE_READY(InputMethodManager);
    MAKE_INSTANCE_READY(IntentHelper);
    MAKE_INSTANCE_READY(Keymaster);
    MAKE_INSTANCE_READY_WITH_NAMESPACE(mojom::keymint, KeyMint);
    MAKE_INSTANCE_READY(MediaSession);
    MAKE_INSTANCE_READY(Metrics);
    MAKE_INSTANCE_READY(Midis);
    MAKE_INSTANCE_READY(NearbyShare);
    MAKE_INSTANCE_READY(Net);
    // TODO(khmel): Test mojom::NotificationsInstance. Unlike others, the
    // notification instance is not managed by ArcBridgeHostImpl. Since the
    // instance is forwarded to ash, we need a completely different test.
    MAKE_INSTANCE_READY(ObbMounter);
    MAKE_INSTANCE_READY(OemCrypto);
    MAKE_INSTANCE_READY(OnDeviceSafety);
    MAKE_INSTANCE_READY_WITH_NAMESPACE(chromeos::payments::mojom, PaymentApp);
    MAKE_INSTANCE_READY(Pip);
    MAKE_INSTANCE_READY(Policy);
    MAKE_INSTANCE_READY(Power);
    MAKE_INSTANCE_READY(PrintSpooler);
    MAKE_INSTANCE_READY(Process);
    MAKE_INSTANCE_READY(ScreenCapture);
    MAKE_INSTANCE_READY(Sharesheet);
    MAKE_INSTANCE_READY(Timer);
    MAKE_INSTANCE_READY(Tracing);
    MAKE_INSTANCE_READY(Tts);
    MAKE_INSTANCE_READY(UsbHost);
    MAKE_INSTANCE_READY(Video);
    MAKE_INSTANCE_READY(VolumeMounter);
    MAKE_INSTANCE_READY(WakeLock);
    MAKE_INSTANCE_READY(Wallpaper);

#undef MAKE_INSTANCE_READY
#undef MAKE_INSTANCE_READY_PREFIX_NAMESPACE
    EXPECT_LT(0u, impl->GetNumMojoChannelsForTesting());
  }
  // After all ScopedPendingReceiver<T> objects are destructed, the number of
  // mojo channels |impl| manages goes down to zero.
  EXPECT_EQ(0u, impl->GetNumMojoChannelsForTesting());
}

}  // namespace
}  // namespace arc
