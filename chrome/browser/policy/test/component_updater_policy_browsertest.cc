// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/component_updater/chrome_component_updater_configurator.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/component_updater_switches.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/update_client/net/url_loader_post_interceptor.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace policy {

// Tests the ComponentUpdater's EnabledComponentUpdates group policy by
// calling the OnDemand interface. It uses the network interceptor to inspect
// the presence of the updatedisabled="true" attribute in the update check
// request. The update check request is expected to fail, since CUP fails.
class ComponentUpdaterPolicyTest : public PolicyTest {
 public:
  ComponentUpdaterPolicyTest();
  ComponentUpdaterPolicyTest(const ComponentUpdaterPolicyTest&) = delete;
  ComponentUpdaterPolicyTest& operator=(const ComponentUpdaterPolicyTest&) =
      delete;
  ~ComponentUpdaterPolicyTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  using TestCaseAction = void (ComponentUpdaterPolicyTest::*)();
  using TestCase = std::pair<TestCaseAction, TestCaseAction>;

  // These test scenarios run as part of one test case by using the
  // CallAsync helper, which calls OnDemand, then chains up to the next
  // scenario when the OnDemandComplete callback fires.
  void DefaultPolicy_GroupPolicySupported();
  void FinishDefaultPolicy_GroupPolicySupported();

  void DefaultPolicy_GroupPolicyNotSupported();
  void FinishDefaultPolicy_GroupPolicyNotSupported();

  void EnabledPolicy_GroupPolicySupported();
  void FinishEnabledPolicy_GroupPolicySupported();

  void EnabledPolicy_GroupPolicyNotSupported();
  void FinishEnabledPolicy_GroupPolicyNotSupported();

  void DisabledPolicy_GroupPolicySupported();
  void FinishDisabled_PolicyGroupPolicySupported();

  void DisabledPolicy_GroupPolicyNotSupported();
  void FinishDisabledPolicy_GroupPolicyNotSupported();

  void BeginTest();
  void EndTest();

  void UpdateComponent(
      const component_updater::ComponentRegistration& crx_component);
  void CallAsync(TestCaseAction action);
  void VerifyExpectations(bool update_disabled);

  void SetEnableComponentUpdates(bool enable_component_updates);

  static component_updater::ComponentRegistration MakeComponentRegistration(
      bool supports_group_policy_enable_component_updates);

  TestCase cur_test_case_;
  base::RepeatingClosure quit_closure_;

  static const char component_id_[];

  static const bool kUpdateDisabled = true;

 private:
  void OnDemandComplete(update_client::Error error);

  std::unique_ptr<update_client::URLLoaderPostInterceptor> post_interceptor_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // This member is owned by g_browser_process;
  raw_ptr<component_updater::ComponentUpdateService> cus_ = nullptr;

  net::EmbeddedTestServer https_server_;
};

const char ComponentUpdaterPolicyTest::component_id_[] =
    "jebgalgnebhfojomionfpkfelancnnkf";

ComponentUpdaterPolicyTest::ComponentUpdaterPolicyTest()
    : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  scoped_feature_list_.InitAndDisableFeature(
      ash::features::kGrowthCampaignsInConsumerSession);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

ComponentUpdaterPolicyTest::~ComponentUpdaterPolicyTest() {}

void ComponentUpdaterPolicyTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Set up the mock server, for the network requests.
  ASSERT_TRUE(https_server_.InitializeAndListen());
  const std::string val = base::StringPrintf(
      "url-source=%s", https_server_.GetURL("/service/update2").spec().c_str());
  command_line->AppendSwitchASCII(switches::kComponentUpdater, val.c_str());
  PolicyTest::SetUpCommandLine(command_line);
}

void ComponentUpdaterPolicyTest::SetUpOnMainThread() {
  const auto config = component_updater::MakeChromeComponentUpdaterConfigurator(
      base::CommandLine::ForCurrentProcess(), g_browser_process->local_state());
  const auto urls = config->UpdateUrl();
  ASSERT_EQ(1u, urls.size());
  post_interceptor_ = std::make_unique<update_client::URLLoaderPostInterceptor>(
      urls, &https_server_);

  https_server_.StartAcceptingConnections();
  PolicyTest::SetUpOnMainThread();
}

void ComponentUpdaterPolicyTest::SetEnableComponentUpdates(
    bool enable_component_updates) {
  PolicyMap policies;
  policies.Set(key::kComponentUpdatesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
               base::Value(enable_component_updates), nullptr);
  UpdateProviderPolicy(policies);
}

component_updater::ComponentRegistration
ComponentUpdaterPolicyTest::MakeComponentRegistration(
    bool supports_group_policy_enable_component_updates) {
  class MockInstaller : public update_client::CrxInstaller {
   public:
    MockInstaller() {}

    void Install(const base::FilePath& unpack_path,
                 const std::string& public_key,
                 std::unique_ptr<InstallParams> /*install_params*/,
                 ProgressCallback /*progress_callback*/,
                 Callback callback) override {
      DoInstall(unpack_path, public_key, std::move(callback));
    }

    MOCK_METHOD1(OnUpdateError, void(int error));
    MOCK_METHOD3(DoInstall,
                 void(const base::FilePath& unpack_path,
                      const std::string& public_key,
                      const Callback& callback));
    MOCK_METHOD1(GetInstalledFile,
                 std::optional<base::FilePath>(const std::string& file));
    MOCK_METHOD0(Uninstall, bool());

   private:
    ~MockInstaller() override {}
  };

  // component id "jebgalgnebhfojomionfpkfelancnnkf".
  std::vector<uint8_t> jebg_hash = {
      0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec, 0x8e, 0xd5, 0xfa,
      0x54, 0xb0, 0xd2, 0xdd, 0xa5, 0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47,
      0xf6, 0xc4, 0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};

  // The component uses HTTPS only for network interception purposes.
  return component_updater::ComponentRegistration(
      "jebgalgnebhfojomionfpkfelancnnkf", {}, jebg_hash, base::Version("0.9"),
      /*fingerprint=*/{}, /*installer_attributes=*/{},
      /*action_handler=*/nullptr, base::MakeRefCounted<MockInstaller>(),
      /*requires_network_encryption=*/true,
      supports_group_policy_enable_component_updates,
      /*allow_cached_copies=*/true,
      /*allow_updates_on_metered_connection=*/true,
      /*allow_updates=*/true);
}

void ComponentUpdaterPolicyTest::UpdateComponent(
    const component_updater::ComponentRegistration& reg) {
  post_interceptor_->Reset();
  EXPECT_TRUE(post_interceptor_->ExpectRequest(
      std::make_unique<update_client::PartialMatch>("updatecheck")));
  EXPECT_TRUE(cus_->RegisterComponent(reg));
  cus_->GetOnDemandUpdater().OnDemandUpdate(
      component_id_, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      base::BindOnce(&ComponentUpdaterPolicyTest::OnDemandComplete,
                     base::Unretained(this)));
}

void ComponentUpdaterPolicyTest::CallAsync(TestCaseAction action) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(action, base::Unretained(this)));
}

void ComponentUpdaterPolicyTest::OnDemandComplete(update_client::Error error) {
  CallAsync(cur_test_case_.second);
}

void ComponentUpdaterPolicyTest::BeginTest() {
  cus_ = g_browser_process->component_updater();

  const auto config = component_updater::MakeChromeComponentUpdaterConfigurator(
      base::CommandLine::ForCurrentProcess(), g_browser_process->local_state());
  const auto urls = config->UpdateUrl();
  ASSERT_TRUE(urls.size());
  const GURL url = urls.front();

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicySupported);

  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EndTest() {
  post_interceptor_.reset();
  cus_ = nullptr;
  quit_closure_.Run();
}

void ComponentUpdaterPolicyTest::VerifyExpectations(bool update_disabled) {
  EXPECT_EQ(1, post_interceptor_->GetHitCount())
      << post_interceptor_->GetRequestsAsString();
  ASSERT_EQ(1, post_interceptor_->GetCount())
      << post_interceptor_->GetRequestsAsString();

  const auto& request = post_interceptor_->GetRequestBody(0);

  // Handle XML and JSON protocols.
  if (base::StartsWith(request, "<?xml", base::CompareCase::SENSITIVE)) {
    EXPECT_NE(std::string::npos,
              request.find(base::StringPrintf(
                  "<updatecheck%s/>",
                  update_disabled ? " updatedisabled=\"true\"" : "")));
  } else if (base::StartsWith(request, R"({"request":{)",
                              base::CompareCase::SENSITIVE)) {
    const auto root = base::JSONReader::Read(request);
    ASSERT_TRUE(root);
    const auto* update_check =
        (*root->GetDict().FindDict("request")->FindList("app"))[0]
            .GetDict()
            .FindDict("updatecheck");
    ASSERT_TRUE(update_check);
    if (update_disabled) {
      EXPECT_EQ(true, update_check->Find("updatedisabled")->GetBool());
    } else {
      EXPECT_FALSE(update_check->Find("updatedisabled"));
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

void ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicySupported() {
  UpdateComponent(MakeComponentRegistration(true));
}

void ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicySupported() {
  // Default policy && policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DefaultPolicy_GroupPolicyNotSupported() {
  UpdateComponent(MakeComponentRegistration(false));
}

void ComponentUpdaterPolicyTest::FinishDefaultPolicy_GroupPolicyNotSupported() {
  // Default policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicySupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicySupported() {
  SetEnableComponentUpdates(true);
  UpdateComponent(MakeComponentRegistration(true));
}

void ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicySupported() {
  // Updates enabled policy && policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::EnabledPolicy_GroupPolicyNotSupported() {
  SetEnableComponentUpdates(true);
  UpdateComponent(MakeComponentRegistration(false));
}

void ComponentUpdaterPolicyTest::FinishEnabledPolicy_GroupPolicyNotSupported() {
  // Updates enabled policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicySupported,
      &ComponentUpdaterPolicyTest::FinishDisabled_PolicyGroupPolicySupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicySupported() {
  SetEnableComponentUpdates(false);
  UpdateComponent(MakeComponentRegistration(true));
}

void ComponentUpdaterPolicyTest::FinishDisabled_PolicyGroupPolicySupported() {
  // Updates enabled policy && policy support -> updates are disabled.
  VerifyExpectations(kUpdateDisabled);

  cur_test_case_ = std::make_pair(
      &ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicyNotSupported,
      &ComponentUpdaterPolicyTest::
          FinishDisabledPolicy_GroupPolicyNotSupported);
  CallAsync(cur_test_case_.first);
}

void ComponentUpdaterPolicyTest::DisabledPolicy_GroupPolicyNotSupported() {
  SetEnableComponentUpdates(false);
  UpdateComponent(MakeComponentRegistration(false));
}

void ComponentUpdaterPolicyTest::
    FinishDisabledPolicy_GroupPolicyNotSupported() {
  // Updates enabled policy && no policy support -> updates are enabled.
  VerifyExpectations(!kUpdateDisabled);

  cur_test_case_ = TestCase();
  CallAsync(&ComponentUpdaterPolicyTest::EndTest);
}

IN_PROC_BROWSER_TEST_F(ComponentUpdaterPolicyTest, EnabledComponentUpdates) {
  BeginTest();
  base::RunLoop loop;
  quit_closure_ = loop.QuitWhenIdleClosure();
  loop.Run();
}

}  // namespace policy
