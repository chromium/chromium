// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/bind.h"
#include "base/version.h"
#include "chrome/browser/feedback/system_logs/log_sources/related_website_sets_source.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service.h"
#include "chrome/browser/first_party_sets/first_party_sets_policy_service_factory.h"
#include "chrome/browser/first_party_sets/scoped_mock_first_party_sets_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "net/first_party_sets/global_first_party_sets.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

namespace {

constexpr char kRelatedWebsiteSetsDisabled[] = "Disabled";

}  // namespace

class RelatedWebsiteSetsSourceTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    // Set the active profile.
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetString(::prefs::kProfileLastUsed,
                           profile()->GetBaseName().MaybeAsASCII());

    service_ = first_party_sets::FirstPartySetsPolicyServiceFactory::
        GetForBrowserContext(profile());
    ASSERT_NE(service_, nullptr);

    // We can't avoid eagerly initializing the service, due to
    // indirection/caching in the factory infrastructure. So we wait for the
    // initialization to complete, and then reset the instance so that we can
    // call InitForTesting and inject different configs.
    base::RunLoop run_loop;
    service_->WaitForFirstInitCompleteForTesting(run_loop.QuitClosure());
    run_loop.Run();
    service_->ResetForTesting();
  }

  void TearDown() override {
    CHECK(service_);
    // Even though we reassign this in SetUp, service may be persisted between
    // tests if the factory has already created a service for the testing
    // profile being used.
    service_->ResetForTesting();
    BrowserWithTestWindowTest::TearDown();
  }

  std::unique_ptr<SystemLogsResponse> GetRelatedWebsiteSetsSource(
      first_party_sets::FirstPartySetsPolicyService* service) {
    base::RunLoop run_loop;
    RelatedWebsiteSetsSource source(service);
    std::unique_ptr<SystemLogsResponse> response;
    source.Fetch(
        base::BindLambdaForTesting([&](std::unique_ptr<SystemLogsResponse> r) {
          response = std::move(r);
          run_loop.Quit();
        }));
    run_loop.Run();
    return response;
  }

  void SetRwsEnabledViaPref(bool enabled) {
    profile()->GetPrefs()->SetBoolean(
        prefs::kPrivacySandboxRelatedWebsiteSetsEnabled, enabled);
  }

  void SetGlobalSets(net::GlobalFirstPartySets global_sets) {
    first_party_sets_handler_.SetGlobalSets(std::move(global_sets));
  }

  void SetContextConfig(net::FirstPartySetsContextConfig config) {
    first_party_sets_handler_.SetContextConfig(std::move(config));
  }

  first_party_sets::FirstPartySetsPolicyService* service() { return service_; }

 private:
  first_party_sets::ScopedMockFirstPartySetsHandler first_party_sets_handler_;
  raw_ptr<first_party_sets::FirstPartySetsPolicyService, DanglingUntriaged>
      service_;
};

TEST_F(RelatedWebsiteSetsSourceTest, RWS_Disabled) {
  SetRwsEnabledViaPref(false);
  service()->InitForTesting();
  auto response = GetRelatedWebsiteSetsSource(service());
  EXPECT_EQ(kRelatedWebsiteSetsDisabled,
            response->at(RelatedWebsiteSetsSource::kSetsInfoField));
}

TEST_F(RelatedWebsiteSetsSourceTest, RWS_NotReady) {
  auto response = GetRelatedWebsiteSetsSource(service());
  EXPECT_EQ("", response->at(RelatedWebsiteSetsSource::kSetsInfoField));
}

TEST_F(RelatedWebsiteSetsSourceTest, RWS_Empty) {
  service()->InitForTesting();
  auto response = GetRelatedWebsiteSetsSource(service());
  EXPECT_EQ(base::Value::List().DebugString(),
            response->at(RelatedWebsiteSetsSource::kSetsInfoField));
}

TEST_F(RelatedWebsiteSetsSourceTest, RWS_OTRProfile) {
  first_party_sets::FirstPartySetsPolicyService* otr_service =
      first_party_sets::FirstPartySetsPolicyServiceFactory::
          GetForBrowserContext(
              profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));

  base::RunLoop run_loop;
  otr_service->WaitForFirstInitCompleteForTesting(run_loop.QuitClosure());
  run_loop.Run();
  otr_service->ResetForTesting();
  otr_service->InitForTesting();

  auto response = GetRelatedWebsiteSetsSource(otr_service);
  EXPECT_EQ(kRelatedWebsiteSetsDisabled,
            response->at(RelatedWebsiteSetsSource::kSetsInfoField));
}

TEST_F(RelatedWebsiteSetsSourceTest, RWS) {
  net::SchemefulSite primary1_site(GURL("https://primary1.test"));
  net::SchemefulSite primary1_cctld(GURL("https://primary1.com"));
  net::SchemefulSite associate_site(GURL("https://associate.test"));

  net::SchemefulSite primary2_site(GURL("https://primary2.test"));
  net::SchemefulSite service_site(GURL("https://service.test"));

  // Create Global Related Website Sets with the following set:
  // { primary: "https://primary1.test",
  // associatedSites: ["https://associate.test"}
  // and alias { "https://primary1.com": "https://primary1.test" }.
  SetGlobalSets(net::GlobalFirstPartySets(
      base::Version("0.0"),
      {{primary1_site,
        {net::FirstPartySetEntry(primary1_site, net::SiteType::kPrimary,
                                 std::nullopt)}},
       {associate_site,
        {net::FirstPartySetEntry(primary1_site, net::SiteType::kAssociated,
                                 0)}}},
      {{primary1_cctld, primary1_site}}));

  // The context config of the profile adds a new set:
  // { primary: "https://primary2.test",
  // serviceSites: ["https://service.test"}
  SetContextConfig(net::FirstPartySetsContextConfig(
      {{primary2_site,
        net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
            primary2_site, net::SiteType::kPrimary, std::nullopt))},
       {service_site,
        net::FirstPartySetEntryOverride(net::FirstPartySetEntry(
            primary2_site, net::SiteType::kService, std::nullopt))}}));

  service()->InitForTesting();
  base::Value::List expected =
      base::Value::List()  //
          .Append(
              base::Value::Dict()
                  .Set("AssociatedSites",
                       base::Value::List().Append(associate_site.Serialize()))
                  .Set("PrimarySites", base::Value::List()
                                           .Append(primary1_cctld.Serialize())
                                           .Append(primary1_site.Serialize())))
          .Append(
              base::Value::Dict()
                  .Set("PrimarySites",
                       base::Value::List().Append(primary2_site.Serialize()))
                  .Set("ServiceSites",
                       base::Value::List().Append(service_site.Serialize())));
  auto response = GetRelatedWebsiteSetsSource(service());
  EXPECT_EQ(expected.DebugString(),
            response->at(RelatedWebsiteSetsSource::kSetsInfoField));
}

TEST_F(RelatedWebsiteSetsSourceTest, SubsetsAreSorted) {
  const net::SchemefulSite primary(GURL("https://primary.test"));
  const net::SchemefulSite associated1(GURL("https://associated1.test"));
  const net::SchemefulSite associated2(GURL("https://associated2.test"));
  const net::SchemefulSite associated3(GURL("https://associated3.test"));

  const net::SchemefulSite service1(GURL("https://service1.test"));
  const net::SchemefulSite service2(GURL("https://service2.test"));
  const net::SchemefulSite service3(GURL("https://service3.test"));

  SetGlobalSets(net::GlobalFirstPartySets(
      base::Version("0.0"),
      {
          {primary,
           {net::FirstPartySetEntry(primary, net::SiteType::kPrimary,
                                    std::nullopt)}},
          {associated3,
           {net::FirstPartySetEntry(primary, net::SiteType::kAssociated, 2)}},
          {associated1,
           {net::FirstPartySetEntry(primary, net::SiteType::kAssociated, 0)}},
          {associated2,
           {net::FirstPartySetEntry(primary, net::SiteType::kAssociated, 1)}},
          {service2,
           {net::FirstPartySetEntry(primary, net::SiteType::kService,
                                    std::nullopt)}},
          {service1,
           {net::FirstPartySetEntry(primary, net::SiteType::kService,
                                    std::nullopt)}},
          {service3,
           {net::FirstPartySetEntry(primary, net::SiteType::kService,
                                    std::nullopt)}},
      },
      {}));

  service()->InitForTesting();

  EXPECT_EQ(
      GetRelatedWebsiteSetsSource(service())->at(
          RelatedWebsiteSetsSource::kSetsInfoField),
      base::Value::List()
          .Append(
              base::Value::Dict()
                  .Set("AssociatedSites", base::Value::List()
                                              .Append(associated1.Serialize())
                                              .Append(associated2.Serialize())
                                              .Append(associated3.Serialize()))
                  .Set("PrimarySites",
                       base::Value::List().Append(primary.Serialize()))
                  .Set("ServiceSites", base::Value::List()
                                           .Append(service1.Serialize())
                                           .Append(service2.Serialize())
                                           .Append(service3.Serialize())))
          .DebugString());
}

}  // namespace system_logs
