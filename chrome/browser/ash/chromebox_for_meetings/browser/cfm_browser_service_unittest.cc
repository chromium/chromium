// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/browser/cfm_browser_service.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/dbus/chromebox_for_meetings/fake_cfm_hotline_client.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/fake_service_context.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_browser.mojom.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "components/variations/field_trial_config/field_trial_util.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::cfm {
namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

class CfmBrowserServiceTest : public testing::Test {
 public:
  CfmBrowserServiceTest() {
    scoped_feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }
  CfmBrowserServiceTest(const CfmBrowserServiceTest&) = delete;
  CfmBrowserServiceTest& operator=(const CfmBrowserServiceTest&) = delete;

  void SetUp() override {
    CfmHotlineClient::InitializeFake();
    chromeos::cfm::ServiceConnection::UseFakeServiceConnectionForTesting(
        &fake_service_connection_);
    CfmBrowserService::Initialize();
  }

  void TearDown() override {
    CfmBrowserService::Shutdown();
    CfmHotlineClient::Shutdown();
  }

  FakeCfmHotlineClient* GetClient() {
    return static_cast<FakeCfmHotlineClient*>(CfmHotlineClient::Get());
  }

  // Returns a mojo::Remote for the mojom::CfmBrowser by faking the
  // way the cfm mojom binder daemon would request it through chrome.
  const mojo::Remote<mojom::CfmBrowser>& GetBrowserRemote() {
    if (browser_remote_.is_bound()) {
      return browser_remote_;
    }

    // If there is no valid remote create one.
    auto* interface_name = mojom::CfmBrowser::Name_;

    base::RunLoop run_loop;

    // Fake out CfmServiceContext
    fake_service_connection_.SetCallback(base::BindLambdaForTesting(
        [&](mojo::PendingReceiver<mojom::CfmServiceContext> pending_receiver,
            bool success) {
          ASSERT_TRUE(success);
          context_receiver_set_.Add(&context_, std::move(pending_receiver));
        }));

    context_.SetFakeProvideAdaptorCallback(base::BindLambdaForTesting(
        [&](const std::string& service_id,
            mojo::PendingRemote<mojom::CfmServiceAdaptor>
                pending_adaptor_remote,
            mojom::CfmServiceContext::ProvideAdaptorCallback callback) {
          ASSERT_EQ(interface_name, service_id);
          adaptor_remote_.Bind(std::move(pending_adaptor_remote));
          std::move(callback).Run(true);
        }));

    EXPECT_TRUE(GetClient()->FakeEmitSignal(interface_name));
    run_loop.RunUntilIdle();

    EXPECT_TRUE(adaptor_remote_.is_connected());

    adaptor_remote_->OnBindService(
        browser_remote_.BindNewPipeAndPassReceiver().PassPipe());
    EXPECT_TRUE(browser_remote_.is_connected());

    return browser_remote_;
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::cfm::FakeCfmServiceContext context_;
  mojo::Remote<mojom::CfmBrowser> browser_remote_;
  mojo::ReceiverSet<mojom::CfmServiceContext> context_receiver_set_;
  mojo::Remote<mojom::CfmServiceAdaptor> adaptor_remote_;
  chromeos::cfm::FakeServiceConnectionImpl fake_service_connection_;
};

// This test ensures that the CfmBrowserService is discoverable by its mojom
// name by sending a signal received by CfmHotlineClient.
TEST_F(CfmBrowserServiceTest, BrowserServiceAvailable) {
  ASSERT_TRUE(GetClient()->FakeEmitSignal(mojom::CfmBrowser::Name_));
}

// This test ensures that the CfmBrowserService correctly registers itself for
// discovery by the cfm mojom binder daemon and correctly returns a working
// mojom remote.
TEST_F(CfmBrowserServiceTest, GetBrowserRemote) {
  ASSERT_TRUE(GetBrowserRemote().is_connected());
}

TEST_F(CfmBrowserServiceTest, GetVariationsData) {
  std::string field_trial_parameters = "Foo.Bar:Key/Value";
  std::string field_trial_states = "*Baz/Qux/Foo/Bar";
  std::string enabled_features = "enabled<Foo";
  std::string disabled_features = "disabled<Baz";

  ASSERT_TRUE(variations::AssociateParamsFromString(field_trial_parameters));
  ASSERT_TRUE(base::FieldTrialList::CreateTrialsFromString(field_trial_states));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitFromCommandLine(enabled_features, disabled_features);

  base::RunLoop run_loop;
  GetBrowserRemote()->GetVariationsData(base::BindLambdaForTesting(
      [&](const std::string& parameters, const std::string& states,
          const std::string& enabled, const std::string& disabled) {
        EXPECT_EQ(field_trial_parameters, parameters);
        EXPECT_EQ(field_trial_states, states);
        EXPECT_EQ(enabled_features, enabled);
        EXPECT_EQ(disabled_features, disabled);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace ash::cfm
